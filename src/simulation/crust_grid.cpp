#include "simulation/crust_grid.hpp"
#include "utils/parallel.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <random>
#include <utility>

namespace simulation {

namespace {

constexpr float PI = 3.14159265358979323846f;

// Rotate a point about an axis by an angle (Rodrigues' formula).
glm::vec3 rotateAbout(const glm::vec3& p, const glm::vec3& axis, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return p * c + glm::cross(axis, p) * s + axis * glm::dot(axis, p) * (1.0f - c);
}

// Value noise on the sphere, used only to seed the initial crust. Cheap and
// self-contained - the simulation, not this, decides the final shape.
float hashNoise(const glm::vec3& p, uint32_t seed) {
    const float d = glm::dot(p, glm::vec3(127.1f, 311.7f, 74.7f)) + static_cast<float>(seed) * 0.137f;
    const float e = glm::dot(p, glm::vec3(269.5f, 183.3f, 246.1f)) - static_cast<float>(seed) * 0.311f;
    const float a = std::sin(d) * 43758.5453f;
    const float b = std::sin(e) * 28001.8384f;
    return (a - std::floor(a)) + (b - std::floor(b)) - 1.0f;  // roughly [-1, 1]
}

float smoothSphereNoise(const glm::vec3& n, uint32_t seed, float frequency) {
    // Sum a few rotated samples so the field is smooth rather than white.
    float total = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    for (int octave = 0; octave < 4; octave++) {
        const glm::vec3 p = n * frequency;
        const glm::vec3 base = glm::floor(p);
        const glm::vec3 frac = p - base;
        const glm::vec3 w = frac * frac * (3.0f - 2.0f * frac);

        float corners[8];
        for (int i = 0; i < 8; i++) {
            const glm::vec3 offset(static_cast<float>(i & 1),
                                   static_cast<float>((i >> 1) & 1),
                                   static_cast<float>((i >> 2) & 1));
            corners[i] = hashNoise(base + offset, seed + static_cast<uint32_t>(octave) * 7919u);
        }
        const float x00 = glm::mix(corners[0], corners[1], w.x);
        const float x10 = glm::mix(corners[2], corners[3], w.x);
        const float x01 = glm::mix(corners[4], corners[5], w.x);
        const float x11 = glm::mix(corners[6], corners[7], w.x);
        const float y0 = glm::mix(x00, x10, w.y);
        const float y1 = glm::mix(x01, x11, w.y);

        total += glm::mix(y0, y1, w.z) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return norm > 0.0f ? total / norm : 0.0f;
}

} // namespace

// ============================================================================
// The rock record
// ============================================================================

void CrustGrid::Marker::deposit(RockType rock, double addedVolume, float atAge) {
    if (addedVolume <= 0.0) {
        return;
    }

    // Rock of the same type arriving on top of itself is one episode, not two.
    if (layerCount > 0 && layers[layerCount - 1].rock == rock) {
        Layer& top = layers[layerCount - 1];
        const double total = top.volume + addedVolume;
        top.age = static_cast<float>((top.age * top.volume + atAge * addedVolume) / total);
        top.volume = total;
        refresh();
        return;
    }

    if (layerCount >= MAX_LAYERS) {
        // Out of room. Merge the two deepest episodes: that rock is buried far
        // enough to be metamorphosed, and its individual history is the least
        // worth keeping.
        Layer& deepest = layers[0];
        const Layer& next = layers[1];
        const double total = deepest.volume + next.volume;
        if (total > 0.0) {
            deepest.age = static_cast<float>(
                (deepest.age * deepest.volume + next.age * next.volume) / total);
            // The merged rock takes the identity of whichever dominates.
            if (next.volume > deepest.volume) {
                deepest.rock = next.rock;
            }
            deepest.volume = total;
        }
        for (int i = 1; i < layerCount - 1; i++) {
            layers[i] = layers[i + 1];
        }
        layerCount--;
    }

    layers[layerCount].rock = rock;
    layers[layerCount].volume = addedVolume;
    layers[layerCount].age = atAge;
    layerCount++;
    refresh();
}

double CrustGrid::Marker::erodeFromTop(double wanted) {
    double removed = 0.0;
    while (wanted > 0.0 && layerCount > 0) {
        Layer& top = layers[layerCount - 1];
        if (top.volume > wanted) {
            top.volume -= wanted;
            removed += wanted;
            wanted = 0.0;
        } else {
            removed += top.volume;
            wanted -= top.volume;
            layerCount--;
        }
    }
    refresh();
    return removed;
}

double CrustGrid::Marker::removeFromBottom(double wanted) {
    double removed = 0.0;
    while (wanted > 0.0 && layerCount > 0) {
        Layer& bottom = layers[0];
        if (bottom.volume > wanted) {
            bottom.volume -= wanted;
            removed += wanted;
            wanted = 0.0;
        } else {
            removed += bottom.volume;
            wanted -= bottom.volume;
            for (int i = 0; i < layerCount - 1; i++) {
                layers[i] = layers[i + 1];
            }
            layerCount--;
        }
    }
    refresh();
    return removed;
}

double CrustGrid::Marker::consumeProportionally(double wanted) {
    if (wanted <= 0.0 || volume <= 0.0) {
        return 0.0;
    }
    const double fraction = std::min(1.0, wanted / volume);
    double removed = 0.0;
    for (int i = 0; i < layerCount; i++) {
        const double take = layers[i].volume * fraction;
        layers[i].volume -= take;
        removed += take;
    }
    refresh();
    return removed;
}

CrustGrid::CrustGrid(float planetRadius, uint32_t seed, int subdivisions, int plateCount,
                     int parcelsPerCell)
    : planetRadius(planetRadius), seed(seed), climate(*this) {
    if (parcelsPerCell > 0) {
        constants.markersPerCell = parcelsPerCell;
        constants.maxMarkersPerCell =
            std::max(constants.maxMarkersPerCell, parcelsPerCell * 2);
    }
    buildGeodesicGrid(subdivisions);
    buildAccelerator();
    assignPlates(plateCount);
    seedInitialCrust();
    seedMarkers();
    updateIsostasy();
    solveSeaLevel();
    refreshElevationField();

    // Constructed with the grid, but the surface only exists now - so this is
    // the first point at which asking what climate it produces means anything.
    climate.update();
    initialCrustVolume = computeCrustVolume();
}

// How much crust exists is a question about the material, so it is answered by
// the parcels. The cell field is a smoothed readout that reconcileCrust also
// writes to directly, and summing that instead gave an imbalance that looked
// like a leak but was only the difference between the rock and the picture
// we draw of it.
CrustGrid::Diagnostics CrustGrid::computeDiagnostics() const {
    Diagnostics d;
    if (cells.empty()) {
        return d;
    }

    // Which plate holds how much crust in each cell. Real plates do not
    // overlap: at a convergent margin one of them goes down. Crust sitting on
    // a cell that another plate dominates is either a margin one cell wide -
    // which is expected - or two plates occupying the same ground, which is
    // not, and which looks exactly like a landmass sliding across another one.
    std::vector<std::vector<std::pair<uint16_t, double>>> perCell(cells.size());
    for (const Marker& marker : markers) {
        const int cell = findNearestCell(marker.position);
        if (cell < 0) continue;
        auto& list = perCell[cell];
        auto it = std::find_if(list.begin(), list.end(),
                               [&](const auto& e) { return e.first == marker.plateId; });
        if (it == list.end()) {
            list.emplace_back(marker.plateId, marker.volume);
        } else {
            it->second += marker.volume;
        }
    }

    double totalVolume = 0.0;
    double minorityVolume = 0.0;
    for (const auto& list : perCell) {
        if (list.empty()) {
            d.emptyCells++;
            continue;
        }
        d.maxPlatesInOneCell = std::max(d.maxPlatesInOneCell, static_cast<int>(list.size()));

        double largest = 0.0;
        double sum = 0.0;
        for (const auto& [plate, volume] : list) {
            (void)plate;
            sum += volume;
            largest = std::max(largest, volume);
        }
        totalVolume += sum;
        minorityVolume += sum - largest;
    }
    d.overlapFraction = totalVolume > 0.0
        ? static_cast<float>(minorityVolume / totalVolume) : 0.0f;

    d.maxElevationJump = largestElevationJump;
    d.cellOfLargestJump = largestJumpCell;

    std::vector<int> population(plates.size(), 0);
    for (const Cell& cell : cells) {
        if (cell.plateId < population.size()) {
            population[cell.plateId]++;
        }
    }
    for (size_t p = 0; p < plates.size(); p++) {
        if (population[p] > 0 && population[p] < minPlateCellCount()) {
            d.microPlates++;
        }
        d.fastestPlateCmPerYear = std::max(
            d.fastestPlateCmPerYear,
            plates[p].angularVelocity() * planetRadius * 1e-4f);
    }

    return d;
}

double CrustGrid::computeCrustVolume() const {
    double volume = 0.0;
    for (const Marker& marker : markers) {
        volume += marker.volume;
    }
    return volume;
}

double CrustGrid::computeContinentalVolume() const {
    double volume = 0.0;
    for (const Marker& marker : markers) {
        if (marker.density < constants.subductionDensity) {
            volume += marker.volume;
        }
    }
    return volume;
}

// ============================================================================
// Grid construction
// ============================================================================

void CrustGrid::buildGeodesicGrid(int subdivisions) {
    // Start from an icosahedron and subdivide its faces. The resulting vertex
    // set is near-uniform on the sphere, which matters: an equirectangular
    // grid would crowd cells at the poles and make the timestep pole-limited.
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;

    std::vector<glm::vec3> verts = {
        glm::normalize(glm::vec3(-1,  t,  0)), glm::normalize(glm::vec3( 1,  t,  0)),
        glm::normalize(glm::vec3(-1, -t,  0)), glm::normalize(glm::vec3( 1, -t,  0)),
        glm::normalize(glm::vec3( 0, -1,  t)), glm::normalize(glm::vec3( 0,  1,  t)),
        glm::normalize(glm::vec3( 0, -1, -t)), glm::normalize(glm::vec3( 0,  1, -t)),
        glm::normalize(glm::vec3( t,  0, -1)), glm::normalize(glm::vec3( t,  0,  1)),
        glm::normalize(glm::vec3(-t,  0, -1)), glm::normalize(glm::vec3(-t,  0,  1))
    };

    std::vector<glm::ivec3> faces = {
        {0, 11,  5}, {0,  5,  1}, {0,  1,  7}, {0,  7, 10}, {0, 10, 11},
        {1,  5,  9}, {5, 11,  4}, {11, 10, 2}, {10, 7,  6}, {7,  1,  8},
        {3,  9,  4}, {3,  4,  2}, {3,  2,  6}, {3,  6,  8}, {3,  8,  9},
        {4,  9,  5}, {2,  4, 11}, {6,  2, 10}, {8,  6,  7}, {9,  8,  1}
    };

    for (int level = 0; level < subdivisions; level++) {
        std::map<std::pair<int, int>, int> midpointCache;
        std::vector<glm::ivec3> nextFaces;
        nextFaces.reserve(faces.size() * 4);

        auto midpoint = [&](int a, int b) -> int {
            const std::pair<int, int> key = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
            const auto it = midpointCache.find(key);
            if (it != midpointCache.end()) {
                return it->second;
            }
            const int index = static_cast<int>(verts.size());
            verts.push_back(glm::normalize(verts[a] + verts[b]));
            midpointCache.emplace(key, index);
            return index;
        };

        for (const glm::ivec3& f : faces) {
            const int a = midpoint(f.x, f.y);
            const int b = midpoint(f.y, f.z);
            const int c = midpoint(f.z, f.x);
            nextFaces.push_back({f.x, a, c});
            nextFaces.push_back({f.y, b, a});
            nextFaces.push_back({f.z, c, b});
            nextFaces.push_back({a, b, c});
        }
        faces.swap(nextFaces);
    }

    cells.resize(verts.size());
    cellPositions.resize(verts.size());
    for (size_t i = 0; i < verts.size(); i++) {
        cells[i].position = verts[i];
        cellPositions[i] = verts[i];
    }

    // Adjacency from the triangle edges, deduplicated.
    std::vector<std::vector<int>> adjacency(verts.size());
    auto connect = [&](int a, int b) {
        auto& list = adjacency[a];
        if (std::find(list.begin(), list.end(), b) == list.end()) {
            list.push_back(b);
        }
    };
    for (const glm::ivec3& f : faces) {
        connect(f.x, f.y); connect(f.y, f.x);
        connect(f.y, f.z); connect(f.z, f.y);
        connect(f.z, f.x); connect(f.x, f.z);
    }

    neighbourStart.resize(verts.size() + 1);
    neighbourStart[0] = 0;
    for (size_t i = 0; i < verts.size(); i++) {
        neighbourStart[i + 1] = neighbourStart[i] + static_cast<int>(adjacency[i].size());
    }
    neighbourIndices.reserve(neighbourStart.back());
    for (const auto& list : adjacency) {
        neighbourIndices.insert(neighbourIndices.end(), list.begin(), list.end());
    }

    // Keep the triangulation. Adjacency alone says which cells are next to
    // each other but not which three bound a given patch of sphere, and that
    // is what reconstructing a value between cell centres needs.
    triangles.assign(faces.begin(), faces.end());

    std::vector<std::vector<int>> incident(verts.size());
    for (size_t f = 0; f < triangles.size(); f++) {
        incident[triangles[f].x].push_back(static_cast<int>(f));
        incident[triangles[f].y].push_back(static_cast<int>(f));
        incident[triangles[f].z].push_back(static_cast<int>(f));
    }

    cellTriangleStart.resize(verts.size() + 1);
    cellTriangleStart[0] = 0;
    for (size_t i = 0; i < verts.size(); i++) {
        cellTriangleStart[i + 1] = cellTriangleStart[i] + static_cast<int>(incident[i].size());
    }
    cellTriangleIndices.reserve(cellTriangleStart.back());
    for (const auto& list : incident) {
        cellTriangleIndices.insert(cellTriangleIndices.end(), list.begin(), list.end());
    }
}

void CrustGrid::refreshElevationField() {
    elevationField.resize(cells.size());
    for (size_t i = 0; i < cells.size(); i++) {
        elevationField[i] = cells[i].elevation;
    }

    elevationGradient.resize(cells.size());
    for (size_t i = 0; i < cells.size(); i++) {
        elevationGradient[i] = estimateGradient(static_cast<int>(i), elevationField);
    }
}

glm::vec3 CrustGrid::estimateGradient(int cell, const std::vector<float>& values) const {
    // Least squares fit of a plane to the neighbours, in the tangent plane at
    // this cell.
    //
    // Each neighbour gives one equation: the slope along the direction towards
    // it must account for the difference in value. Five or six of those
    // overdetermine a two-component gradient, and solving them together rather
    // than averaging pairs is what stops the result depending on which way the
    // neighbours happen to be arranged.
    const glm::vec3 up = cellPositions[cell];

    // Any pair of axes spanning the tangent plane will do; the gradient that
    // comes out is returned in world space and does not depend on the choice.
    const glm::vec3 reference =
        std::abs(up.y) > 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 e1 = glm::normalize(glm::cross(reference, up));
    const glm::vec3 e2 = glm::cross(up, e1);

    double a11 = 0.0, a12 = 0.0, a22 = 0.0, b1 = 0.0, b2 = 0.0;

    for (int k = 0; k < neighbourCount(cell); k++) {
        const int other = neighbourAt(cell, k);
        const glm::vec3 delta = cellPositions[other] - up;

        // Flattened into the tangent plane. Over one cell spacing the sphere
        // barely curves, so the error in doing so is far below the noise in
        // the values themselves.
        const glm::vec2 offset(glm::dot(delta, e1), glm::dot(delta, e2));
        const double lengthSquared = static_cast<double>(glm::dot(offset, offset));
        if (lengthSquared < 1e-20) {
            continue;
        }

        const double difference = static_cast<double>(values[other] - values[cell]);
        a11 += offset.x * offset.x;
        a12 += offset.x * offset.y;
        a22 += offset.y * offset.y;
        b1 += offset.x * difference;
        b2 += offset.y * difference;
    }

    const double determinant = a11 * a22 - a12 * a12;
    if (std::abs(determinant) < 1e-24) {
        return glm::vec3(0.0f);
    }

    const double g1 = (b1 * a22 - b2 * a12) / determinant;
    const double g2 = (a11 * b2 - a12 * b1) / determinant;

    // Back to world space, still tangent to the sphere. Note this is per unit
    // of chord length on the unit sphere, so a displacement measured the same
    // way can be dotted with it directly.
    return e1 * static_cast<float>(g1) + e2 * static_cast<float>(g2);
}

float CrustGrid::reconstruct(const glm::vec3& sphereNormal, const std::vector<float>& values,
                             const std::vector<glm::vec3>& gradients) const {
    int corner[3];
    float weight[3];
    if (!barycentricCells(sphereNormal, corner, weight)) {
        return 0.0f;
    }

    if (gradients.size() != values.size()) {
        float flat = 0.0f;
        for (int i = 0; i < 3; i++) {
            flat += weight[i] * values[corner[i]];
        }
        return flat;
    }

    // A cubic Bezier triangle through the three corner values, with the two
    // control points along each edge placed so the surface leaves each corner
    // at the slope measured there.
    //
    // Linear weights reproduce the corners and nothing else, so the surface is
    // a flat plane inside every triangle with a crease at every edge - the
    // faceting that replaced the honeycomb. This matches the slope as well as
    // the height at each corner, so neighbouring triangles leave their shared
    // corners heading the same way and the creases go.
    const glm::vec3& a = cellPositions[corner[0]];
    const glm::vec3& b = cellPositions[corner[1]];
    const glm::vec3& c = cellPositions[corner[2]];

    const float fa = values[corner[0]];
    const float fb = values[corner[1]];
    const float fc = values[corner[2]];

    // Directional derivative along each edge, a third of the way in - which is
    // where a cubic Bezier's inner control points sit.
    const auto along = [&](int from, const glm::vec3& toward, float value) {
        return value + glm::dot(gradients[corner[from]], toward - cellPositions[corner[from]]) / 3.0f;
    };

    const float b210 = along(0, b, fa);
    const float b120 = along(1, a, fb);
    const float b021 = along(1, c, fb);
    const float b012 = along(2, b, fc);
    const float b102 = along(2, a, fc);
    const float b201 = along(0, c, fa);

    // The centre control point, set so a plane stays a plane.
    const float b111 = (b210 + b120 + b021 + b012 + b102 + b201) / 4.0f -
                       (fa + fb + fc) / 6.0f;

    const float u = weight[0];
    const float v = weight[1];
    const float w = weight[2];

    return fa * u * u * u + fb * v * v * v + fc * w * w * w +
           3.0f * (b210 * u * u * v + b120 * u * v * v +
                   b021 * v * v * w + b012 * v * w * w +
                   b102 * u * w * w + b201 * u * u * w) +
           6.0f * b111 * u * v * w;
}

float CrustGrid::channelWidthFor(float catchments) const {
    // Width grows with discharge, which is roughly how real channels widen:
    // more water needs more cross-section, and a channel gets it mostly by
    // widening rather than by deepening.
    //
    // The exponent is above a half deliberately. Square root is the textbook
    // hydraulic relation, but discharge across a network spans two or three
    // orders of magnitude and a square root compresses that into one - so every
    // river came out much the same size, which is most of what made the network
    // read as a grid of channels rather than as a network.
    //
    // Floored so a headwater stream is not sub-metre, and capped because the
    // relation describes channels: a continental trunk draining a thousand
    // cells is still a river and not an inland sea.
    return glm::clamp(180.0f * std::pow(catchments, 0.62f), 120.0f,
                      static_cast<float>(cellSpacing()) * 0.55f);
}

CrustGrid::RiverSample CrustGrid::sampleRiverGeometry(
    const Snapshot& snapshot, const glm::vec3& sphereNormal) const {
    RiverSample nearestRiver;

    if (snapshot.channelDepth.size() != cells.size() ||
        snapshot.discharge.size() != cells.size() ||
        snapshot.flowsInto.size() != cells.size()) {
        return nearestRiver;
    }

    const glm::vec3 n = glm::normalize(sphereNormal);
    const int centre = findNearestCell(n);
    if (centre < 0) {
        return nearestRiver;
    }

    // Rivers are drawn along the path the water takes, not over the cells that
    // carry it. A cell is seventeen kilometres across and a river is not, so
    // colouring cells by discharge would draw every major river as a band
    // wider than the Rhine's whole catchment. Each cell routes into a specific
    // neighbour, though, so the channel between the two is a line, and how
    // close a point lies to that line is what decides whether it is in the
    // river.
    //
    // The nearest cell's own channel is not enough: a point near the edge of a
    // cell can be closest to a channel belonging to a neighbour. Testing the
    // ring as well is what makes the network continuous instead of a series of
    // dashes.
    const float spacing = cellSpacing();
    if (spacing <= 0.0f) {
        return nearestRiver;
    }

    const auto consider = [&](int cell) {
        if (cell < 0) {
            return;
        }
        const int into = snapshot.flowsInto[cell];
        if (into < 0 || into >= static_cast<int>(cells.size())) {
            return;
        }

        // Only where enough water has gathered to cut a channel. Every cell
        // has some discharge - it rains everywhere - and what makes a river is
        // the collecting.
        //
        // Three upstream cells rather than twelve. Twelve is closer to where a
        // real river starts and it produced a network nobody could find: some
        // eight hundred segments spread over every continent, each a few
        // hundred metres wide, adding up to half a per cent of the land. That
        // is roughly right and entirely invisible.
        //
        // The grid is the reason. Seventeen kilometres per cell cannot resolve
        // a tributary, so what is drawn is a representation of the network at
        // the scale the simulation knows it - and a representation that only
        // shows the trunk is not showing the network.
        const float catchments = snapshot.discharge[cell];
        if (catchments < constants.channelThreshold) {
            return;
        }

        // Through the sub-grid crossing points where they are known, so the
        // river is not pinned to the lattice of cell centres.
        const bool havePoints = snapshot.channelPoint.size() == cells.size();
        const auto crossing = [&](int c) {
            if (havePoints && glm::dot(snapshot.channelPoint[c], snapshot.channelPoint[c]) > 0.5f) {
                return snapshot.channelPoint[c];
            }
            return cellPositions[c];
        };
        const glm::vec3 a = crossing(cell);
        const glm::vec3 b = crossing(into);
        const glm::vec3 along = b - a;
        const float lengthSquared = glm::dot(along, along);
        if (lengthSquared < 1e-12f) {
            return;
        }

        const float width = channelWidthFor(catchments);

        // Meanders.
        //
        // The routing knows that this cell drains into that one and nothing
        // about the path between them, because the path is far below the grid.
        // Joining the two centres with a straight line is the honest drawing of
        // what is known and it looks like a drainage diagram, because no river
        // has ever run straight for seventeen kilometres - a channel wanders
        // across its floodplain, and that wandering is most of what makes a
        // river recognisable from above.
        //
        // So the path within a cell is synthesised, exactly as sub-grid relief
        // is: the simulation decides where the network goes and noise decides
        // what it looks like in between. Two harmonics, vanishing at both ends
        // so the channel still leaves one cell centre and arrives at the next -
        // connectivity is the part that has to stay true, because it is the part
        // the simulation actually determined.
        //
        // The amplitude is a fraction of the segment, which is what a meander
        // belt is: ten to twenty channel widths, and on this grid that is
        // kilometres.
        // Cheap rejection before any of that. The meandering channel never
        // leaves a corridor of the straight segment plus the bend amplitude
        // plus its own width, so a point outside that corridor cannot be in
        // the river and does not need the curve walked for it. Almost every
        // sample on a planet is outside every corridor.
        {
            const float straightT =
                glm::clamp(glm::dot(n - a, along) / lengthSquared, 0.0f, 1.0f);
            const float straight = glm::length(n - (a + along * straightT)) * planetRadius;
            const float corridor =
                width * 3.0f + std::sqrt(lengthSquared) * 0.16f * planetRadius;
            if (straight > corridor) {
                return;
            }
        }

        const glm::vec3 up = glm::normalize(a + b);
        glm::vec3 lateral = glm::cross(up, along);
        const float lateralLength = glm::length(lateral);

        // Deterministic from the pair, so a river does not rewrite its own
        // course between one frame and the next.
        const float phase = static_cast<float>((cell * 2654435761u) % 1000u) * 0.006283f;
        const float second = static_cast<float>((into * 40503u) % 1000u) * 0.006283f;

        const float bendAmplitude = std::sqrt(lengthSquared) * 0.16f;

        float nearest = 1e30f;
        if (lateralLength > 1e-9f) {
            lateral /= lateralLength;

            // Walked rather than solved: the curve has no closed-form nearest
            // point, and a handful of samples is cheaper than one that does.
            //
            // The distance is to the straight pieces between the samples, not
            // to the samples themselves. Measuring to the points draws a circle
            // around each one, and with samples two kilometres apart and a
            // channel one kilometre wide that is a string of beads rather than
            // a river - which is exactly what it looked like.
            constexpr int SAMPLES = 10;
            const auto curvePoint = [&](float t) {
                const float envelope = std::sin(t * PI);
                const float wander = std::sin(t * PI * 2.0f + phase) * 0.7f +
                                     std::sin(t * PI * 5.0f + second) * 0.3f;
                return a + along * t + lateral * (envelope * wander * bendAmplitude);
            };

            glm::vec3 previous = curvePoint(0.0f);
            for (int s = 1; s <= SAMPLES; s++) {
                const glm::vec3 current = curvePoint(static_cast<float>(s) / SAMPLES);
                const glm::vec3 piece = current - previous;
                const float pieceLengthSquared = glm::dot(piece, piece);
                if (pieceLengthSquared > 1e-16f) {
                    const float u = glm::clamp(glm::dot(n - previous, piece) / pieceLengthSquared,
                                               0.0f, 1.0f);
                    nearest = std::min(nearest, glm::length(n - (previous + piece * u)));
                }
                previous = current;
            }
        } else {
            const float t = glm::clamp(glm::dot(n - a, along) / lengthSquared, 0.0f, 1.0f);
            nearest = glm::length(n - (a + along * t));
        }

        const float distance = nearest * planetRadius;
        if (distance < nearestRiver.distance) {
            nearestRiver.distance = distance;
            nearestRiver.width = width;
            nearestRiver.catchments = catchments;
            nearestRiver.depth = cell < static_cast<int>(snapshot.channelDepth.size())
                                     ? snapshot.channelDepth[cell]
                                     : 0.0f;
        }
    };

    consider(centre);
    for (int k = 0; k < neighbourCount(centre); k++) {
        consider(neighbourAt(centre, k));
    }
    return nearestRiver;
}

float CrustGrid::sampleRiver(const Snapshot& snapshot, const glm::vec3& sphereNormal) const {
    const RiverSample river = sampleRiverGeometry(snapshot, sphereNormal);
    if (river.width <= 0.0f) {
        return 0.0f;
    }

    // A bank rather than a blur. A Gaussian falloff is smooth everywhere,
    // which is what made every river look like a soft stain - water has an
    // edge, and where it ends the ground starts. Half the width of the channel
    // is spent on that edge, which at this scale is a bank and a gravel bar.
    const float half = river.width * 0.5f;
    return 1.0f - glm::smoothstep(half * 0.55f, half * 1.15f, river.distance);
}

float CrustGrid::sampleTemperature(const Snapshot& snapshot,
                                   const glm::vec3& sphereNormal) const {
    if (snapshot.temperature.size() != cells.size()) {
        return 15.0f;
    }
    int corner[3];
    float weight[3];
    if (!barycentricCells(sphereNormal, corner, weight)) {
        return 15.0f;
    }
    // Flat blend across the triangle. Temperature varies smoothly over
    // thousands of kilometres, so the linear part is all there is; the sharp
    // structure people see in a snow line comes from the terrain crossing it,
    // not from the temperature field itself.
    float degrees = 0.0f;
    for (int i = 0; i < 3; i++) {
        degrees += weight[i] * snapshot.temperature[corner[i]];
    }
    return degrees;
}

float CrustGrid::sampleCloudCover(const Snapshot& snapshot,
                                  const glm::vec3& sphereNormal) const {
    if (snapshot.cloudCover.size() != cells.size()) {
        return 0.0f;
    }
    int corner[3];
    float weight[3];
    if (!barycentricCells(sphereNormal, corner, weight)) {
        return 0.0f;
    }
    // Blended flat rather than curved. Cloud has no slope worth reconstructing
    // and the detail that matters at close range comes from noise anyway.
    float cover = 0.0f;
    for (int i = 0; i < 3; i++) {
        cover += weight[i] * snapshot.cloudCover[corner[i]];
    }
    return glm::clamp(cover, 0.0f, 1.0f);
}

CrustGrid::SurfaceSample CrustGrid::sampleSurface(const Snapshot& snapshot,
                                                  const glm::vec3& sphereNormal) const {
    SurfaceSample sample;

    int corner[3];
    float weight[3];
    if (!barycentricCells(sphereNormal, corner, weight)) {
        return sample;
    }

    sample.elevation = reconstruct(sphereNormal, snapshot.elevation, snapshot.elevationGradient);

    // Slope is blended, because the ground genuinely does get steeper and
    // gentler continuously; the rock is not, because a point is made of one
    // thing or another and averaging basalt with granite means nothing. It
    // comes from whichever cell dominates.
    if (snapshot.elevationGradient.size() == snapshot.elevation.size()) {
        glm::vec3 gradient(0.0f);
        for (int i = 0; i < 3; i++) {
            gradient += weight[i] * snapshot.elevationGradient[corner[i]];
        }
        // The gradient is per unit of chord on the unit sphere, and one such
        // unit is a planet radius of ground.
        sample.slope = glm::length(gradient) / std::max(planetRadius, 1.0f);
    }

    int dominant = corner[0];
    float best = weight[0];
    float roughness = 0.0f;
    for (int i = 0; i < 3; i++) {
        if (weight[i] > best) {
            best = weight[i];
            dominant = corner[i];
        }
        if (corner[i] < static_cast<int>(snapshot.surfaceRock.size())) {
            roughness += weight[i] *
                         rockRoughness(static_cast<RockType>(snapshot.surfaceRock[corner[i]]));
        }
    }
    if (dominant < static_cast<int>(snapshot.surfaceRock.size())) {
        sample.rock = snapshot.surfaceRock[dominant];
    }
    sample.roughness = roughness > 0.0f ? roughness : 1.0f;
    return sample;
}

CrustGrid::SurfaceSample CrustGrid::sampleSurface(const glm::vec3& sphereNormal) const {
    SurfaceSample sample;

    int corner[3];
    float weight[3];
    if (!barycentricCells(sphereNormal, corner, weight) ||
        elevationField.size() != cells.size()) {
        return sample;
    }

    sample.elevation = reconstruct(sphereNormal, elevationField, elevationGradient) - seaLevel;

    glm::vec3 gradient(0.0f);
    for (int i = 0; i < 3; i++) {
        gradient += weight[i] * elevationGradient[corner[i]];
    }
    sample.slope = glm::length(gradient) / std::max(planetRadius, 1.0f);

    int dominant = corner[0];
    float best = weight[0];
    for (int i = 1; i < 3; i++) {
        if (weight[i] > best) {
            best = weight[i];
            dominant = corner[i];
        }
    }

    // The live grid keeps its rock in the markers rather than in a published
    // array, so this is the type of the largest parcel sitting on each cell.
    const auto rockAt = [&](int cell) {
        uint8_t rock = static_cast<uint8_t>(RockType::Basalt);
        double biggest = 0.0;
        if (cell >= 0 && cell < static_cast<int>(cellMarkers.size())) {
            for (int index : cellMarkers[cell]) {
                const Marker& marker = markers[index];
                if (marker.layerCount > 0 && marker.volume > biggest) {
                    biggest = marker.volume;
                    rock = static_cast<uint8_t>(marker.layers[marker.layerCount - 1].rock);
                }
            }
        }
        return rock;
    };

    float roughness = 0.0f;
    for (int i = 0; i < 3; i++) {
        roughness += weight[i] * rockRoughness(static_cast<RockType>(rockAt(corner[i])));
    }
    sample.roughness = roughness > 0.0f ? roughness : 1.0f;
    sample.rock = rockAt(dominant);
    return sample;
}

bool CrustGrid::barycentricCells(const glm::vec3& sphereNormal, int outCells[3],
                                 float outWeights[3]) const {
    if (cells.empty() || triangles.empty()) {
        return false;
    }

    const glm::vec3 n = glm::normalize(sphereNormal);
    const int centre = findNearestCell(n);
    if (centre < 0) {
        return false;
    }

    // The nearest cell centre is a corner of whichever triangle contains the
    // point, so only the five or six triangles meeting there need testing.
    const int first = cellTriangleStart[centre];
    const int last = cellTriangleStart[centre + 1];

    int bestTriangle = -1;
    float bestPenalty = std::numeric_limits<float>::max();
    glm::vec3 bestWeights(0.0f);

    for (int t = first; t < last; t++) {
        const glm::ivec3& tri = triangles[cellTriangleIndices[t]];
        const glm::vec3& a = cellPositions[tri.x];
        const glm::vec3& b = cellPositions[tri.y];
        const glm::vec3& c = cellPositions[tri.z];

        // Barycentric coordinates of where the ray along n crosses the plane
        // of the triangle, as the signed volumes of the three tetrahedra it
        // makes with the origin. No plane intersection or division needed to
        // find out whether the point is inside - only the signs matter.
        glm::vec3 w(glm::dot(glm::cross(b, c), n),
                    glm::dot(glm::cross(c, a), n),
                    glm::dot(glm::cross(a, b), n));

        const float total = w.x + w.y + w.z;
        if (std::abs(total) < 1e-20f) {
            continue;
        }
        w /= total;

        // Inside when no coordinate is negative. Tracking how far outside the
        // nearest miss is means a point that lands exactly on an edge, or in
        // the sliver left by the sphere's curvature, still resolves to the
        // triangle it belongs to instead of failing.
        const float penalty = -std::min(std::min(w.x, w.y), std::min(w.z, 0.0f));
        if (penalty < bestPenalty) {
            bestPenalty = penalty;
            bestTriangle = cellTriangleIndices[t];
            bestWeights = w;
            if (penalty <= 0.0f) {
                break;
            }
        }
    }

    if (bestTriangle < 0) {
        return false;
    }

    const glm::ivec3& tri = triangles[bestTriangle];
    outCells[0] = tri.x;
    outCells[1] = tri.y;
    outCells[2] = tri.z;
    outWeights[0] = bestWeights.x;
    outWeights[1] = bestWeights.y;
    outWeights[2] = bestWeights.z;
    return true;
}

int CrustGrid::binIndex(const glm::vec3& n) const {
    const float lat = std::acos(glm::clamp(n.y, -1.0f, 1.0f));        // 0..pi
    const float lon = std::atan2(n.z, n.x) + PI;                       // 0..2pi
    int i = static_cast<int>(lat / PI * BIN_LAT);
    int j = static_cast<int>(lon / (2.0f * PI) * BIN_LON);
    i = glm::clamp(i, 0, BIN_LAT - 1);
    j = glm::clamp(j, 0, BIN_LON - 1);
    return i * BIN_LON + j;
}

void CrustGrid::buildAccelerator() {
    // Cell positions never move, so this is built once. Bins are only a
    // lookup aid - the field itself has no lat/long parameterisation and so
    // no pole singularity.
    bins.assign(BIN_LAT * BIN_LON, {});
    for (size_t i = 0; i < cells.size(); i++) {
        bins[binIndex(cells[i].position)].push_back(static_cast<int>(i));
    }
}

int CrustGrid::findNearestCell(const glm::vec3& sphereNormal) const {
    if (cellPositions.empty()) {
        return -1;
    }
    const glm::vec3 n = glm::normalize(sphereNormal);

    // Step 1: any nearby cell from the bin grid. Bins converge at the poles so
    // the closest bin does not always hold the closest cell - this only needs
    // to be a good starting guess, not the answer.
    const float lat = std::acos(glm::clamp(n.y, -1.0f, 1.0f));
    const float lon = std::atan2(n.z, n.x) + PI;
    const int ci = glm::clamp(static_cast<int>(lat / PI * BIN_LAT), 0, BIN_LAT - 1);
    const int cj = glm::clamp(static_cast<int>(lon / (2.0f * PI) * BIN_LON), 0, BIN_LON - 1);

    int current = -1;
    float currentDot = -2.0f;
    for (int radius = 0; radius <= BIN_LAT && current < 0; radius++) {
        for (int di = -radius; di <= radius; di++) {
            const int i = ci + di;
            if (i < 0 || i >= BIN_LAT) continue;
            for (int dj = -radius; dj <= radius; dj++) {
                // Only walk the ring, not the filled square, on later passes.
                if (radius > 0 && std::abs(di) != radius && std::abs(dj) != radius) continue;
                const int j = ((cj + dj) % BIN_LON + BIN_LON) % BIN_LON;
                for (int index : bins[i * BIN_LON + j]) {
                    const float d = glm::dot(cellPositions[index], n);
                    if (d > currentDot) {
                        currentDot = d;
                        current = index;
                    }
                }
            }
        }
    }
    if (current < 0) {
        current = 0;
        currentDot = glm::dot(cellPositions[0], n);
    }

    // Step 2: greedy descent over the adjacency graph. Hop to whichever
    // neighbour lies closer to the query until none does. On a geodesic grid
    // this converges to the true nearest cell in a handful of hops, and it is
    // exact regardless of how badly the bin guess started - which is what
    // makes the poles safe.
    for (int iteration = 0; iteration < 128; iteration++) {
        int best = current;
        float bestDot = currentDot;
        for (int k = 0; k < neighbourCount(current); k++) {
            const int j = neighbourAt(current, k);
            const float d = glm::dot(cellPositions[j], n);
            if (d > bestDot) {
                bestDot = d;
                best = j;
            }
        }
        if (best == current) {
            break;
        }
        current = best;
        currentDot = bestDot;
    }

    return current;
}

float CrustGrid::getCellArea() const {
    return 4.0f * PI * planetRadius * planetRadius / static_cast<float>(cells.size());
}

// ============================================================================
// Initial condition
// ============================================================================

void CrustGrid::assignPlates(int plateCount) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    std::uniform_real_distribution<float> angle(0.0f, 2.0f * PI);

