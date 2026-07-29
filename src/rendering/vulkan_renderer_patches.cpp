#include "rendering/vulkan_renderer.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

// Drawing the surface as a quadtree of patches.
//
// The old path built one mesh for the whole planet at a detail level picked
// from camera distance, and rebuilt all of it whenever that level changed.
// Here, each patch is built once and kept; moving the camera changes which
// patches are drawn, not what has to be built. The triangle count depends on
// how much surface is visible rather than on how close the camera is, so
// flying down to the ground costs no more than looking at the planet from
// orbit - it just spends the budget on a smaller area.
//
// Two rules decide how this feels to use.
//
// A patch is never taken away before its replacement exists. Tectonics moves
// the surface several times a second, and rebuilding everything each time - or
// worse, dropping everything and refilling over the following frames - makes
// the planet strobe.
//
// All patches share one pair of buffers. Every patch is the same fixed grid,
// so they are interchangeable slots, and a slot is an offset rather than an
// allocation. A buffer pair per patch meant nearly four thousand device
// allocations at a couple of thousand patches, which is up against the limit
// most drivers expose, plus a rebind per draw.

namespace rendering {

namespace {

constexpr VkDeviceSize kVertexStride = sizeof(algorithms::MeshVertex);
constexpr VkDeviceSize kIndexStride = sizeof(uint32_t);

// The six frustum planes, in the space the matrix maps from. Each is
// (normal.xyz, distance) with the normal pointing inwards, so a point is
// inside when dot(normal, p) + distance >= 0.
struct Frustum {
    glm::vec4 planes[6];

