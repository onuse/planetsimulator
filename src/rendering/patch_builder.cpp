#include "rendering/patch_builder.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <cstring>

namespace rendering {

PatchBuilder::~PatchBuilder() {
    stop();
}

void PatchBuilder::start(const Pool& poolIn, unsigned threadCount) {
    if (workersRunning.load()) {
        return;
    }
    pool = poolIn;

    if (threadCount == 0) {
        // Leave the render thread and the simulation thread a core each.
        const unsigned cores = std::max(2u, std::thread::hardware_concurrency());
        threadCount = std::max(1u, cores > 3u ? cores - 2u : 1u);
    }

    workersRunning.store(true);
    workers.reserve(threadCount);
    for (unsigned i = 0; i < threadCount; i++) {
        workers.emplace_back([this] { workerLoop(); });
    }

    util::vlog() << "[patches] building on " << threadCount << " worker threads\n";
}

void PatchBuilder::stop() {
    if (!workersRunning.exchange(false)) {
        return;
    }
    queueSignal.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        pending.clear();
    }
    {
        std::lock_guard<std::mutex> lock(doneMutex);
        done.clear();
    }
}

void PatchBuilder::setSource(const core::DensityField& field,
                             std::shared_ptr<const simulation::CrustGrid::Snapshot> snapshot,
                             float planetRadius) {
    auto next = std::make_shared<Source>();
    next->field = field;                      // a value type; copying is cheap
    next->snapshot = std::move(snapshot);
    next->planetRadius = planetRadius;

    // Bound to this job's own snapshot rather than to whichever one the render
    // thread happens to be pointing at when the build runs.
    next->field.setCrustSnapshot(next->snapshot.get());

    std::lock_guard<std::mutex> lock(sourceMutex);
    source = std::move(next);
}

void PatchBuilder::submit(const Job& job) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        pending.push_back(job);
    }
    queueSignal.notify_one();
}

void PatchBuilder::collect(std::vector<Result>& out) {
    out.clear();
    std::lock_guard<std::mutex> lock(doneMutex);
    out.swap(done);
}

size_t PatchBuilder::queued() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return pending.size();
}

void PatchBuilder::workerLoop() {
    PatchTree::Patch patch;

    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueSignal.wait(lock, [this] {
                return !pending.empty() || !workersRunning.load();
            });
            if (!workersRunning.load()) {
                return;
            }
            job = pending.front();
            pending.pop_front();
        }

        std::shared_ptr<const Source> current;
        {
            std::lock_guard<std::mutex> lock(sourceMutex);
            current = source;
        }

        Result result;
        result.key = job.key;
        result.slot = job.slot;
        result.crustVersion = job.crustVersion;
        result.styleVersion = job.styleVersion;
        result.isRefresh = job.isRefresh;

        if (current) {
            patch = PatchTree::Patch{};
            patch.key = job.key;
            PatchTree::build(patch, current->field, current->planetRadius);

            // Slots are uniform because every patch is the same grid, which is
            // what lets them be written independently. If that ever stops
            // being true the geometry would land across a neighbour's slot,
            // so it is checked rather than assumed.
            if (patch.vertices.size() == pool.verticesPerPatch &&
                patch.indices.size() == pool.indicesPerPatch) {
                auto* vertexDst = static_cast<algorithms::MeshVertex*>(pool.vertices) +
                                  static_cast<size_t>(job.slot) * pool.verticesPerPatch;
                auto* indexDst = static_cast<uint32_t*>(pool.indices) +
                                 static_cast<size_t>(job.slot) * pool.indicesPerPatch;

                std::memcpy(vertexDst, patch.vertices.data(),
                            pool.verticesPerPatch * sizeof(algorithms::MeshVertex));
                std::memcpy(indexDst, patch.indices.data(),
                            pool.indicesPerPatch * sizeof(uint32_t));

                result.centre = patch.centre;
                result.boundingRadius = patch.boundingRadius;
                result.indexCount = pool.indicesPerPatch;
                result.usable = true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(doneMutex);
            done.push_back(result);
        }
    }
}

} // namespace rendering