    // Plate seeds scattered over the sphere; cells take the nearest seed, so
    // plates come out as spherical Voronoi regions.
    std::vector<glm::vec3> seeds;
    seeds.reserve(plateCount);
    plates.resize(plateCount);

    for (int p = 0; p < plateCount; p++) {
        const float z = unit(rng);
        const float phi = angle(rng);
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        seeds.push_back(glm::vec3(r * std::cos(phi), z, r * std::sin(phi)));

        // A small random nudge, only to break the symmetry. Plate motion is
        // not prescribed - within a few steps the torque balance has taken
        // over entirely and this initial guess is forgotten. It exists because
        // slab pull needs some convergence to already be happening before it
        // can identify which side of a boundary goes down.
        const float pz = unit(rng);
        const float pphi = angle(rng);
        const float pr = std::sqrt(std::max(0.0f, 1.0f - pz * pz));
        const glm::vec3 axis =
            glm::normalize(glm::vec3(pr * std::cos(pphi), pz, pr * std::sin(pphi)));

        std::uniform_real_distribution<float> rate(1.0e-3f, 4.0e-3f);
        std::uniform_real_distribution<float> sign(-1.0f, 1.0f);
        plates[p].omega = axis * (rate(rng) * (sign(rng) < 0.0f ? -1.0f : 1.0f));
    }

