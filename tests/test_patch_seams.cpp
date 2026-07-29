// Seams between surface patches.
//
// Neighbouring patches are not always at the same level of detail, and when
// they are not they do not share edge vertices: the finer one samples the
// shared edge at twice the spacing, so its edge follows the ground while the
// coarser one cuts the corner. Wherever they part company there is a hole
// straight through the planet.
//
// A skirt hangs a short wall inward from every patch edge to cover that. The
// only thing that makes it work is being deeper than the mismatch it has to
// hide, which is measurable: build a patch and its neighbour at one level
// finer, walk the shared edge, and compare the gap to the skirt.
//
// Sizing the skirt from the patch's own width passes casual inspection and
// fails this, because patches shrink with level while terrain relief does not.

#include "core/density_field.hpp"
#include "rendering/patch_tree.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::printf("  FAIL: %s\n", what.c_str());
        failures++;
    } else {
        std::printf("  ok:   %s\n", what.c_str());
    }
}

constexpr float PLANET_RADIUS = 1000000.0f;

using rendering::PatchTree;

// Height of the patch surface along one edge, at a parameter running across
// the patch. The edge is walked in the patch's own parameterisation so the two
// levels can be compared at the same places on the sphere.
double heightAlongEdge(const PatchTree::PatchKey& key, const core::DensityField& field,
                       double t, bool alongX) {
    const glm::dvec3 dir = alongX ? PatchTree::patchDirection(key, t, 0.0)
                                  : PatchTree::patchDirection(key, 0.0, t);
    const float h = field.getTerrainHeight(glm::vec3(dir));
    return std::max(h, field.getSeaLevelHeight());
}

// The worst distance between a fine patch's edge and the straight line a
// coarse neighbour draws between the same two points - which is exactly what
// the skirt has to cover.
double worstSeamGap(const PatchTree::PatchKey& fine, const core::DensityField& field,
                    bool alongX) {
    double worst = 0.0;

    // Every span between two of the coarse patch's vertices holds one extra
    // vertex of the fine patch, and that is where they differ most.
    for (int segment = 0; segment < PatchTree::GRID; segment++) {
        const double t0 = static_cast<double>(segment) / PatchTree::GRID;
        const double t1 = static_cast<double>(segment + 1) / PatchTree::GRID;

        const double h0 = heightAlongEdge(fine, field, t0, alongX);
        const double h1 = heightAlongEdge(fine, field, t1, alongX);

        for (int k = 1; k < 4; k++) {
            const double t = t0 + (t1 - t0) * (k / 4.0);
            const double actual = heightAlongEdge(fine, field, t, alongX);
            const double straight = h0 + (h1 - h0) * (k / 4.0);
            worst = std::max(worst, std::abs(actual - straight));
        }
    }
    return worst;
}

// The roughest patch at a level, found by looking.
//
// Testing an arbitrary patch is close to worthless here: most of the planet is
// ocean, which renders flat at sea level and has no seam to hide at any level
// of detail. The skirt has to cover the worst case, so the worst case is what
// has to be measured, and it has to be searched for.
PatchTree::PatchKey roughestPatch(int level, const core::DensityField& field,
                                  double& outRelief) {
    PatchTree::PatchKey best;
    best.level = static_cast<uint8_t>(level);
    outRelief = -1.0;

    const uint32_t span = 1u << level;
    const uint32_t stride = std::max(1u, span / 12u);

    for (uint8_t face = 0; face < 6; face++) {
        for (uint32_t y = 0; y < span; y += stride) {
            for (uint32_t x = 0; x < span; x += stride) {
                PatchTree::PatchKey key;
                key.face = face;
                key.level = static_cast<uint8_t>(level);
                key.x = x;
                key.y = y;

                double low = 1e30;
                double high = -1e30;
                for (int j = 0; j <= 4; j++) {
                    for (int i = 0; i <= 4; i++) {
                        const glm::dvec3 dir =
                            PatchTree::patchDirection(key, i / 4.0, j / 4.0);
                        const float h = field.getTerrainHeight(glm::vec3(dir));
                        const double surface = std::max(h, field.getSeaLevelHeight());
                        low = std::min(low, surface);
                        high = std::max(high, surface);
                    }
                }

                if (high - low > outRelief) {
                    outRelief = high - low;
                    best = key;
                }
            }
        }
    }
    return best;
}

