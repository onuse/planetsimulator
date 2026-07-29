#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/density_field.hpp"
#include "rendering/patch_tree.hpp"
#include "simulation/crust_grid.hpp"

namespace rendering {

// Builds patch geometry on worker threads.
//
// A patch costs around four hundred microseconds: twelve hundred terrain
// samples, each one a cell lookup and an interpolation. On the render thread
// that is the ceiling on everything. New patches arrive at whatever rate fits
// in the frame's spare time, so flying somewhere leaves the surface coarse
// until it catches up; and when tectonics moves the ground, the visible
// patches can only be brought up to date a handful at a time, so for an eighth
// of a second neighbours are at different steps and disagree about where the
// ground is along the edge they share.
//
// Building is pure - terrain in, vertices out - so it parallelises directly.
//
// The subtlety is what the terrain is. The render thread swaps to a newer
// simulation snapshot once a frame and the old one is freed when nothing holds
// it, so a worker reading through a bare pointer can have the ground pulled
// out from under it mid-patch. Each job therefore carries shared ownership of
// the snapshot it was started against, and its own copy of the density field
// bound to that snapshot. A build is then reproducible and isolated: two
// patches built against the same version agree along their shared edge no
// matter when either finished.
class PatchBuilder {
public:
    ~PatchBuilder();

    // Where finished geometry is written. Slots are handed out by the caller
    // and each is owned exclusively by one job, so workers can copy into the
    // mapped pool without coordinating.
    struct Pool {
        void* vertices = nullptr;
        void* indices = nullptr;
        uint32_t verticesPerPatch = 0;
        uint32_t indicesPerPatch = 0;
    };

    struct Job {
        PatchTree::PatchKey key;
        uint32_t slot = 0;
        uint64_t crustVersion = 0;
        uint32_t styleVersion = 0;
        bool isRefresh = false;
    };

    // What the render thread needs to file the result. The geometry itself is
    // already in the pool by the time this appears.
    struct Result {
        PatchTree::PatchKey key;
        uint32_t slot = 0;
        uint64_t crustVersion = 0;
        uint32_t styleVersion = 0;
        bool isRefresh = false;
        bool usable = false;
        glm::dvec3 centre{0.0};
        float boundingRadius = 0.0f;
        uint32_t indexCount = 0;
    };

    void start(const Pool& pool, unsigned threadCount = 0);
    void stop();

    // Point subsequent jobs at a newer surface. Jobs already running keep the
    // one they started with.
    void setSource(const core::DensityField& field,
                   std::shared_ptr<const simulation::CrustGrid::Snapshot> snapshot,
                   float planetRadius);

    void submit(const Job& job);

    // Take everything finished since the last call. Never blocks.
    void collect(std::vector<Result>& out);

    size_t queued() const;
    size_t workerCount() const { return workers.size(); }
    bool running() const { return workersRunning.load(); }

private:
    // Everything a build reads, versioned together so a job cannot see half of
    // one surface and half of the next.
    struct Source {
        core::DensityField field;
        std::shared_ptr<const simulation::CrustGrid::Snapshot> snapshot;
        float planetRadius = 0.0f;
    };

    void workerLoop();

    Pool pool;
    std::vector<std::thread> workers;
    std::atomic<bool> workersRunning{false};

    mutable std::mutex queueMutex;
    std::condition_variable queueSignal;
    std::deque<Job> pending;

    std::mutex doneMutex;
    std::vector<Result> done;

    mutable std::mutex sourceMutex;
    std::shared_ptr<const Source> source;
};

} // namespace rendering
