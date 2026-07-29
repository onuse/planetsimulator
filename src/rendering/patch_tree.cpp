#include "rendering/patch_tree.hpp"
#include "core/density_field.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rendering {

namespace {

// The six cube faces, as an origin corner and two edge vectors spanning
// [-1, 1] in each direction.
struct FaceBasis {
    glm::dvec3 normal, right, up;
};

const FaceBasis FACES[6] = {
    {{ 1,  0,  0}, { 0,  0, -1}, { 0,  1,  0}},   // +X
    {{-1,  0,  0}, { 0,  0,  1}, { 0,  1,  0}},   // -X
    {{ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1}},   // +Y
    {{ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1}},   // -Y
    {{ 0,  0,  1}, { 1,  0,  0}, { 0,  1,  0}},   // +Z
    {{ 0,  0, -1}, {-1,  0,  0}, { 0,  1,  0}},   // -Z
};

// Mapping a cube face straight onto the sphere by normalising bunches the
// vertices towards the face corners - patches near a corner end up covering
// far less area than those at the centre. Warping the face coordinate first
// evens that out, so a quadtree level means roughly the same ground size
// wherever you are.
inline double warp(double t) {
    return std::tan(t * 0.7853981633974483);   // tan(t * pi/4)
}

} // namespace

glm::dvec3 PatchTree::patchDirection(const PatchKey& key, double u, double v) {
    const double span = 2.0 / static_cast<double>(1u << key.level);
    const double faceU = -1.0 + (key.x + u) * span;
    const double faceV = -1.0 + (key.y + v) * span;

    const FaceBasis& f = FACES[key.face];
    const glm::dvec3 point = f.normal + f.right * warp(faceU) + f.up * warp(faceV);
    return glm::normalize(point);
}

glm::dvec3 PatchTree::patchCentre(const PatchKey& key, float planetRadius) {
    return patchDirection(key, 0.5, 0.5) * static_cast<double>(planetRadius);
}

double PatchTree::patchWorldSize(const PatchKey& key, float planetRadius) {
    // Angular width of the patch times the radius. Taking two opposite
    // corners is close enough and accounts for the warp.
    const glm::dvec3 a = patchDirection(key, 0.0, 0.0);
    const glm::dvec3 b = patchDirection(key, 1.0, 1.0);
    const double angle = std::acos(glm::clamp(glm::dot(a, b), -1.0, 1.0));
    return angle * static_cast<double>(planetRadius);
}

void PatchTree::selectRecursive(const PatchKey& key, const glm::dvec3& cameraPosition,
                                float planetRadius,
                                const std::function<bool(const PatchKey&)>& isReady,
                                std::vector<PatchKey>& visible,
                                std::vector<PatchKey>& wanted) const {
    const glm::dvec3 centre = patchCentre(key, planetRadius);
    const double size = patchWorldSize(key, planetRadius);

    // Distance to the patch, not to its centre: a patch the camera is sitting
    // on top of has a centre far away in absolute terms but is right here.
    const double toCentre = glm::length(cameraPosition - centre);
    const double distance = std::max(1.0, toCentre - size * 0.5);

    // Reject everything below the horizon before descending into it. If a
    // patch is hidden by the planet's own curvature then so is every one of
    // its children, so this prunes whole subtrees rather than leaves - which
    // is the difference between selecting the far side at full detail and not
    // touching it at all.
    //
    // Safe to do here, unlike frustum culling, because the horizon depends
    // only on where the camera is and not on where it is pointing. Turning on
    // the spot changes nothing, so nothing has to be rebuilt when it does.
    const double cameraDistance = glm::length(cameraPosition);
    if (cameraDistance > planetRadius) {
        // Half the diagonal, plus room for terrain standing up off the sphere.
        const double bounding = size * 0.75 + 15000.0;
        const double occluder = planetRadius * 0.985;
        if (glm::dot(centre, cameraPosition) + bounding * cameraDistance <
            occluder * occluder) {
            return;
        }
    }

    const bool wantsSplit = key.level < MAX_LEVEL && (size / distance) > splitFactor;

    if (wantsSplit) {
        PatchKey children[4];
        bool allReady = true;
        int n = 0;
        for (uint32_t dy = 0; dy < 2; dy++) {
            for (uint32_t dx = 0; dx < 2; dx++) {
                PatchKey& child = children[n++];
                child.face = key.face;
                child.level = static_cast<uint8_t>(key.level + 1);
                child.x = key.x * 2 + dx;
                child.y = key.y * 2 + dy;
                if (!isReady(child)) {
                    allReady = false;
                    wanted.push_back(child);
                }
            }
        }

        if (allReady) {
            for (const PatchKey& child : children) {
                selectRecursive(child, cameraPosition, planetRadius, isReady, visible, wanted);
            }
            return;
        }
        // Otherwise fall through and keep drawing this level. The missing
        // children are queued; the split happens once all four exist.
    }

    // Anything reached by descending is ready by construction - the parent
    // checked before it recursed. Only the six face roots can arrive here
    // unbuilt, and only in the first frames after startup.
    if (isReady(key)) {
        visible.push_back(key);
    } else {
        wanted.push_back(key);
    }
}