    for (Cell& cell : cells) {
        int best = 0;
        float bestDot = -2.0f;
        for (int p = 0; p < plateCount; p++) {
            const float d = glm::dot(cell.position, seeds[p]);
            if (d > bestDot) {
                bestDot = d;
                best = p;
            }
        }
        cell.plateId = static_cast<uint16_t>(best);
    }
}

void CrustGrid::seedInitialCrust() {
    // The one place noise belongs: primordial thickness heterogeneity. Where
    // the young crust happened to be thick, a craton survives and grows; where
    // it was thin, the surface founders and becomes ocean. Everything after
    // this is decided by the simulation.
    const Constants& k = constants;

    // Sample the primordial heterogeneity, then pick the threshold that gives
    // the requested continental area by percentile rather than by guessing a
    // cutoff. The physical input is "how much continental crust exists", which
    // is a measurable quantity; where it happens to sit is the noise's job.
    std::vector<float> heterogeneity(cells.size());
    for (size_t i = 0; i < cells.size(); i++) {
        heterogeneity[i] = smoothSphereNoise(cells[i].position, seed, 1.4f);
    }

    std::vector<float> sorted = heterogeneity;
    std::sort(sorted.begin(), sorted.end());
    const float fraction = glm::clamp(k.initialContinentalFraction, 0.0f, 1.0f);
    const size_t cut = static_cast<size_t>((1.0f - fraction) * (sorted.size() - 1));
    const float threshold = sorted[cut];

    // Width of the continental margin in noise units, so shelves grade into
    // ocean instead of stepping.
    const float spread = std::max(1e-4f, (sorted.back() - sorted.front()) * 0.08f);

    for (size_t i = 0; i < cells.size(); i++) {
        Cell& cell = cells[i];
        const float continentality =
            glm::clamp((heterogeneity[i] - threshold) / spread, 0.0f, 1.0f);

        cell.thickness = glm::mix(k.oceanicThickness, k.continentalThickness, continentality);
        cell.density = glm::mix(k.oceanicDensity, k.continentalDensity, continentality);

        // Oceanic crust starts with a spread of ages so basins are not all
        // uniformly deep at t = 0.
        cell.age = (1.0f - continentality) *
                   (0.5f + 0.5f * smoothSphereNoise(cell.position, seed + 991u, 2.3f)) *
                   k.thermalSubsidenceMaxAge;
    }

    for (size_t p = 0; p < plates.size(); p++) {
        // A plate counts as continental if most of its crust is thick.
        double continental = 0.0;
        double total = 0.0;
        for (const Cell& cell : cells) {
            if (cell.plateId == p) {
                total += 1.0;
                if (cell.thickness > (constants.oceanicThickness + constants.continentalThickness) * 0.5f) {
                    continental += 1.0;
                }
            }
        }
        plates[p].oceanic = total > 0.0 ? (continental / total) < 0.5 : true;
    }
}

// ============================================================================
// Isostasy
// ============================================================================

float CrustGrid::isostaticHeight(const Cell& cell) const {
    const Constants& k = constants;

    // Airy isostasy: a crustal column floating on denser mantle stands above
    // the compensation datum by thickness * (1 - rho_crust / rho_mantle).
    // This is what makes continents high and ocean floor low - the 4-5 km
    // difference is a consequence of crust being thick and light, not a
    // number anybody chose.
    float height = cell.thickness * (1.0f - cell.density / k.mantleDensity);

    // Half-space cooling: oceanic lithosphere thickens and densifies with age,
    // so seafloor subsides as sqrt(age). Continental crust barely does this,
    // so scale the term by how oceanic the column is.
    const float oceanicFraction = glm::clamp(
        (cell.density - k.continentalDensity) / (k.oceanicDensity - k.continentalDensity),
        0.0f, 1.0f);
    const float age = std::min(cell.age, k.thermalSubsidenceMaxAge);
    height -= oceanicFraction * k.thermalSubsidenceRate * std::sqrt(std::max(0.0f, age));

    return height;
}

void CrustGrid::updateIsostasy() {
    for (Cell& cell : cells) {
        cell.elevation = isostaticHeight(cell);
    }
}

void CrustGrid::solveSeaLevel() {
    // Sea level is where the planet's water volume runs out, not a constant.
    // Thicken the continents and the ocean basins shrink, so sea level rises.
    const float cellArea = getCellArea();
    const float targetVolume = constants.oceanWaterGEL * 4.0f * PI * planetRadius * planetRadius;

    float low = std::numeric_limits<float>::max();
    float high = std::numeric_limits<float>::lowest();
    for (const Cell& cell : cells) {
        low = std::min(low, cell.elevation);
        high = std::max(high, cell.elevation);
    }
    // The water could in principle cover everything.
    high += constants.oceanWaterGEL * 2.0f + 1.0f;

    for (int iteration = 0; iteration < 60; iteration++) {
        const float mid = 0.5f * (low + high);
        double volume = 0.0;
        for (const Cell& cell : cells) {
            if (cell.elevation < mid) {
                volume += static_cast<double>(mid - cell.elevation) * cellArea;
            }
        }
        if (volume < targetVolume) {
            low = mid;
        } else {
            high = mid;
        }
    }
    seaLevel = 0.5f * (low + high);

    // Cache the extremes relative to sea level. Sampling code needs these per
    // vertex and must not rescan the grid to get them.
    minElevation = std::numeric_limits<float>::max();
    maxElevation = std::numeric_limits<float>::lowest();
    for (const Cell& cell : cells) {
        const float relative = cell.elevation - seaLevel;
        minElevation = std::min(minElevation, relative);
        maxElevation = std::max(maxElevation, relative);
    }
}

// ============================================================================
// Plate motion
// ============================================================================

glm::vec3 CrustGrid::plateVelocityAt(const glm::vec3& sphereNormal, uint16_t plateId) const {
    if (plateId >= plates.size()) {
        return glm::vec3(0.0f);
    }
    // v = omega x r, with r on the planet surface. Units: metres per My.
    return glm::cross(plates[plateId].omega, glm::normalize(sphereNormal) * planetRadius);
}

float CrustGrid::cellSpacing() const {
    return std::sqrt(getCellArea());
}

void CrustGrid::seedMarkers() {
    // Fill each cell with parcels carrying that cell's crust. From here on the
    // markers are the material and the cell fields are only a projection of
    // them, rebuilt every step.
    const Constants& k = constants;
    const float cellArea = getCellArea();
    const int perCell = std::max(1, k.markersPerCell);
    const float jitter = cellSpacing() / planetRadius * 0.45f;  // radians

    std::mt19937 rng(seed ^ 0x9e3779b9u);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);

