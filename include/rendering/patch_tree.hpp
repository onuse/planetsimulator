#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "algorithms/mesh_generation.hpp"

namespace core { class DensityField; }

namespace rendering {

// Surface geometry as a quadtree of patches over a cube mapped to the sphere.
//
// The mesh this replaces was one subdivided icosahedron covering the whole
// planet at a single detail level chosen by camera distance. That has a hard
// ceiling: to see ten metre features you need ten metre triangles everywhere,
// including on the far side of the planet, which is around 10^11 of them. It
// also had to be rebuilt in full whenever the level changed, which is what
// made zooming stutter.
//
// A quadtree spends triangles where the camera is looking. Each patch is a
// fixed grid, so the cost per patch is constant and the total is bounded by
// how many patches are visible, not by how close the camera is. Patches are
// built once and kept.
//
// Cube-sphere rather than a subdivided icosahedron because quadtree
// subdivision of a square is trivial and neighbours are easy to find; the
// seams between cube faces are handled by skirts.
class PatchTree {
public:
    // Vertices along one edge of a patch. 33 gives 32x32 quads, 2048
    // triangles, which is a reasonable draw call.
    static constexpr int GRID = 32;
    static constexpr int VERTS = GRID + 1;

    // Deepest subdivision. On a 1000 km planet level 14 puts about three
    // metres between vertices.
    static constexpr int MAX_LEVEL = 14;

    struct PatchKey {
        uint8_t face = 0;    // which of the six cube faces
        uint8_t level = 0;   // 0 is the whole face
        uint32_t x = 0;      // position within the face at this level
        uint32_t y = 0;

        bool operator==(const PatchKey& o) const {
            return face == o.face && level == o.level && x == o.x && y == o.y;
        }
    };

    // A built piece of surface, ready to draw.
    struct Patch {
        PatchKey key;

        // Patch centre in world space, held in double. Vertex positions are
        // stored relative to it: a float metre offset from a patch a few
        // kilometres across keeps millimetre precision, where absolute
        // positions on a planet lose everything below a few metres.
        glm::dvec3 centre{0.0};
        float boundingRadius = 0.0f;

        // How far the edge skirt hangs below the surface. Recorded because it
        // is what has to exceed the mismatch between this patch and a
        // neighbour at a different level, and that is worth being able to
        // check rather than assume.
        float skirtDepth = 0.0f;

        std::vector<algorithms::MeshVertex> vertices;
        std::vector<uint32_t> indices;

        bool built = false;
    };

    PatchTree() = default;

    // Decide which patches should be drawn from here, splitting where the
    // surface is close enough to the camera to warrant it.
    //
    // Selection only descends into a group of four children when all four are
    // already built, which is what `isReady` reports. Anything it would like
    // to descend into but cannot yet goes into `wanted` for the caller to
    // build over the next few frames.
    //
    // This coupling is deliberate. Choosing purely on geometry means the
    // moment the camera moves, selection stops asking for the patch it has and
    // starts asking for four it does not, so the surface is drawn from
    // whatever fraction of the new level happens to exist - holes, and
    // neighbouring ground at two different levels of detail sitting on top of
    // each other. Refusing to split until the replacement is complete makes
    // detail arrive a few frames late instead, which is not noticeable.
    void select(const glm::dvec3& cameraPosition, float planetRadius,
                const std::function<bool(const PatchKey&)>& isReady,
                std::vector<PatchKey>& visible,
                std::vector<PatchKey>& wanted) const;

    // Build one patch's geometry by sampling the terrain field.
    static void build(Patch& patch, const core::DensityField& field, float planetRadius);

    // Where a patch sits and how big it is, without building it.
    static glm::dvec3 patchCentre(const PatchKey& key, float planetRadius);
    static double patchWorldSize(const PatchKey& key, float planetRadius);

    // Direction on the unit sphere for a point inside a patch, u and v in
    // [0, 1] across the patch.
    static glm::dvec3 patchDirection(const PatchKey& key, double u, double v);

    // How aggressively to subdivide. A patch splits when its width exceeds
    // this fraction of its distance to the camera - an angular size test, and
    // the standard chunked-LOD criterion.
    //
    // Read it as radians of view a patch may cover: at 0.3 a patch spans about
    // 17 degrees, so with a 75 degree field of view roughly four span the
    // screen in each direction and a 32x32 patch lands near 300 pixels across.
    // That is the number that decides how sharp the horizon looks.
    float splitFactor = 0.3f;

private:
    void selectRecursive(const PatchKey& key, const glm::dvec3& cameraPosition,
                         float planetRadius,
                         const std::function<bool(const PatchKey&)>& isReady,
                         std::vector<PatchKey>& visible,
                         std::vector<PatchKey>& wanted) const;
};

} // namespace rendering