    bool containsSphere(const glm::vec3& centre, float radius) const {
        for (const glm::vec4& p : planes) {
            if (glm::dot(glm::vec3(p), centre) + p.w < -radius) {
                return false;
            }
        }
        return true;
    }
};

// Gribb-Hartmann: adding or subtracting a row of the view-projection from its
// w row gives a clip plane directly, no matrix inverse involved. The near
// plane is the z row alone rather than w+z because Vulkan's depth range is
// [0,1] where OpenGL's is [-1,1].
Frustum frustumFrom(const glm::mat4& m) {
    const glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    const glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    const glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    const glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

    Frustum f;
    f.planes[0] = row3 + row0;   // left
    f.planes[1] = row3 - row0;   // right
    f.planes[2] = row3 + row1;   // bottom
    f.planes[3] = row3 - row1;   // top
    f.planes[4] = row2;          // near
    f.planes[5] = row3 - row2;   // far

    // Normalised so plane distances are in metres and can be compared against
    // a bounding radius.
    for (glm::vec4& p : f.planes) {
        const float length = glm::length(glm::vec3(p));
        if (length > 0.0f) {
            p /= length;
        }
    }
    return f;
}

} // namespace

uint64_t VulkanRenderer::packPatchKey(const PatchTree::PatchKey& key) {
    return (static_cast<uint64_t>(key.face) << 60) |
           (static_cast<uint64_t>(key.level) << 56) |
           (static_cast<uint64_t>(key.x) << 28) |
           static_cast<uint64_t>(key.y);
}

void VulkanRenderer::createPatchPools() {
    if (patchVertexPool != VK_NULL_HANDLE) {
        return;
    }

    const VkDeviceSize vertexBytes =
        static_cast<VkDeviceSize>(MAX_PATCHES) * PATCH_VERTEX_COUNT * kVertexStride;
    const VkDeviceSize indexBytes =
        static_cast<VkDeviceSize>(MAX_PATCHES) * PATCH_INDEX_COUNT * kIndexStride;

    // Host-visible and permanently mapped. The GPU reads these across PCIe
    // rather than from its own memory, which costs bandwidth but avoids a
    // staging copy and a queue wait on the render thread for every patch
    // built. Worth revisiting if patch counts grow much further.
    createBuffer(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 patchVertexPool, patchVertexPoolMemory);
    createBuffer(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 patchIndexPool, patchIndexPoolMemory);

    vkMapMemory(device, patchVertexPoolMemory, 0, vertexBytes, 0, &patchVertexPoolMapped);
    vkMapMemory(device, patchIndexPoolMemory, 0, indexBytes, 0, &patchIndexPoolMapped);

    freePatchSlots.resize(MAX_PATCHES);
    for (uint32_t i = 0; i < MAX_PATCHES; i++) {
        freePatchSlots[i] = MAX_PATCHES - 1 - i;   // hand out low slots first
    }

    util::vlog() << "[patches] pool: " << MAX_PATCHES << " slots, "
                 << (vertexBytes + indexBytes) / (1024 * 1024) << " MB\n";

    // Workers write straight into the mapped pool. Slots are uniform and each
    // is owned by one job, so no two workers ever touch the same bytes and
    // none of this needs a lock.
    PatchBuilder::Pool builderPool;
    builderPool.vertices = patchVertexPoolMapped;
    builderPool.indices = patchIndexPoolMapped;
    builderPool.verticesPerPatch = PATCH_VERTEX_COUNT;
    builderPool.indicesPerPatch = PATCH_INDEX_COUNT;
    patchBuilder.start(builderPool);
}

void VulkanRenderer::releasePatchSlot(uint32_t slot) {
    pendingPatchSlots.push_back({slot, patchFrameCounter});
}

void VulkanRenderer::reclaimPendingSlots() {
    // Once no in-flight frame can still be drawing it, the slot is reusable.
    constexpr uint64_t FRAMES_TO_WAIT = MAX_FRAMES_IN_FLIGHT + 1;
    auto it = pendingPatchSlots.begin();
    while (it != pendingPatchSlots.end()) {
        if (patchFrameCounter - it->freedOnFrame >= FRAMES_TO_WAIT) {
            freePatchSlots.push_back(it->slot);
            it = pendingPatchSlots.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t VulkanRenderer::acquirePatchSlot() {
    if (freePatchSlots.empty()) {
        // Pool exhausted: give up whichever cached patch has gone longest
        // without being drawn. It goes through the pending list like any other
        // release, so it is not reused until the frames that referenced it
        // have finished.
        auto oldest = patchCache.end();
        for (auto it = patchCache.begin(); it != patchCache.end(); ++it) {
            if (patchFrameCounter - it->second.lastUsedFrame <= MAX_FRAMES_IN_FLIGHT) {
                continue;   // possibly still being drawn
            }
            if (oldest == patchCache.end() ||
                it->second.lastUsedFrame < oldest->second.lastUsedFrame) {
                oldest = it;
            }
        }
        if (oldest == patchCache.end()) {
            return UINT32_MAX;
        }
        releasePatchSlot(oldest->second.slot);
        patchCache.erase(oldest);
        return UINT32_MAX;   // available in a few frames, not now
    }

    const uint32_t slot = freePatchSlots.back();
    freePatchSlots.pop_back();
    return slot;
}

void VulkanRenderer::destroyAllPatches() {
    // Workers first: they write into the pool, so it cannot be unmapped while
    // any of them is still running.
    patchBuilder.stop();

    vkDeviceWaitIdle(device);
    inFlightPatches.clear();
    patchCache.clear();
    freePatchSlots.clear();
    pendingPatchSlots.clear();

    if (patchVertexPool != VK_NULL_HANDLE) {
        vkUnmapMemory(device, patchVertexPoolMemory);
        vkDestroyBuffer(device, patchVertexPool, nullptr);
        vkFreeMemory(device, patchVertexPoolMemory, nullptr);
        patchVertexPool = VK_NULL_HANDLE;
        patchVertexPoolMemory = VK_NULL_HANDLE;
        patchVertexPoolMapped = nullptr;
    }
    if (patchIndexPool != VK_NULL_HANDLE) {
        vkUnmapMemory(device, patchIndexPoolMemory);
        vkDestroyBuffer(device, patchIndexPool, nullptr);
        vkFreeMemory(device, patchIndexPoolMemory, nullptr);
        patchIndexPool = VK_NULL_HANDLE;
        patchIndexPoolMemory = VK_NULL_HANDLE;
        patchIndexPoolMapped = nullptr;
    }
}

void VulkanRenderer::evictUnusedPatches() {
    // Patches the camera has turned away from are worth keeping - turning back
    // should not mean rebuilding - so they are only dropped once the pool is
    // under real pressure, which acquirePatchSlot() handles. This is the
    // gentler pass: release anything not looked at for a long time.
    constexpr uint64_t GRACE_FRAMES = 1800;

    if (freePatchSlots.size() > MAX_PATCHES / 4) {
        return;
    }
    for (auto it = patchCache.begin(); it != patchCache.end();) {
        if (patchFrameCounter - it->second.lastUsedFrame > GRACE_FRAMES) {
            releasePatchSlot(it->second.slot);
            it = patchCache.erase(it);
        } else {
            ++it;
        }
    }
}

void VulkanRenderer::updatePatches(octree::OctreePlanet* planet, core::Camera* camera) {
    if (!planet || !camera) {
        return;
    }

    createPatchPools();
    patchFrameCounter++;
    reclaimPendingSlots();

    const float planetRadius = planet->getRadius();
    const glm::dvec3 cameraPosition(camera->getPosition());
    const uint64_t crustVersion = planet->getCrustVersion();
    patchCullPlanetRadius = planetRadius;

    if (meshRebuildRequested) {
        meshRebuildRequested = false;
        patchStyleVersion++;    // colouring changed, so every patch is stale
    }

    // Selection draws only from what exists, and reports what it would need in
    // order to go finer. Nothing here can produce a hole or a level mismatch:
    // the worst case is that the surface stays coarse for a few more frames.
    const auto isReady = [this](const PatchTree::PatchKey& key) {
        auto it = patchCache.find(packPatchKey(key));
        return it != patchCache.end() && it->second.indexCount > 0;
    };
    patchTree.select(cameraPosition, planetRadius, isReady, visiblePatches, wantedPatches);

    // Work goes to the builder; nothing is built on this thread.
    //
    // Keep the workers pointed at the newest surface. Jobs already running
    // keep the one they started against, so a patch is always internally
    // consistent even if it lands a version late - and lands stale, which the
    // refresh pass then picks up like any other staleness.
    if (crustVersion != sourceCrustVersion) {
        sourceCrustVersion = crustVersion;
        patchBuilder.setSource(planet->getDensityField(), planet->getRenderSnapshot(),
                               planetRadius);
    }

    // File whatever finished since last frame. The geometry is already in the
    // pool by now; all that is left is deciding whether it is still wanted.
    patchBuilder.collect(builtPatches);
    for (const PatchBuilder::Result& result : builtPatches) {
        const uint64_t packed = packPatchKey(result.key);
        inFlightPatches.erase(packed);

        if (!result.usable) {
            releasePatchSlot(result.slot);
            continue;
        }

        auto it = patchCache.find(packed);
        if (it == patchCache.end()) {
            GpuPatch gpu;
            gpu.slot = result.slot;
            gpu.indexCount = result.indexCount;
            gpu.centre = result.centre;
            gpu.boundingRadius = result.boundingRadius;
            gpu.lastUsedFrame = patchFrameCounter;
            gpu.builtAtCrustVersion = result.crustVersion;
            gpu.builtAtStyle = result.styleVersion;
            patchCache.emplace(packed, gpu);
            continue;
        }

        // A refresh that finished after something newer already replaced this
        // patch is thrown away rather than allowed to move it backwards.
        if (result.crustVersion < it->second.builtAtCrustVersion) {
            releasePatchSlot(result.slot);
            continue;
        }

        releasePatchSlot(it->second.slot);   // retired, not freed: still in flight
        it->second.slot = result.slot;
        it->second.indexCount = result.indexCount;
        it->second.centre = result.centre;
        it->second.boundingRadius = result.boundingRadius;
        it->second.builtAtCrustVersion = result.crustVersion;
        it->second.builtAtStyle = result.styleVersion;
    }

    // How much to keep in flight.
    //
    // Enough that no worker idles, and not so much that the queue outlives the
    // decision that filled it - the camera moves, and a patch nobody wants any
    // more still costs a slot and a build. A couple of jobs per worker is the
    // balance.
    const size_t workerDepth = std::max<size_t>(16, patchBuilder.workerCount() * 3);
    const auto hasRoom = [&] { return inFlightPatches.size() < workerDepth; };

    const auto request = [&](const PatchTree::PatchKey& key, bool isRefresh,
                             uint32_t slot) {
        PatchBuilder::Job job;
        job.key = key;
        job.slot = slot;
        job.crustVersion = crustVersion;
        job.styleVersion = patchStyleVersion;
        job.isRefresh = isRefresh;
        inFlightPatches.insert(packPatchKey(key));
        patchBuilder.submit(job);
    };

    // Missing patches first: those are the ones holding the surface coarse.
    for (const PatchTree::PatchKey& key : wantedPatches) {
        if (!hasRoom()) {
            break;
        }
        const uint64_t packed = packPatchKey(key);
        if (patchCache.count(packed) || inFlightPatches.count(packed)) {
            continue;
        }
        const uint32_t slot = acquirePatchSlot();
        if (slot == UINT32_MAX) {
            break;      // pool under pressure; slots free up in a few frames
        }
        request(key, false, slot);
    }

    // Oldest first, not first-come.
    //
    // Staleness is not uniform. A patch that has been on screen throughout is
    // one step behind; one the camera turned away from and came back to can be
    // many, and a continent will have moved in between - so it draws ocean
    // where its neighbours draw land, standing proud of them with its side
    // wall showing. Those are the ones worth the budget, and refreshing in
    // whatever order selection happened to emit made them queue behind patches
    // that were nearly right already.
    staleVisible.clear();
    for (const PatchTree::PatchKey& key : visiblePatches) {
        auto it = patchCache.find(packPatchKey(key));
        if (it == patchCache.end()) {
            continue;
        }
        it->second.lastUsedFrame = patchFrameCounter;

        if (it->second.builtAtCrustVersion != crustVersion ||
            it->second.builtAtStyle != patchStyleVersion) {
            staleVisible.push_back(key);
        }
    }
    std::sort(staleVisible.begin(), staleVisible.end(),
              [this](const PatchTree::PatchKey& a, const PatchTree::PatchKey& b) {
                  return patchCache[packPatchKey(a)].builtAtCrustVersion <
                         patchCache[packPatchKey(b)].builtAtCrustVersion;
              });

    for (const PatchTree::PatchKey& key : staleVisible) {
        if (!hasRoom()) {
            break;
        }
        const uint64_t packed = packPatchKey(key);
        if (inFlightPatches.count(packed)) {
            continue;
        }

        // Into a fresh slot, never over the one being drawn from. A frame in
        // flight is reading those vertices; overwriting them gives it some of
        // the old version and some of the new, and a triangle with one corner
        // at each stretches between them into a long thin spike. A tectonic
        // step moves plates a hundred kilometres, so the two are nowhere near
        // each other. The old slot keeps its contents until the frames
        // referencing it retire.
        const uint32_t replacementSlot = acquirePatchSlot();
        if (replacementSlot == UINT32_MAX) {
            break;
        }
        request(key, true, replacementSlot);
    }


    evictUnusedPatches();

    // Recorded rather than inferred: everything here is a count of something
    // that happened this frame.
    patchStats.selected = static_cast<uint32_t>(visiblePatches.size());
    patchStats.cached = static_cast<uint32_t>(patchCache.size());
    patchStats.inFlight = static_cast<uint32_t>(inFlightPatches.size());
    patchStats.poolSlots = MAX_PATCHES;
    patchStats.workers = static_cast<uint32_t>(patchBuilder.workerCount());

    uint32_t finest = 0;
    for (const PatchTree::PatchKey& key : visiblePatches) {
        finest = std::max(finest, static_cast<uint32_t>(key.level));
    }
    patchStats.finestLevel = finest;

    // How far apart the vertices are at the sharpest patch on screen. This is
    // the number that says what the surface can actually resolve, which is
    // what a level on its own never told anyone.
    PatchTree::PatchKey finestKey;
    finestKey.level = static_cast<uint8_t>(finest);
    patchStats.metresPerVertex =
        static_cast<float>(PatchTree::patchWorldSize(finestKey, planetRadius) /
                           PatchTree::GRID);
}

void VulkanRenderer::renderPatches(const glm::dvec3& cameraPosition) {
    if (!currentCommandBuffer || trianglePipeline == VK_NULL_HANDLE ||
        hierarchicalDescriptorSets.empty() || patchVertexPool == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
    vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            hierarchicalPipelineLayout, 0, 1,
                            &hierarchicalDescriptorSets[currentFrame], 0, nullptr);

    // Bound once for every patch in the frame.
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, &patchVertexPool, offsets);
    vkCmdBindIndexBuffer(currentCommandBuffer, patchIndexPool, 0, VK_INDEX_TYPE_UINT32);

    const Frustum frustum = frustumFrom(patchCullMatrix);

    // Anything below the horizon is behind the planet's own bulk. On a sphere
    // that is a little over half the surface at any altitude, and none of it
    // survives the depth test, so it is the cheaper half of the two tests.
    //
    // A point X on the surface is visible from C only when dot(X, C) >= R^2.
    // Allowing for the patch's bounding sphere, the furthest that quantity can
    // reach is dot(P, C) + r|C|, so that is what has to clear R^2. R is shaded
    // slightly under sea level to keep the test conservative near the limb.
    const double cameraDistance = glm::length(cameraPosition);
    const double occluderRadius = patchCullPlanetRadius * 0.985;
    const bool horizonCulling = patchCullPlanetRadius > 0.0f &&
                                cameraDistance > occluderRadius;
    const double horizonThreshold = occluderRadius * occluderRadius;

    uint32_t drawn = 0;
    uint32_t culledByHorizon = 0;
    uint32_t culledByFrustum = 0;

    for (const PatchTree::PatchKey& key : visiblePatches) {
        auto it = patchCache.find(packPatchKey(key));
        if (it == patchCache.end() || it->second.indexCount == 0) {
            continue;   // not built yet
        }
        const GpuPatch& gpu = it->second;

        // A patch's geometry has to belong to the key it is filed under. If a
        // slot is ever written by one patch while another is drawing from it,
        // or a cache entry outlives the geometry it points at, the result is
        // ground drawn somewhere it does not belong - which is what detached
        // fragments of surface look like. The key implies exactly where its
        // centre should be, so the two can be compared.
        const glm::dvec3 expectedCentre =
            PatchTree::patchCentre(key, patchCullPlanetRadius);
        const double allowed =
            PatchTree::patchWorldSize(key, patchCullPlanetRadius) * 0.5 + 30000.0;
        if (glm::length(gpu.centre - expectedCentre) > allowed) {
            static int reported = 0;
            if (reported++ < 20) {
                std::cerr << "[patches] MISPLACED patch face=" << int(key.face)
                          << " level=" << int(key.level) << " x=" << key.x
                          << " y=" << key.y << " slot=" << gpu.slot << " off by "
                          << glm::length(gpu.centre - expectedCentre) << " m\n";
            }
            continue;
        }

        if (horizonCulling &&
            glm::dot(gpu.centre, cameraPosition) + gpu.boundingRadius * cameraDistance <
                horizonThreshold) {
            culledByHorizon++;
            continue;
        }

        // The one place the planet's absolute scale is handled, and it is done
        // in double before anything reaches the shader.
        PatchPushConstants push;
        push.patchOffset = glm::vec3(gpu.centre - cameraPosition);

        if (!frustum.containsSphere(push.patchOffset, gpu.boundingRadius)) {
            culledByFrustum++;
            continue;
        }

        vkCmdPushConstants(currentCommandBuffer, hierarchicalPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PatchPushConstants), &push);

        vkCmdDrawIndexed(currentCommandBuffer, gpu.indexCount, 1,
                         gpu.slot * PATCH_INDEX_COUNT,
                         static_cast<int32_t>(gpu.slot * PATCH_VERTEX_COUNT), 0);
        drawn++;
    }

    meshIndexCount = drawn * PATCH_INDEX_COUNT;   // so the stats overlay stays honest
    patchStats.drawn = drawn;
    patchStats.culledHorizon = culledByHorizon;
    patchStats.culledFrustum = culledByFrustum;
    patchStats.triangles = drawn * (PATCH_INDEX_COUNT / 3);

    static uint64_t lastReport = 0;
    if (patchFrameCounter - lastReport > 600) {
        lastReport = patchFrameCounter;
        util::vlog() << "[patches] " << drawn << " drawn of " << visiblePatches.size()
                     << " selected (" << culledByHorizon << " over horizon, "
                     << culledByFrustum << " off screen), " << patchCache.size()
                     << " cached, " << (drawn * PATCH_INDEX_COUNT / 3) << " triangles\n";
    }
}

} // namespace rendering