    markers.clear();
    markers.reserve(cells.size() * perCell);

    for (const Cell& cell : cells) {
        // A tangent frame at the cell, so parcels can be scattered across its
        // area rather than stacked on the centre.
        const glm::vec3 n = cell.position;
        glm::vec3 tangent = glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::length(tangent) < 1e-4f) {
            tangent = glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        tangent = glm::normalize(tangent);
        const glm::vec3 bitangent = glm::cross(n, tangent);

        // The primordial column is one episode: basalt where the young crust
        // was thin, granite where it was thick enough to have differentiated.
        const RockType primordial = cell.density < k.subductionDensity
            ? RockType::Granite : RockType::Basalt;

        for (int m = 0; m < perCell; m++) {
            Marker marker;
            marker.position = glm::normalize(
                n + tangent * (unit(rng) * jitter) + bitangent * (unit(rng) * jitter));
            marker.plateId = cell.plateId;
            marker.deposit(primordial,
                           static_cast<double>(cell.thickness) * cellArea / perCell,
                           cell.age);
            markers.push_back(marker);
        }
    }
}

void CrustGrid::advectMarkers(float dt) {
    // The whole reason for markers. A parcel is rotated by its plate's motion
    // and that is the entire transport step: exact for any timestep, with no
    // interpolation and therefore no smearing. Plate boundaries move because
    // the parcels either side of them carry their own plate identity, so there
    // is no categorical field to quantise either.
    // Parallel without qualification: each parcel is rotated by its own plate
    // and touches nothing else. The plates are read-only here.
    util::parallelFor(markers.size(), [&](size_t begin, size_t end) {
        for (size_t index = begin; index < end; index++) {
            Marker& marker = markers[index];
            const Plate& plate = plates[marker.plateId];
            const float rate = plate.angularVelocity();
            if (rate > 1e-12f) {
                marker.position = glm::normalize(
                    rotateAbout(marker.position, plate.omega / rate, rate * dt));
            }

            // Every episode in the record ages, not just the column average -
            // otherwise a freshly deposited layer would inherit the mean age of
            // rock beneath it and the stratigraphy would stop meaning anything.
            for (int i = 0; i < marker.layerCount; i++) {
                marker.layers[i].age += dt;
            }
            marker.refresh();
        }
    });
}