void PatchTree::select(const glm::dvec3& cameraPosition, float planetRadius,
                       const std::function<bool(const PatchKey&)>& isReady,
                       std::vector<PatchKey>& visible,
                       std::vector<PatchKey>& wanted) const {
    visible.clear();
    wanted.clear();
    for (uint8_t face = 0; face < 6; face++) {
        PatchKey root;
        root.face = face;
        selectRecursive(root, cameraPosition, planetRadius, isReady, visible, wanted);
    }
}

void PatchTree::build(Patch& patch, const core::DensityField& field, float planetRadius) {
    const PatchKey& key = patch.key;
    patch.centre = patchCentre(key, planetRadius);

    // Sample the terrain on a regular grid across the patch, plus one ring
    // outside it. The ring is the skirt: neighbouring patches at different
    // levels do not share edge vertices, so the seam between them opens a
    // hairline crack. Hanging a short wall down from every edge hides it,
    // which is cheaper and far more robust than stitching the two levels
    // together.
    constexpr int N = VERTS;
    patch.vertices.clear();
    patch.vertices.reserve(N * N + 4 * N);
    patch.indices.clear();
    patch.indices.reserve(GRID * GRID * 6 + 4 * GRID * 6);

    std::vector<glm::dvec3> world(N * N);
    std::vector<float> elevation(N * N);
    std::vector<float> smooth(N * N);

    double maxRadius = 0.0;
    double minSurfaceRadius = std::numeric_limits<double>::max();
    double maxSurfaceRadius = 0.0;

    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            const double u = static_cast<double>(i) / GRID;
            const double v = static_cast<double>(j) / GRID;
            const glm::dvec3 dir = patchDirection(key, u, v);

            const glm::vec3 n(dir);
            const float h = field.getTerrainHeight(n);
            const float seaLevel = field.getSeaLevelHeight();

            // Oceans render as a flat surface at sea level; the floor beneath
            // is real geometry but is not what is being looked at.
            const float surface = std::max(h, seaLevel);
            elevation[j * N + i] = h;

            // Kept separately for colouring the sea. Sub-grid roughness
            // belongs in the geometry but not in ocean depth: it is invisible
            // under kilometres of water, and because vertex spacing changes
            // between levels of detail the same noise is sampled at a
            // different scale in each one, banding the ocean along the rings
            // where the level changes.
            smooth[j * N + i] = field.getLargeScaleElevation(n);

            const double radius = static_cast<double>(planetRadius) + surface;
            world[j * N + i] = dir * radius;
            maxRadius = std::max(maxRadius, glm::length(world[j * N + i] - patch.centre));
            minSurfaceRadius = std::min(minSurfaceRadius, radius);
            maxSurfaceRadius = std::max(maxSurfaceRadius, radius);
        }
    }
    patch.boundingRadius = static_cast<float>(maxRadius);

    // Positions are stored relative to the patch centre so they stay small.
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            algorithms::MeshVertex vertex;
            vertex.position = glm::vec3(world[j * N + i] - patch.centre);
            vertex.normal = glm::vec3(glm::normalize(world[j * N + i]));
            vertex.color = glm::vec3(0.5f);
            patch.vertices.push_back(vertex);
        }
    }

    // Normals from the grid itself rather than from the sphere, so slopes
    // actually catch the light.
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            const int i0 = std::max(i - 1, 0), i1 = std::min(i + 1, N - 1);
            const int j0 = std::max(j - 1, 0), j1 = std::min(j + 1, N - 1);
            const glm::vec3 du = patch.vertices[j * N + i1].position -
                                 patch.vertices[j * N + i0].position;
            const glm::vec3 dv = patch.vertices[j1 * N + i].position -
                                 patch.vertices[j0 * N + i].position;
            glm::vec3 n = glm::cross(du, dv);
            const float len = glm::length(n);
            const glm::vec3 outward = glm::vec3(glm::normalize(world[j * N + i]));
            n = len > 1e-9f ? n / len : outward;
            if (glm::dot(n, outward) < 0.0f) {
                n = -n;
            }
            patch.vertices[j * N + i].normal = n;
        }
    }

    // Colour from the same fields the simulation produced.
    const float maxElevation = std::max(field.getMaxElevation(), 1.0f);
    const float maxDepth = std::max(field.getMaxOceanDepth(), 1.0f);
    const float seaLevel = field.getSeaLevelHeight();

    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            const int index = j * N + i;
            const glm::vec3 dir = glm::vec3(glm::normalize(world[index]));
            const float relative = elevation[index] - seaLevel;

            glm::vec3 colour;
            if (relative < 0.0f) {
                // Coastlines come from the full-detail height so the colour
                // change lands exactly where the geometry meets sea level;
                // only the depth shading uses the resolved shape.
                const float depth =
                    glm::clamp((seaLevel - smooth[index]) / maxDepth, 0.0f, 1.0f);
                const glm::vec3 water = glm::mix(glm::vec3(0.18f, 0.45f, 0.65f),
                                                 glm::vec3(0.02f, 0.10f, 0.30f),
                                                 std::sqrt(depth));
                const float ice = field.getSeaIceCoverage(dir);
                colour = glm::mix(water, glm::vec3(0.86f, 0.90f, 0.94f), ice);
            } else {
                const float snowLine = field.getSnowLineElevation(dir);
                const float e = relative / maxElevation;
                if (relative > snowLine) {
                    const float t = glm::clamp((relative - snowLine) / (maxElevation * 0.15f),
                                               0.0f, 1.0f);
                    colour = glm::mix(glm::vec3(0.62f, 0.62f, 0.63f),
                                      glm::vec3(0.95f, 0.95f, 0.98f), t);
                } else if (e < 0.012f) {
                    colour = glm::vec3(0.82f, 0.76f, 0.57f);
                } else if (e < 0.10f) {
                    const float t = glm::clamp((e - 0.012f) / 0.088f, 0.0f, 1.0f);
                    colour = glm::mix(glm::vec3(0.32f, 0.55f, 0.22f),
                                      glm::vec3(0.17f, 0.38f, 0.14f), t);
                } else if (e < 0.30f) {
                    const float t = glm::clamp((e - 0.10f) / 0.20f, 0.0f, 1.0f);
                    colour = glm::mix(glm::vec3(0.17f, 0.38f, 0.14f),
                                      glm::vec3(0.44f, 0.36f, 0.27f), t);
                } else {
                    const float t = glm::clamp((e - 0.30f) / 0.25f, 0.0f, 1.0f);
                    colour = glm::mix(glm::vec3(0.44f, 0.36f, 0.27f),
                                      glm::vec3(0.55f, 0.53f, 0.50f), t);
                }
            }
            patch.vertices[index].color = colour;
        }
    }

    // Interior triangles.
    for (int j = 0; j < GRID; j++) {
        for (int i = 0; i < GRID; i++) {
            const uint32_t a = static_cast<uint32_t>(j * N + i);
            const uint32_t b = a + 1;
            const uint32_t c = a + N;
            const uint32_t d = c + 1;
            patch.indices.push_back(a); patch.indices.push_back(c); patch.indices.push_back(b);
            patch.indices.push_back(b); patch.indices.push_back(c); patch.indices.push_back(d);
        }
    }

    // Skirt: drop a copy of each edge vertex inward along its own radius and
    // join it to the edge with a strip of triangles.
    //
    // How deep it has to hang is set by how far a coarser neighbour's edge
    // departs from this one, and nothing else. That neighbour draws a straight
    // chord between vertices this patch has one or three extra vertices
    // between, so the gap is the amount this edge bends away from straight -
    // its second difference - and not its total height, its slope, or the size
    // of the patch.
    //
    // Getting this wrong is expensive in both directions. Sized from the patch
    // width it scales the wrong way, because patches shrink with level while
    // terrain does not, so it is generous from orbit and too thin on approach.
    // Sized from the patch's total relief it is far too deep - a patch can
    // climb a kilometre across its width while every step along its edge is
    // nearly straight - and an over-deep skirt is not free: it is a wall of
    // ground hanging off every patch edge, and at a grazing angle you see it.
    float worstBend = 0.0f;
    const auto measureEdge = [&](int index0, int index1, int index2) {
        const float chord = 0.5f * (static_cast<float>(glm::length(world[index0])) +
                                    static_cast<float>(glm::length(world[index2])));
        worstBend = std::max(worstBend,
                             std::abs(static_cast<float>(glm::length(world[index1])) - chord));
    };
    for (int i = 1; i < N - 1; i++) {
        measureEdge(i - 1, i, i + 1);                                              // top
        measureEdge((N - 1) * N + i - 1, (N - 1) * N + i, (N - 1) * N + i + 1);     // bottom
        measureEdge((i - 1) * N, i * N, (i + 1) * N);                              // left
        measureEdge((i - 1) * N + N - 1, i * N + N - 1, (i + 1) * N + N - 1);       // right
    }

    // worstBend is already the one-level gap: a neighbour one level coarser
    // joins alternate vertices, and the vertex it skips stands off that chord
    // by exactly this. Two levels coarser spans four steps instead of two and
    // the departure grows with the square of the span, so four times over. The
    // factor below is that, doubled, and no more - the skirt is only ever seen
    // when it is too deep.
    const float skirtDepth =
        std::max(worstBend * 8.0f,
                 static_cast<float>(patchWorldSize(key, planetRadius)) * 0.002f);
    patch.skirtDepth = skirtDepth;

    const auto addSkirt = [&](const std::vector<int>& edge) {
        const uint32_t base = static_cast<uint32_t>(patch.vertices.size());
        for (int index : edge) {
            algorithms::MeshVertex v = patch.vertices[index];
            const glm::vec3 inward = glm::vec3(glm::normalize(world[index]));
            v.position -= inward * skirtDepth;
            patch.vertices.push_back(v);
        }
        for (size_t s = 0; s + 1 < edge.size(); s++) {
            const uint32_t top0 = static_cast<uint32_t>(edge[s]);
            const uint32_t top1 = static_cast<uint32_t>(edge[s + 1]);
            const uint32_t bot0 = base + static_cast<uint32_t>(s);
            const uint32_t bot1 = bot0 + 1;
            patch.indices.push_back(top0); patch.indices.push_back(bot0); patch.indices.push_back(top1);
            patch.indices.push_back(top1); patch.indices.push_back(bot0); patch.indices.push_back(bot1);
            patch.indices.push_back(top1); patch.indices.push_back(bot0); patch.indices.push_back(top0);
            patch.indices.push_back(bot1); patch.indices.push_back(bot0); patch.indices.push_back(top1);
        }
    };

    std::vector<int> edge;
    edge.reserve(N);
    for (int i = 0; i < N; i++) edge.push_back(i);                         addSkirt(edge);
    edge.clear();
    for (int i = 0; i < N; i++) edge.push_back((N - 1) * N + i);           addSkirt(edge);
    edge.clear();
    for (int j = 0; j < N; j++) edge.push_back(j * N);                     addSkirt(edge);
    edge.clear();
    for (int j = 0; j < N; j++) edge.push_back(j * N + (N - 1));           addSkirt(edge);

    patch.built = true;
}

} // namespace rendering