void testSkirtCoversTheSeam() {
    std::printf("Skirts are deeper than the seams they hide\n");

    core::DensityField field(PLANET_RADIUS, 1337);

    // Across the whole range of levels, because the failure was specific to
    // deep ones: at level 2 the old rule gave a skirt kilometres deep and at
    // level 14 a few metres, while the terrain it had to cover barely changed.
    const int levels[] = {2, 5, 8, 11, 14};

    for (int level : levels) {
        double relief = 0.0;
        const PatchTree::PatchKey key = roughestPatch(level, field, relief);
        check(relief > 1.0, "level " + std::to_string(level) +
                                ": found terrain with relief to test (" +
                                std::to_string(static_cast<int>(relief)) + " m)");

        PatchTree::Patch patch;
        patch.key = key;
        PatchTree::build(patch, field, PLANET_RADIUS);

        const double gap = std::max(worstSeamGap(key, field, true),
                                    worstSeamGap(key, field, false));

        std::printf("    level %2d: patch %8.0f m across, seam %7.2f m, skirt %8.2f m\n",
                    level, PatchTree::patchWorldSize(key, PLANET_RADIUS), gap,
                    patch.skirtDepth);

        check(patch.skirtDepth > gap,
              "level " + std::to_string(level) + ": skirt covers the worst seam");
    }
}

void testSkirtSurvivesTwoLevels() {
    std::printf("Skirts still cover when neighbours differ by two levels\n");

    // Selection does not enforce a one-level difference between neighbours, so
    // a patch can sit next to one four times coarser. The gap grows; the skirt
    // has to have been sized with enough margin to absorb it.
    core::DensityField field(PLANET_RADIUS, 4242);

    for (int level : {6, 10, 13}) {
        double relief = 0.0;
        const PatchTree::PatchKey key = roughestPatch(level, field, relief);

        PatchTree::Patch patch;
        patch.key = key;
        PatchTree::build(patch, field, PLANET_RADIUS);

        // A neighbour two levels coarser draws one straight line across four
        // of this patch's spans.
        double worst = 0.0;
        for (int segment = 0; segment + 4 <= PatchTree::GRID; segment += 4) {
            const double t0 = static_cast<double>(segment) / PatchTree::GRID;
            const double t1 = static_cast<double>(segment + 4) / PatchTree::GRID;
            const double h0 = heightAlongEdge(key, field, t0, true);
            const double h1 = heightAlongEdge(key, field, t1, true);

            for (int k = 1; k < 4; k++) {
                const double t = t0 + (t1 - t0) * (k / 4.0);
                const double actual = heightAlongEdge(key, field, t, true);
                const double straight = h0 + (h1 - h0) * (k / 4.0);
                worst = std::max(worst, std::abs(actual - straight));
            }
        }

        std::printf("    level %2d: two-level seam %7.2f m, skirt %8.2f m\n",
                    level, worst, patch.skirtDepth);
        check(patch.skirtDepth > worst,
              "level " + std::to_string(level) + ": skirt covers a two-level seam");
    }
}

void testPatchesAreUniform() {
    std::printf("Every patch is the same size\n");

    // The renderer packs patches into fixed slots of one shared buffer, which
    // only works because every patch has the same vertex and index count.
    // Nothing else would notice if that stopped being true; the geometry would
    // just be written into the wrong place.
    core::DensityField field(PLANET_RADIUS, 99);

    const size_t expectedVertices =
        PatchTree::VERTS * PatchTree::VERTS + 4 * PatchTree::VERTS;
    const size_t expectedIndices =
        PatchTree::GRID * PatchTree::GRID * 6 + 4 * PatchTree::GRID * 12;

    bool uniform = true;
    for (uint8_t face = 0; face < 6; face++) {
        for (int level : {0, 3, 9, 14}) {
            PatchTree::PatchKey key;
            key.face = face;
            key.level = static_cast<uint8_t>(level);
            key.x = (1u << level) / 2;
            key.y = (1u << level) / 4;

            PatchTree::Patch patch;
            patch.key = key;
            PatchTree::build(patch, field, PLANET_RADIUS);

            if (patch.vertices.size() != expectedVertices ||
                patch.indices.size() != expectedIndices) {
                uniform = false;
            }
        }
    }

    check(uniform, "vertex and index counts match what the buffer pool assumes");
}

} // namespace

int main() {
    std::printf("=== Patch seams ===\n\n");

    testSkirtCoversTheSeam();
    std::printf("\n");
    testSkirtSurvivesTwoLevels();
    std::printf("\n");
    testPatchesAreUniform();

    std::printf("\n%s\n", failures == 0 ? "All passed" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