void CrustGrid::projectMarkersToGrid() {
    // Gather the parcels back onto the grid, which is what isostasy and the
    // renderer read. This projection does average - but it averages a fresh
    // copy every step and never writes back, so no error accumulates in the
    // material itself.
    const float cellArea = getCellArea();
    const int n = static_cast<int>(cells.size());

    cellMarkers.assign(cells.size(), {});

    std::vector<double> volume(cells.size(), 0.0);
    std::vector<double> mass(cells.size(), 0.0);
    std::vector<double> ageVolume(cells.size(), 0.0);

    // Spread each parcel over its landing cell and that cell's neighbours,
    // with weights summing to one.
    //
    // Counting parcels into whichever cell they happen to land in makes the
    // thickness field a sampling estimate, and its shot noise is large: a cell
    // that happens to catch four parcels instead of six reads a third too thin
    // and the reconcile step rifts it, while its neighbour reads too thick and
    // subducts. That noise, not tectonics, drove crust creation and
    // destruction an order of magnitude too high.
    //
    // Smoothing the readout is safe in a way that smoothing the material never
    // was: the parcels keep their exact positions and compositions, and this
    // projection is thrown away and rebuilt from them every step.
    // Split in two, because this is a scatter: many parcels land on one cell, so
    // the accumulation cannot be done from several threads without either
    // locking every cell or accepting a race, and locking would cost more than
    // the arithmetic it protects.
    //
    // Everything that depends on one parcel alone - the nearest-cell lookup,
    // which is the expensive part, and the inverse square weights - runs in
    // parallel. What is left is adding numbers into cells, which is
    // memory-bound and quick, and runs on one thread.
    projection.resize(markers.size());

    util::parallelFor(markers.size(), [&](size_t begin, size_t end) {
        for (size_t index = begin; index < end; index++) {
            const Marker& marker = markers[index];
            Projection& out = projection[index];
            out.count = 0;
            out.landing = findNearestCell(marker.position);
            if (out.landing < 0) {
                continue;
            }

            int stencil[Projection::MAX];
            double weights[Projection::MAX];
            int used = 0;
            double weightTotal = 0.0;

            const auto consider = [&](int cell) {
                if (used >= Projection::MAX) {
                    return;
                }
                const float cosAngle =
                    glm::clamp(glm::dot(cells[cell].position, marker.position), -1.0f, 1.0f);
                const float angle = std::acos(cosAngle);
                const double weight = 1.0 / (static_cast<double>(angle) * angle + 1e-9);
                stencil[used] = cell;
                weights[used] = weight;
                used++;
                weightTotal += weight;
            };

            consider(out.landing);
            for (int m = 0; m < neighbourCount(out.landing); m++) {
                consider(neighbourAt(out.landing, m));
            }
            if (weightTotal <= 0.0) {
                continue;
            }

            for (int s = 0; s < used; s++) {
                out.cells[s] = stencil[s];
                out.shares[s] = static_cast<double>(marker.volume) * (weights[s] / weightTotal);
            }
            out.count = used;
        }
    });

    for (size_t index = 0; index < markers.size(); index++) {
        const Projection& out = projection[index];
        if (out.landing < 0) {
            continue;
        }

        // Ownership stays with the nearest cell, so reconcileCrust knows which
        // parcels to consume where.
        cellMarkers[out.landing].push_back(static_cast<int>(index));

        const Marker& marker = markers[index];
        for (int s = 0; s < out.count; s++) {
            const int cell = out.cells[s];
            const double share = out.shares[s];
            volume[cell] += share;
            mass[cell] += share * marker.density;
            ageVolume[cell] += share * marker.age;
        }
    }

    std::vector<float> plateVolume(plates.size(), 0.0f);

    for (int i = 0; i < n; i++) {
        Cell& cell = cells[i];
        if (volume[i] <= 0.0) {
            // No crust here at all. Thickness zero tells reconcileCrust that a
            // hole has opened and melt has to fill it.
            cell.thickness = 0.0f;
            continue;
        }

        cell.thickness = static_cast<float>(volume[i] / cellArea);
        cell.density = glm::clamp(static_cast<float>(mass[i] / volume[i]),
                                  constants.continentalDensity, constants.oceanicDensity);
        cell.age = static_cast<float>(ageVolume[i] / volume[i]);

        // Plate membership goes to whichever plate holds the most crust here.
        // Voting by volume rather than averaging keeps it categorical, which
        // is what it is.
        std::fill(plateVolume.begin(), plateVolume.end(), 0.0f);
        for (int markerIndex : cellMarkers[i]) {
            const Marker& marker = markers[markerIndex];
            if (marker.plateId < plateVolume.size()) {
                plateVolume[marker.plateId] += marker.volume;
            }
        }
        int winner = 0;
        float best = -1.0f;
        for (size_t p = 0; p < plateVolume.size(); p++) {
            if (plateVolume[p] > best) {
                best = plateVolume[p];
                winner = static_cast<int>(p);
            }
        }
        cell.plateId = static_cast<uint16_t>(winner);
    }
}

