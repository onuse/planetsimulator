#include "utils/parallel.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace util {

namespace {

class Pool {
public:
    Pool() {
        // One thread per core, less two: the render thread and the simulation
        // thread are both doing something while this runs, and oversubscribing
        // makes a parallel loop finish later rather than sooner because the
        // slowest range decides when everyone is done.
        const unsigned cores = std::max(2u, std::thread::hardware_concurrency());
        const unsigned wanted = cores > 3u ? cores - 2u : 1u;

        workers.reserve(wanted);
        for (unsigned i = 0; i < wanted; i++) {
            workers.emplace_back([this] { work(); });
        }
    }

    ~Pool() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        wake.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    size_t size() const { return workers.size(); }

    void run(size_t count, const std::function<void(size_t, size_t)>& body) {
        if (count == 0) {
            return;
        }

        // Small loops are not worth waking anyone for. The wake-and-wait costs
        // a few microseconds, which is more than a few thousand parcels take.
        const size_t threads = workers.size();
        if (threads == 0 || count < 4096) {
            body(0, count);
            return;
        }

        // One chunk per worker, and the calling thread takes one too rather
        // than sitting idle while it waits.
        const size_t chunks = threads + 1;
        const size_t chunkSize = (count + chunks - 1) / chunks;

        {
            std::lock_guard<std::mutex> lock(mutex);
            job = &body;
            jobCount = count;
            jobChunk = chunkSize;
            nextChunk = 1;                 // chunk 0 belongs to the caller
            outstanding = chunks - 1;
            generation++;
        }
        wake.notify_all();

        // The caller's share.
        body(0, std::min(chunkSize, count));

        std::unique_lock<std::mutex> lock(mutex);
        done.wait(lock, [this] { return outstanding == 0; });
        job = nullptr;
    }

private:
    void work() {
        uint64_t seen = 0;
        std::unique_lock<std::mutex> lock(mutex);
        while (true) {
            wake.wait(lock, [&] { return stopping || (job != nullptr && generation != seen); });
            if (stopping) {
                return;
            }
            seen = generation;

            const std::function<void(size_t, size_t)>* body = job;
            const size_t count = jobCount;
            const size_t chunk = jobChunk;

            while (true) {
                const size_t index = nextChunk++;
                const size_t begin = index * chunk;
                if (begin >= count) {
                    break;
                }
                const size_t end = std::min(begin + chunk, count);

                lock.unlock();
                (*body)(begin, end);
                lock.lock();
            }

            if (outstanding > 0 && --outstanding == 0) {
                done.notify_one();
            }
        }
    }

    std::vector<std::thread> workers;

    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable done;

    bool stopping = false;
    const std::function<void(size_t, size_t)>* job = nullptr;
    size_t jobCount = 0;
    size_t jobChunk = 0;
    size_t nextChunk = 0;
    size_t outstanding = 0;
    uint64_t generation = 0;
};

Pool& pool() {
    static Pool instance;
    return instance;
}

} // namespace

void parallelFor(size_t count, const std::function<void(size_t, size_t)>& body) {
    pool().run(count, body);
}

size_t workerCount() {
    return pool().size();
}

} // namespace util
