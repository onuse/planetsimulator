#include "simulation/crust_grid.hpp"

#include <algorithm>
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

CrustGrid::CrustGrid(float planetRadius, uint32_t seed, int subdivisions, int plateCount)
    : planetRadius(planetRadius), seed(seed) {
    buildGeodesicGrid(subdivisions);
    buildAccelerator();
    assignPlates(plateCount);
    seedInitialCrust();
    updateIsostasy();
    solveSeaLevel();
    initialCrustVolume = computeCrustVolume();
}

double CrustGrid::computeCrustVolume() const {
    const double cellArea = getCellArea();
    double volume = 0.0;
    for (const Cell& cell : cells) {
        volume += static_cast<double>(cell.thickness) * cellArea;
    }
    return volume;
}

double CrustGrid::computeContinentalVolume() const {
    const double cellArea = getCellArea();
    double volume = 0.0;
    for (const Cell& cell : cells) {
        if (cell.density < constants.subductionDensity) {
            volume += static_cast<double>(cell.thickness) * cellArea;
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
    for (size_t i = 0; i < verts.size(); i++) {
        cells[i].position = verts[i];
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
    if (cells.empty()) {
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
                    const float d = glm::dot(cells[index].position, n);
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
        currentDot = glm::dot(cells[0].position, n);
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
            const float d = glm::dot(cells[j].position, n);
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

        const float pz = unit(rng);
        const float pphi = angle(rng);
        const float pr = std::sqrt(std::max(0.0f, 1.0f - pz * pz));
        plates[p].eulerPole = glm::normalize(glm::vec3(pr * std::cos(pphi), pz, pr * std::sin(pphi)));

        // Earth's plates move a few cm/yr, which is a few tens of km per My.
        // As an angular rate that is ~5e-3 rad/My and is radius-independent.
        std::uniform_real_distribution<float> rate(2.0e-3f, 9.0e-3f);
        std::uniform_real_distribution<float> sign(-1.0f, 1.0f);
        plates[p].angularVelocity = rate(rng) * (sign(rng) < 0.0f ? -1.0f : 1.0f);
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
    const Plate& plate = plates[plateId];
    // v = omega x r, with r on the planet surface. Units: metres per My.
    const glm::vec3 omega = plate.eulerPole * plate.angularVelocity;
    return glm::cross(omega, glm::normalize(sphereNormal) * planetRadius);
}

float CrustGrid::cellSpacing() const {
    return std::sqrt(getCellArea());
}

void CrustGrid::transportCrust(float dt) {
    const Constants& k = constants;
    const float cellArea = getCellArea();
    const int n = static_cast<int>(cells.size());

    // Plates move by this step's rotation, however small. Weighted scatter
    // handles sub-cell displacement naturally - the weights shift a little and
    // a little material flows - so there is no need to bank motion until it is
    // worth a whole cell. Banking it made convergent margins receive an entire
    // column in one jump, doubling their crust instantly and blowing straight
    // through the delamination limit, which is orogeny by discretisation
    // rather than by tectonics.

    // Forward scatter. Each column is carried by its plate to wherever the
    // plate takes it and deposited there, spread over the destination
    // neighbourhood by weights that sum to one.
    //
    // This is the opposite of asking each cell where its material came from,
    // and the difference matters: a backward gather has no way to guarantee
    // that every column is collected exactly once, so material silently
    // vanishes at margins - which is what was dissolving the continents. Here
    // conservation is structural. Every column is deposited, once, in full.
    //
    // It also means ridges and trenches stop being things we detect. Where
    // plates pull apart, less material arrives than left, and the column
    // thins. Where they converge, more arrives than can stand, and the excess
    // founders. Spreading and subduction are what transport does, not rules
    // laid on top of it.
    std::vector<double> volume(cells.size(), 0.0);
    std::vector<double> mass(cells.size(), 0.0);
    std::vector<double> ageVolume(cells.size(), 0.0);
    std::vector<double> plateWeight(cells.size(), 0.0);
    std::vector<uint16_t> plateVote(cells.size(), 0);

    std::vector<int> stencil;
    std::vector<double> weights;
    stencil.reserve(8);
    weights.reserve(8);

    for (int i = 0; i < n; i++) {
        const Cell& cell = cells[i];
        const Plate& plate = plates[cell.plateId];

        const glm::vec3 destination =
            rotateAbout(cell.position, plate.eulerPole, plate.angularVelocity * dt);

        const int landing = findNearestCell(destination);
        if (landing < 0) {
            continue;
        }

        // Spread over the landing cell and its neighbours so that a rotation
        // of a fraction of a cell does not quantise into spurious holes and
        // pile-ups across the interior of an otherwise rigid plate.
        stencil.clear();
        weights.clear();
        double weightTotal = 0.0;

        const auto consider = [&](int index) {
            const float cosAngle =
                glm::clamp(glm::dot(cells[index].position, destination), -1.0f, 1.0f);
            const float angle = std::acos(cosAngle);
            const double weight = 1.0 / (static_cast<double>(angle) * angle + 1e-9);
            stencil.push_back(index);
            weights.push_back(weight);
            weightTotal += weight;
        };

        consider(landing);
        for (int m = 0; m < neighbourCount(landing); m++) {
            consider(neighbourAt(landing, m));
        }
        if (weightTotal <= 0.0) {
            continue;
        }

        const double columnVolume = cell.thickness;
        for (size_t s = 0; s < stencil.size(); s++) {
            const int j = stencil[s];
            const double fraction = weights[s] / weightTotal;
            const double share = columnVolume * fraction;

            volume[j] += share;
            mass[j] += share * cell.density;
            ageVolume[j] += share * cell.age;

            // Plate identity is categorical, so it is voted on by mass rather
            // than averaged - averaging plate numbers is meaningless.
            if (share > plateWeight[j]) {
                plateWeight[j] = share;
                plateVote[j] = cell.plateId;
            }
        }
    }

    double toMantle = 0.0;
    double fromMantle = 0.0;
    std::vector<float> arcPending(cells.size(), 0.0f);

    for (int i = 0; i < n; i++) {
        Cell& cell = cells[i];

        if (volume[i] <= 0.0) {
            // Nothing arrived at all. The plates opened a hole here, and it
            // fills with mantle melt.
            fromMantle += static_cast<double>(k.oceanicThickness) * cellArea;
            cell.thickness = k.oceanicThickness;
            cell.density = k.oceanicDensity;
            cell.age = 0.0f;
            continue;
        }

        float thickness = static_cast<float>(volume[i]);
        float density = static_cast<float>(mass[i] / volume[i]);
        float age = static_cast<float>(ageVolume[i] / volume[i]);
        cell.plateId = plateVote[i];

        density = glm::clamp(density, k.continentalDensity, k.oceanicDensity);

        // How much crust this column can support. Dense ocean floor founders
        // rather than stacking, which is why trenches are deep and narrow;
        // buoyant crust cannot be pulled under and piles into an orogen.
        const bool buoyant = density < k.subductionDensity;
        const float capacity = buoyant ? k.maxCrustThickness : k.oceanicThickness * 1.6f;

        if (thickness > capacity) {
            const float excess = thickness - capacity;
            toMantle += static_cast<double>(excess) * cellArea;
            if (buoyant) {
                continentalLostToDelamination += static_cast<double>(excess) * cellArea;
            } else {
                // A descending slab dehydrates and melts the wedge above it.
                // The arc it builds is the only source of new continental
                // crust; without it continents can only ever shrink.
                arcPending[i] = excess * k.arcProductionRatio;
            }
            thickness = capacity;
        }

        if (thickness < k.oceanicThickness) {
            // The column was stretched thinner than crust can be. Mantle melt
            // floods the gap and freezes as new basaltic seafloor.
            if (buoyant) {
                continentalLostToRifting += static_cast<double>(thickness) * cellArea;
            }
            fromMantle += static_cast<double>(k.oceanicThickness - thickness) * cellArea;
            thickness = k.oceanicThickness;
            density = k.oceanicDensity;
            age = 0.0f;
        }

        cell.thickness = thickness;
        cell.density = density;
        cell.age = age + dt;
    }

    // Emplace arc crust on the most buoyant neighbour of each subducting cell,
    // which is where the volcanic arc actually builds on the overriding plate.
    for (int i = 0; i < n; i++) {
        if (arcPending[i] <= 0.0f) {
            continue;
        }
        int target = -1;
        float lowestDensity = cells[i].density;
        for (int m = 0; m < neighbourCount(i); m++) {
            const int j = neighbourAt(i, m);
            if (cells[j].density < lowestDensity) {
                lowestDensity = cells[j].density;
                target = j;
            }
        }
        if (target < 0) {
            target = i;
        }

        Cell& receiver = cells[target];
        const float arc = arcPending[i];
        const float newThickness = std::min(receiver.thickness + arc, k.maxCrustThickness);
        const float added = newThickness - receiver.thickness;
        if (added <= 0.0f) {
            continue;
        }

        // Andesite, so the column gets lighter in proportion to how much of it
        // arrived, and its thermal clock partly resets.
        const float totalMass = receiver.thickness * receiver.density + added * k.continentalDensity;
        receiver.thickness = newThickness;
        receiver.density = glm::clamp(totalMass / newThickness,
                                      k.continentalDensity, k.oceanicDensity);
        receiver.age *= (1.0f - glm::clamp(added / newThickness, 0.0f, 1.0f));

        fromMantle += static_cast<double>(added) * cellArea;
        continentalCreatedByArcs += static_cast<double>(added) * cellArea;
    }

    mantleReservoir += toMantle - fromMantle;
}

void CrustGrid::migratePlateBoundaries(float dt) {
    // Plate membership is categorical, so it cannot be scattered with weights
    // the way thickness is - averaging plate numbers is meaningless, and a
    // sub-cell rotation would leave every column voting for itself and the
    // boundaries frozen in place forever. Bank the rotation instead and move
    // the boundary a whole cell once a plate has carried it that far.
    const float threshold = cellSpacing() * 0.5f / planetRadius;  // radians

    std::vector<bool> moving(plates.size(), false);
    bool anyMoving = false;
    for (size_t p = 0; p < plates.size(); p++) {
        plates[p].pendingRotation += plates[p].angularVelocity * dt;
        if (std::fabs(plates[p].pendingRotation) >= threshold) {
            moving[p] = true;
            anyMoving = true;
        }
    }
    if (!anyMoving) {
        return;
    }

    const int n = static_cast<int>(cells.size());
    std::vector<uint16_t> next(cells.size());

    for (int i = 0; i < n; i++) {
        const uint16_t owner = cells[i].plateId;
        if (!moving[owner]) {
            next[i] = owner;
            continue;
        }
        // Which column was standing here before this plate carried it along.
        const glm::vec3 source =
            rotateAbout(cells[i].position, plates[owner].eulerPole, -plates[owner].pendingRotation);
        const int from = findNearestCell(source);
        next[i] = from >= 0 ? cells[from].plateId : owner;
    }

    for (int i = 0; i < n; i++) {
        cells[i].plateId = next[i];
    }

    for (size_t p = 0; p < plates.size(); p++) {
        if (moving[p]) {
            plates[p].pendingRotation = 0.0f;
        }
    }
}

void CrustGrid::diffuseThermalAge(float dt) {
    // Lithosphere conducts heat sideways as well as upwards, so neighbouring
    // seafloor cannot differ arbitrarily in thermal age. This is not cosmetic:
    // advecting by nearest cell quantises how far crust moves, which leaves an
    // age speckle that isostasy turns into a visible mottle over every basin.
    if (cells.size() < 2) {
        return;
    }

    // Explicit diffusion number for lithospheric thermal diffusivity
    // (~1e-6 m^2/s) over one step at this cell spacing. Deriving it rather
    // than picking it keeps the smoothing to what conduction actually does -
    // seafloor age has genuine sharp structure at fracture zones and should
    // not be blurred away.
    constexpr float THERMAL_DIFFUSIVITY = 1.0e-6f;   // m^2/s
    constexpr float SECONDS_PER_MY = 3.1557e13f;
    const float spacing = cellSpacing();
    const float rate = glm::clamp(
        THERMAL_DIFFUSIVITY * dt * SECONDS_PER_MY / (spacing * spacing), 0.0f, 0.25f);
    std::vector<float> smoothed(cells.size());

    for (size_t i = 0; i < cells.size(); i++) {
        const int count = neighbourCount(static_cast<int>(i));
        if (count == 0) {
            smoothed[i] = cells[i].age;
            continue;
        }
        float sum = 0.0f;
        for (int k = 0; k < count; k++) {
            sum += cells[neighbourAt(static_cast<int>(i), k)].age;
        }
        smoothed[i] = glm::mix(cells[i].age, sum / static_cast<float>(count), rate);
    }

    for (size_t i = 0; i < cells.size(); i++) {
        cells[i].age = smoothed[i];
    }
}

void CrustGrid::step(float millionYears) {
    if (millionYears <= 0.0f) {
        return;
    }

    // Semi-Lagrangian transport does not conserve volume. Measure what it
    // gains or loses rather than folding it into the mantle reservoir: the
    // physics and the numerical error should not be recorded in the same
    // place, or a leaking scheme looks like geology.
    const double continentalBefore = computeContinentalVolume();
    transportCrust(millionYears);
    continentalDeltaTransport += computeContinentalVolume() - continentalBefore;

    migratePlateBoundaries(millionYears);
    diffuseThermalAge(millionYears);
    updateIsostasy();
    solveSeaLevel();

    simulationTime += millionYears;
    version++;
}

// ============================================================================
// Sampling and diagnostics
// ============================================================================

float CrustGrid::sampleElevation(const glm::vec3& sphereNormal) const {
    const glm::vec3 n = glm::normalize(sphereNormal);
    const int centre = findNearestCell(n);
    if (centre < 0) {
        return -seaLevel;
    }

    // Interpolate across the cell and its neighbours by inverse square angular
    // distance. Taking the nearest cell's value alone would make every cell a
    // flat facet, which at render resolution covers the planet in a visible
    // honeycomb - the grid is a discretisation of the crust, not a mosaic it
    // is actually made of.
    double weightSum = 0.0;
    double valueSum = 0.0;

    auto accumulate = [&](int index) {
        const float cosAngle = glm::clamp(glm::dot(cells[index].position, n), -1.0f, 1.0f);
        const float angle = std::acos(cosAngle);
        const double weight = 1.0 / (static_cast<double>(angle) * angle + 1e-9);
        weightSum += weight;
        valueSum += weight * cells[index].elevation;
    };

    accumulate(centre);
    for (int k = 0; k < neighbourCount(centre); k++) {
        accumulate(neighbourAt(centre, k));
    }

    const float elevation = weightSum > 0.0
        ? static_cast<float>(valueSum / weightSum)
        : cells[centre].elevation;

    return elevation - seaLevel;
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
    stats.crustVolume = static_cast<float>(volume);
    stats.continentalVolume = static_cast<float>(continentalVolume);
    stats.oceanicVolume = static_cast<float>(oceanicVolume);
    return stats;
}

} // namespace simulation