void CrustGrid::reconcileCrust(float dt) {
    (void)dt;
    const Constants& k = constants;
    const float cellArea = getCellArea();
    const int n = static_cast<int>(cells.size());

    double toMantle = 0.0;
    double fromMantle = 0.0;
    std::vector<float> arcPending(cells.size(), 0.0f);

    for (int i = 0; i < n; i++) {
        Cell& cell = cells[i];

        if (cell.thickness <= 0.0f) {
            // The plates opened a hole. Mantle melt floods it and freezes as
            // new basaltic seafloor - this is a spreading ridge, and it exists
            // because transport left a gap, not because anything looked for it.
            Marker fresh;
            fresh.position = cell.position;
            fresh.plateId = cell.plateId;
            fresh.deposit(RockType::Basalt,
                          static_cast<double>(k.oceanicThickness) * cellArea, 0.0f);
            markers.push_back(fresh);

            fromMantle += static_cast<double>(fresh.volume);
            crustBudget.meltFromMantle += static_cast<double>(fresh.volume);
            cell.thickness = k.oceanicThickness;
            cell.density = k.oceanicDensity;
            cell.age = 0.0f;
            continue;
        }

        const bool buoyant = cell.density < k.subductionDensity;

        if (cell.thickness < k.oceanicThickness) {
            // Stretched thinner than crust can be: melt tops it up.
            const float deficit = k.oceanicThickness - cell.thickness;
            if (buoyant) {
                continentalLostToRifting += static_cast<double>(cell.thickness) * cellArea;
                crustBudget.riftedAway += static_cast<double>(cell.thickness) * cellArea;
            }

            Marker fresh;
            fresh.position = cell.position;
            fresh.plateId = cell.plateId;
            fresh.deposit(RockType::Basalt, static_cast<double>(deficit) * cellArea, 0.0f);
            markers.push_back(fresh);

            fromMantle += static_cast<double>(fresh.volume);
            crustBudget.meltFromMantle += static_cast<double>(fresh.volume);
            cell.thickness = k.oceanicThickness;
            continue;
        }

        // How much crust this column can support. Dense ocean floor founders
        // rather than stacking, which is why trenches stay deep; buoyant crust
        // cannot be pulled under and piles into an orogen instead.
        //
        // This has to vary smoothly with composition. A hard switch at the
        // subduction density means a column whose density drifts a hair across
        // the threshold has its capacity collapse from 70 km to 11 km, and the
        // crust above that is removed in a single step - a five kilometre
        // change in surface height, which is the isostatic difference between
        // continent and ocean floor appearing all at once. Watching the planet
        // it reads as landmasses flickering in and out of existence.
        const float buoyancy = glm::clamp(
            (k.oceanicDensity - cell.density) / (k.oceanicDensity - k.continentalDensity),
            0.0f, 1.0f);
        const float capacity = glm::mix(k.oceanicThickness * 1.6f, k.maxCrustThickness,
                                        buoyancy * buoyancy);
        if (cell.thickness <= capacity) {
            continue;
        }

        const float excess = cell.thickness - capacity;
        const double excessVolume = static_cast<double>(excess) * cellArea;

        // Take the excess out of the parcels in this cell, densest first: it
        // is the heavy rock that founders, and the buoyant rock that stays.
        std::vector<int> order = cellMarkers[i];
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return markers[a].density > markers[b].density;
        });

        double remaining = excessVolume;
        for (int markerIndex : order) {
            if (remaining <= 0.0) {
                break;
            }
            Marker& marker = markers[markerIndex];
            // Which end of the record goes matters. An over-thickened orogen
            // sheds its root: the deepest rock turns to eclogite and founders,
            // leaving the young rock at the surface untouched. A subducting
            // slab descends entire, so it is consumed through its whole
            // thickness at once.
            remaining -= buoyant ? marker.removeFromBottom(remaining)
                                 : marker.consumeProportionally(remaining);
        }

        crustBudget.shedEvents++;
        crustBudget.excessThickness += static_cast<double>(excess);
        if (excess < capacity * 0.05f) {
            crustBudget.marginalEvents++;
            crustBudget.marginalVolume += excessVolume;
        }

        const double consumed = excessVolume - std::max(0.0, remaining);
        toMantle += consumed;
        if (buoyant) {
            continentalLostToDelamination += consumed;
            crustBudget.delaminated += consumed;
        } else {
            crustBudget.subducted += consumed;
            arcPending[i] = static_cast<float>(consumed * k.arcProductionRatio);
        }
        cell.thickness = capacity;
    }

    // Emplace arc crust on the most buoyant neighbour of each subducting cell,
    // which is where the volcanic arc builds on the overriding plate.
    for (int i = 0; i < n; i++) {
        if (arcPending[i] <= 0.0f) {
            continue;
        }
        int target = i;
        float lowestDensity = cells[i].density;
        for (int m = 0; m < neighbourCount(i); m++) {
            const int j = neighbourAt(i, m);
            if (cells[j].density < lowestDensity) {
                lowestDensity = cells[j].density;
                target = j;
            }
        }

        Marker arc;
        arc.position = cells[target].position;
        arc.plateId = cells[target].plateId;
        arc.deposit(RockType::Andesite, static_cast<double>(arcPending[i]), 0.0f);
        markers.push_back(arc);

        fromMantle += static_cast<double>(arc.volume);
        continentalCreatedByArcs += static_cast<double>(arc.volume);
        crustBudget.arcFromMantle += static_cast<double>(arc.volume);
        cells[target].thickness += arc.volume / cellArea;
    }

    mantleReservoir += toMantle - fromMantle;
}

void CrustGrid::resolvePlateOverlap(float dt) {
    // Two plates cannot occupy the same ground.
    //
    // Rigid plates rotating independently have nothing stopping them from
    // being carried onto each other, and the thickness cap alone does not
    // help: two thirty-five kilometre continents overlapping come to seventy,
    // which is exactly the limit, so they pass straight through one another.
    // On screen that reads as a landmass sliding across another one.
    //
    // What actually happens at a convergent margin is that the relative motion
    // is taken up. The denser slab descends and is consumed; buoyant crust
    // cannot be pulled under, so it docks onto the plate that holds the ground
    // and travels with it from then on. That second case is terrane accretion,
    // and it is how most continents grew their margins.
    const Constants& k = constants;
    const int n = static_cast<int>(cells.size());
    const float cellArea = getCellArea();
    const float rate = glm::clamp(dt / std::max(0.01f, k.overlapResolutionTime), 0.0f, 1.0f);

    double subducted = 0.0;
    std::vector<float> arcPending(cells.size(), 0.0f);

    for (int i = 0; i < n; i++) {
        if (cellMarkers[i].size() < 2) {
            continue;
        }

        // Who holds this ground, by volume.
        std::vector<std::pair<uint16_t, double>> byPlate;
        for (int index : cellMarkers[i]) {
            const Marker& marker = markers[index];
            auto it = std::find_if(byPlate.begin(), byPlate.end(),
                                   [&](const auto& e) { return e.first == marker.plateId; });
            if (it == byPlate.end()) {
                byPlate.emplace_back(marker.plateId, marker.volume);
            } else {
                it->second += marker.volume;
            }
        }
        if (byPlate.size() < 2) {
            continue;
        }

        uint16_t owner = byPlate.front().first;
        double largest = byPlate.front().second;
        for (const auto& [plate, volume] : byPlate) {
            if (volume > largest) {
                largest = volume;
                owner = plate;
            }
        }

        // Mean density of the crust that holds this ground, to decide which
        // side of the margin goes down.
        double ownerMass = 0.0, ownerVolume = 0.0;
        for (int index : cellMarkers[i]) {
            if (markers[index].plateId == owner) {
                ownerMass += markers[index].volume * markers[index].density;
                ownerVolume += markers[index].volume;
            }
        }
        const double ownerDensity = ownerVolume > 0.0 ? ownerMass / ownerVolume
                                                      : k.oceanicDensity;

        for (int index : cellMarkers[i]) {
            Marker& marker = markers[index];
            if (marker.plateId == owner) {
                continue;
            }

            if (marker.density > ownerDensity + 1.0f) {
                // Denser: this is the down-going side. Consume it gradually -
                // a slab takes time to descend, and removing it all at once
                // drops the surface by the whole continent-ocean step in a
                // single frame.
                const double consumed = marker.consumeProportionally(marker.volume * rate);
                subducted += consumed;
                crustBudget.subducted += consumed;
                arcPending[i] += static_cast<float>(consumed * k.arcProductionRatio);
            } else {
                // Buoyant: it cannot subduct, so it docks. The ground is now
                // part of the plate that was already here.
                marker.plateId = owner;
            }
        }
    }

    double fromMantle = 0.0;
    for (int i = 0; i < n; i++) {
        if (arcPending[i] <= 0.0f) {
            continue;
        }
        Marker arc;
        arc.position = cellPositions[i];
        arc.plateId = cells[i].plateId;
        arc.deposit(RockType::Andesite, static_cast<double>(arcPending[i]), 0.0f);
        markers.push_back(arc);
        fromMantle += arc.volume;
        continentalCreatedByArcs += arc.volume;
        crustBudget.arcFromMantle += arc.volume;
    }

    mantleReservoir += subducted - fromMantle;
    (void)cellArea;
}

void CrustGrid::rebalanceMarkers() {
    // Convergence crowds parcels together and divergence spreads them out, so
    // without maintenance the population drifts. Merge where a cell is
    // overcrowded and drop parcels that have been consumed to nothing.
    const Constants& k = constants;
    const int maxPerCell = std::max(2, k.maxMarkersPerCell);

    std::vector<Marker> kept;
    kept.reserve(markers.size());
    std::vector<bool> consumed(markers.size(), false);

    for (size_t i = 0; i < cellMarkers.size(); i++) {
        std::vector<int>& here = cellMarkers[i];
        if (static_cast<int>(here.size()) <= maxPerCell) {
            continue;
        }

        // Merge the smallest parcels first - they carry the least history, so
        // combining them loses the least.
        std::sort(here.begin(), here.end(), [&](int a, int b) {
            return markers[a].volume < markers[b].volume;
        });

        const int surplus = static_cast<int>(here.size()) - maxPerCell;
        for (int s = 0; s < surplus; s++) {
            const int from = here[s];
            const int into = here[here.size() - 1 - (s % maxPerCell)];
            // Never merge into a parcel that has itself already been merged
            // away, or its volume is written into a marker that is about to be
            // dropped and the crust goes missing.
            if (from == into || consumed[from] || consumed[into]) {
                continue;
            }
            Marker& source = markers[from];
            Marker& sink = markers[into];
            if (source.volume + sink.volume <= 0.0) {
                continue;
            }
            // Pour the source's record onto the sink, oldest episode first, so
            // the merged column keeps its rock types and stacking order rather
            // than dissolving into an average.
            for (int layerIndex = 0; layerIndex < source.layerCount; layerIndex++) {
                const Layer& layer = source.layers[layerIndex];
                sink.deposit(layer.rock, layer.volume, layer.age);
            }
            consumed[from] = true;
        }
    }

    // Parcels worn down to nothing by subduction are retired. Their remaining
    // sliver still has to be booked - dropping it silently is a leak, small
    // per parcel but relentless across a few hundred thousand of them.
    double retired = 0.0;
    for (size_t i = 0; i < markers.size(); i++) {
        if (consumed[i]) {
            continue;
        }
        if (markers[i].volume > 1.0) {
            kept.push_back(markers[i]);
        } else {
            retired += markers[i].volume;
        }
    }
    mantleReservoir += retired;
    markers.swap(kept);
}



void CrustGrid::step(float millionYears) {
    if (millionYears <= 0.0f) {
        return;
    }

    // Transport itself is exact at any timestep, but the boundary processes
    // are not: a plate crossing several cells in one go has its trenches and
    // ridges sampled only where it happens to land. Split the request into
    // sub-steps that keep plates to half a cell each.
    //
    // Note this bites hardest on small planets. Plate speed in metres per year
    // is set by mantle properties and barely depends on planet size, so a
    // thousand-kilometre world crosses its own circumference far faster than
    // Earth does and needs proportionally finer steps.
    const float stable = maxStableTimestep();
    int subSteps = 1;
    if (stable > 0.0f && millionYears > stable) {
        subSteps = std::min(32, static_cast<int>(std::ceil(millionYears / stable)));
    }
    if (subSteps > 1) {
        const float slice = millionYears / static_cast<float>(subSteps);
        for (int s = 0; s < subSteps; s++) {
            stepOnce(slice);
        }
        return;
    }

    stepOnce(millionYears);
}

void CrustGrid::stepOnce(float millionYears) {
    const double continentalBefore = computeContinentalVolume();

    // Solve what the forces want the plates to be doing before moving anything.
    // Timed per phase. Measured in a running simulation rather than in a
    // microbenchmark, because what matters is the cost with the real marker
    // population and the real plate layout.
    using Clock = std::chrono::steady_clock;
    const auto stepBegan = Clock::now();
    auto mark = Clock::now();
    const auto lap = [&mark]() {
        const auto now = Clock::now();
        const float ms = std::chrono::duration<float, std::milli>(now - mark).count();
        mark = now;
        return ms;
    };

    updatePlateMotion(millionYears);
    timings.plateMotion = lap();

    // Every so often, ask whether the plate layout itself should change.
    // Boundaries do not rearrange every few hundred thousand years, and the
    // search costs more than a step does.
    sinceReorganisation += millionYears;
    if (sinceReorganisation >= constants.reorganisationInterval) {
        sinceReorganisation = 0.0f;
        reorganisePlates();
    }

    // Crust is carried by the parcels; the grid is rebuilt from them each step
    // rather than being evolved in place, so transport error cannot accumulate.
    // The phases that move the crust about run on their own schedule, and are
    // given the time they have accumulated rather than the time of this step.
    //
    // At a thousand years a step a plate moves sixty metres and a cell is
    // seventeen kilometres, so advecting every step computes a third of a per
    // cent of a cell - for half of what the whole step costs. Advecting once
    // per accumulated interval puts the parcels in exactly the same place for
    // a fraction of the work, and it is the same arithmetic either way: the
    // parcels move by elapsed time, and elapsed time is elapsed time.
    tectonicDebt += millionYears;

    // Forced on the first pass however short the step is. The projection is
    // what fills the cell-to-parcel index, and erosion reads that index - so
    // skipping it before it has ever run leaves erosion reading an array that
    // does not exist yet.
    const bool projectionMissing = cellMarkers.size() != cells.size();
    const bool moveCrust = projectionMissing || tectonicDebt >= constants.tectonicInterval;
    const float crustDt = tectonicDebt;

    if (moveCrust) {
        tectonicDebt = 0.0f;
        advectMarkers(crustDt);
        projectMarkersToGrid();
    }
    timings.advection = lap();
    continentalDeltaTransport += computeContinentalVolume() - continentalBefore;

    if (moveCrust) {
        reconcileCrust(crustDt);
        resolvePlateOverlap(crustDt);
    }
    timings.reconcile = lap();

    // Isostasy before erosion, because rivers need to know which way is
    // downhill, and that is decided by how the columns float.
    updateIsostasy();
    solveSeaLevel();
    timings.isostasy = lap();

    // Continents move slowly, so the climate they produce changes slowly too.
    // Resolving it every sub-step would cost as much as the tectonics and
    // change almost nothing between them.
    climateAge += millionYears;
    if (climateAge >= constants.climateInterval) {
        climateAge = 0.0f;
        climate.update();
    }
    timings.climate = lap();

    // Erosion at whichever fidelity this step length can actually resolve, and
    // on its own schedule rather than the caller's.
    if (millionYears <= constants.routedErosionBelow) {
        erosionDebt += millionYears;
        if (erosionDebt >= constants.erosionInterval ||
            lastFlowsInto.size() != cells.size()) {
            const float owed = erosionDebt;
            erosionDebt = 0.0f;
            erodeSurface(owed);
        }
    } else {
        erodeBulk(millionYears);
    }
    timings.erosion = lap();

    if (moveCrust) {
        rebalanceMarkers();
    }
    updateIsostasy();

    // Watch for the surface moving faster than any process should move it.
    if (previousElevation.size() == cells.size()) {
        for (size_t i = 0; i < cells.size(); i++) {
            const float jump = std::fabs(cells[i].elevation - previousElevation[i]);
            if (jump > largestElevationJump) {
                largestElevationJump = jump;
                largestJumpCell = static_cast<int>(i);
            }
        }
    }
    previousElevation.resize(cells.size());
    for (size_t i = 0; i < cells.size(); i++) {
        previousElevation[i] = cells[i].elevation;
    }

    timings.rebalance = lap();

    solveSeaLevel();

    // The drainage network, rebuilt here at the end of the step rather than in
    // the middle of it.
    //
    // A network is only meaningful against the elevations it was routed on, and
    // isostasy and the sea level solve both move elevations after erosion runs.
    // Built earlier, the published network described a surface that no longer
    // existed by the time anything read it - which showed up as cells draining
    // uphill on dry ground, eight per cent of them, for no reason visible
    // anywhere in the routing.
    //
    // Only needed when the routed model did not already do it, and only on its
    // own schedule: a river system outlives a couple of million years, and this
    // costs one erosion pass with the incision skipped.
    if (millionYears > constants.routedErosionBelow) {
        networkAge += millionYears;
        if (networkAge >= constants.networkInterval ||
            lastFlowsInto.size() != cells.size()) {
            networkAge = 0.0f;
            erodeSurface(millionYears, true);
        }
    }

    // A channel cannot be deeper than the ground it is cut into, applied here
    // because here is the only point in the step where the ground has stopped
    // moving.
    //
    // Erosion clamps it too, but isostasy and the tectonics run afterwards and
    // move the elevations again, so a limit applied during erosion is already
    // stale by the time anything reads it. That was invisible while erosion ran
    // every step and each step's drift was tiny; running it on its own schedule
    // made the drift large enough to show.
    if (channelDepth.size() == cells.size()) {
        for (size_t i = 0; i < cells.size(); i++) {
            const float aboveSea = cells[i].elevation - seaLevel;
            channelDepth[i] = std::min(channelDepth[i], std::max(0.0f, aboveSea));
        }
    }

    refreshElevationField();
    timings.gradients = lap();
    timings.total = std::chrono::duration<float, std::milli>(Clock::now() - stepBegan).count();

    erosionBudget.simulatedTime += millionYears;
    crustBudget.simulatedTime += millionYears;
    simulationTime += millionYears;
    version++;
}

// ============================================================================
// Sampling and diagnostics
// ============================================================================

std::shared_ptr<const CrustGrid::Snapshot> CrustGrid::publishSnapshot() const {
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->elevation.resize(cells.size());
    snapshot->elevationGradient.resize(cells.size());
    snapshot->plateId.resize(cells.size());
    snapshot->crustAge.resize(cells.size());
    snapshot->crustThickness.resize(cells.size());
    snapshot->surfaceRock.resize(cells.size());
    snapshot->cloudCover = climate.getFields().cloudCover;
    snapshot->cloudCover.resize(cells.size(), 0.0f);
    snapshot->discharge = lastDischarge;
    snapshot->discharge.resize(cells.size(), 0.0f);
    snapshot->flowsInto = lastFlowsInto;
    snapshot->flowsInto.resize(cells.size(), -1);
    snapshot->lakeDepth = lastLakeDepth;
    snapshot->lakeDepth.resize(cells.size(), 0.0f);
    snapshot->channelDepth = channelDepth;
    snapshot->channelDepth.resize(cells.size(), 0.0f);
    snapshot->channelPoint = lastChannelPoint;
    snapshot->channelPoint.resize(cells.size(), glm::vec3(0.0f));

    snapshot->temperature = climate.getFields().temperature;
    snapshot->temperature.resize(cells.size(), 15.0f);

    snapshot->crustOmega.resize(cells.size(), glm::vec3(0.0f));
    for (size_t i = 0; i < cells.size(); i++) {
        const uint16_t plate = cells[i].plateId;
        snapshot->crustOmega[i] =
            plate < plates.size() ? plates[plate].omega : glm::vec3(0.0f);
    }
    snapshot->routedSurface = lastRoutedSurface;
    snapshot->routedSurface.resize(cells.size(), 0.0f);

    for (size_t i = 0; i < cells.size(); i++) {
        snapshot->elevation[i] = cells[i].elevation - seaLevel;
        snapshot->plateId[i] = cells[i].plateId;
        snapshot->crustAge[i] = cells[i].age;
        snapshot->crustThickness[i] = cells[i].thickness;

        // Whatever rock is exposed at the top of the biggest parcel here.
        uint8_t rock = static_cast<uint8_t>(RockType::Basalt);
        double best = 0.0;
        if (i < cellMarkers.size()) {
            for (int index : cellMarkers[i]) {
                const Marker& marker = markers[index];
                if (marker.layerCount > 0 && marker.volume > best) {
                    best = marker.volume;
                    rock = static_cast<uint8_t>(marker.layers[marker.layerCount - 1].rock);
                }
            }
        }
        snapshot->surfaceRock[i] = rock;
    }
    // Subtracting sea level shifts every cell by the same amount, and a
    // constant offset does not change a slope - so the gradients already
    // fitted for the live field describe the snapshot's elevations too.
    snapshot->elevationGradient = elevationGradient;

    snapshot->minElevation = minElevation;
    snapshot->maxElevation = maxElevation;
    snapshot->seaLevel = seaLevel;
    snapshot->simulationTime = simulationTime;
    snapshot->version = version;
    return snapshot;
}

float CrustGrid::sampleElevation(const Snapshot& snapshot, const glm::vec3& sphereNormal) const {
    if (snapshot.elevation.empty()) {
        return 0.0f;
    }

    // Reads only the snapshot and the fixed topology, so the simulation
    // thread can be mid-step and several renderer threads can be in here at
    // once.
    return reconstruct(sphereNormal, snapshot.elevation, snapshot.elevationGradient);
}

float CrustGrid::sampleElevation(const glm::vec3& sphereNormal) const {
    // Reconstruct between cell centres, not at them. The grid is a
    // discretisation of the crust, not a mosaic the crust is made of, so the
    // surface between two cells has to be the blend of them.
    //
    // Weighting the cell and its ring by inverse square angular distance was
    // meant to do this and very nearly does, but the weight is singular at a
    // cell centre: approach one and it dominates the sum completely, so each
    // cell ends up surrounded by a plateau of its own value with a step at the
    // edge. That is the honeycomb this was written to avoid, an interpolation
    // scheme later rather than a nearest-cell lookup. Barycentric weights over
    // the containing triangle are linear between corners and have no such
    // singularity.
    if (elevationField.size() != cells.size()) {
        return -seaLevel;
    }
    return reconstruct(sphereNormal, elevationField, elevationGradient) - seaLevel;
}

CrustGrid::Stats CrustGrid::computeStats() const {
    Stats stats;
    if (cells.empty()) {
        return stats;
    }

    double elevationSum = 0.0;
    double ageSum = 0.0;
    double volume = 0.0;
    double continentalVolume = 0.0;
    double oceanicVolume = 0.0;
    int land = 0;
    int oceanic = 0;
    stats.minElevation = std::numeric_limits<float>::max();
    stats.maxElevation = std::numeric_limits<float>::lowest();

    const float cellArea = getCellArea();
    const float oceanicCut = (constants.oceanicThickness + constants.continentalThickness) * 0.5f;

    for (const Cell& cell : cells) {
        const float relative = cell.elevation - seaLevel;
        elevationSum += relative;
        stats.minElevation = std::min(stats.minElevation, relative);
        stats.maxElevation = std::max(stats.maxElevation, relative);
        if (relative > 0.0f) {
            land++;
        }
        if (cell.thickness < oceanicCut) {
            ageSum += cell.age;
            oceanic++;
        }
        const double cellVolume = static_cast<double>(cell.thickness) * cellArea;
        volume += cellVolume;
        if (cell.density >= constants.subductionDensity) {
            oceanicVolume += cellVolume;
        } else {
            continentalVolume += cellVolume;
        }
    }

    stats.landFraction = static_cast<float>(land) / static_cast<float>(cells.size());
    stats.meanElevation = static_cast<float>(elevationSum / static_cast<double>(cells.size()));
    stats.meanOceanicAge = oceanic > 0 ? static_cast<float>(ageSum / oceanic) : 0.0f;
    // Volumes come from the parcels, which are the material; the cell sums
    // above only describe the projection.
    (void)volume;
    (void)continentalVolume;
    (void)oceanicVolume;
    stats.crustVolume = static_cast<float>(computeCrustVolume());
    stats.continentalVolume = static_cast<float>(computeContinentalVolume());
    stats.oceanicVolume = stats.crustVolume - stats.continentalVolume;
    return stats;
}

} // namespace simulation
