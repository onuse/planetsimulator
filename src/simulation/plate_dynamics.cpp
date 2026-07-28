#include "simulation/crust_grid.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

// Plate motion, solved rather than prescribed.
//
// A plate floats on the asthenosphere, so its inertia is meaningless - the
// Reynolds number is around 1e-20 and it stops the instant the forces stop.
// That means there is no acceleration term to integrate: at every moment the
// driving torques and the viscous drag exactly balance.
//
//     T_drive + T_drag(omega) = 0
//
// Drag is linear in omega, so T_drag = -D omega for a tensor D that has the
// same form as a moment of inertia, and the whole problem collapses to a 3x3
// solve per plate:
//
//     omega = D^-1 T_drive
//
// The forces are the ones that actually move plates, in the proportions
// measured on Earth: slab pull dominates, ridge push is roughly a tenth of it,
// and basal drag plus collision resistance take up the slack.

namespace simulation {

namespace {

constexpr double GRAVITATIONAL_CONSTANT = 6.674e-11;   // m^3 kg^-1 s^-2
constexpr double SECONDS_PER_MY = 3.1557e13;
constexpr float PI_F = 3.14159265358979323846f;

// Solve a 3x3 symmetric positive definite system by Gaussian elimination with
// partial pivoting. Small enough that anything cleverer would be noise.
//
// In double, and that is not fussiness. The drag tensor entries are of order
// viscosity/thickness * area * radius^2, which for an Earth-sized planet is
// around 4e39 - past the top of the float range. Assembling this in float
// silently produced infinities, the solve returned garbage, and the guard
// against non-finite results then skipped every single update, so the plates
// quietly kept their initial velocities forever while appearing to work.
glm::dvec3 solve3x3(glm::dmat3 a, glm::dvec3 b) {
    for (int column = 0; column < 3; column++) {
        int pivot = column;
        for (int row = column + 1; row < 3; row++) {
            if (std::fabs(a[column][row]) > std::fabs(a[column][pivot])) {
                pivot = row;
            }
        }
        if (std::fabs(a[column][pivot]) < 1e-200) {
            return glm::dvec3(0.0);  // degenerate: a plate with no area
        }
        if (pivot != column) {
            for (int c = 0; c < 3; c++) {
                std::swap(a[c][column], a[c][pivot]);
            }
            std::swap(b[column], b[pivot]);
        }
        for (int row = column + 1; row < 3; row++) {
            const double factor = a[column][row] / a[column][column];
            for (int c = column; c < 3; c++) {
                a[c][row] -= factor * a[c][column];
            }
            b[row] -= factor * b[column];
        }
    }

    glm::dvec3 x(0.0);
    for (int row = 2; row >= 0; row--) {
        double sum = b[row];
        for (int c = row + 1; c < 3; c++) {
            sum -= a[c][row] * x[c];
        }
        x[row] = sum / a[row][row];
    }
    return x;
}

} // namespace

float CrustGrid::getSurfaceGravity() const {
    // g = 4/3 pi G rho R. Earth's numbers give 9.8; a thousand-kilometre world
    // gives about 1.5, and its slabs pull correspondingly less hard.
    return static_cast<float>(4.0 / 3.0 * 3.14159265358979323846 *
                              GRAVITATIONAL_CONSTANT * constants.bodyDensity * planetRadius);
}

float CrustGrid::maxStableTimestep() const {
    float fastest = 0.0f;
    for (const Plate& plate : plates) {
        fastest = std::max(fastest, plate.angularVelocity());
    }
    if (fastest <= 1e-9f) {
        return 100.0f;
    }
    // Keep a plate to half a cell per step so trenches and ridges are still
    // sampled where they actually are.
    const float surfaceSpeed = fastest * planetRadius;   // m per My
    return std::max(0.01f, 0.5f * cellSpacing() / surfaceSpeed);
}

void CrustGrid::updatePlateMotion(float dt) {
    if (plates.empty() || cells.empty()) {
        return;
    }

    const Constants& k = constants;
    const float cellArea = getCellArea();
    const float gravity = getSurfaceGravity();
    const int n = static_cast<int>(cells.size());

    // Basal drag coefficient: shear stress per unit sliding velocity.
    const double dragCoefficient =
        static_cast<double>(k.asthenosphereViscosity) / k.asthenosphereThickness;

    std::vector<glm::dvec3> torque(plates.size(), glm::dvec3(0.0));
    std::vector<glm::dmat3> drag(plates.size(), glm::dmat3(0.0));

    for (Plate& plate : plates) {
        plate.slabPullTorque = glm::dvec3(0.0);
        plate.ridgePushTorque = glm::dvec3(0.0);
        plate.area = 0.0f;
        plate.subductingLength = 0.0f;
        plate.ridgeLength = 0.0f;
        plate.collidingLength = 0.0f;
    }

    // ------------------------------------------------------------------
    // Basal drag over each plate's area
    // ------------------------------------------------------------------
    for (int i = 0; i < n; i++) {
        const Cell& cell = cells[i];
        const uint16_t p = cell.plateId;
        if (p >= plates.size()) {
            continue;
        }

        const glm::dvec3 r = glm::dvec3(cell.position) * static_cast<double>(planetRadius);

        // Continental roots reach deep into the mantle and grip it harder,
        // which is why continent-heavy plates are the slow ones.
        const float buoyantFraction = glm::clamp(
            (k.oceanicDensity - cell.density) / (k.oceanicDensity - k.continentalDensity),
            0.0f, 1.0f);
        const double coupling = dragCoefficient * (1.0 + k.keelDragFactor * buoyantFraction);

        // Drag torque is -C A (r x (omega x r)), and expanding the triple
        // product gives -C A (|r|^2 I - r r^T) omega. Summed, that is a tensor
        // with the same structure as a moment of inertia.
        const double weight = coupling * cellArea;
        const double rr = glm::dot(r, r);
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                drag[p][a][b] += weight * ((a == b ? rr : 0.0) - r[a] * r[b]);
            }
        }
        plates[p].area += cellArea;
    }

    // ------------------------------------------------------------------
    // Boundary forces
    // ------------------------------------------------------------------
    for (int i = 0; i < n; i++) {
        const Cell& cell = cells[i];
        const uint16_t p = cell.plateId;
        if (p >= plates.size()) {
            continue;
        }

        const glm::vec3 ri = cell.position * planetRadius;
        const glm::vec3 vi = plateVelocityAt(cell.position, p);
        const bool buoyantHere = cell.density < k.subductionDensity;

        for (int m = 0; m < neighbourCount(i); m++) {
            const int j = neighbourAt(i, m);
            const Cell& other = cells[j];
            if (other.plateId == p) {
                continue;   // interior, no boundary force
            }

            const glm::vec3 separation = (other.position - cell.position) * planetRadius;
            const float distance = glm::length(separation);
            if (distance < 1.0f) {
                continue;
            }
            const glm::vec3 direction = separation / distance;

            // Boundary segment length this pair stands for: cells have ~6
            // neighbours, so each edge represents roughly a sixth of the
            // cell's perimeter.
            const float segment = cellSpacing() * 0.5f;

            const glm::vec3 vj = plateVelocityAt(other.position, other.plateId);
            const float convergence = glm::dot(vi - vj, direction);  // >0 closing

            const bool buoyantThere = other.density < k.subductionDensity;

            if (convergence > 0.0f) {
                if (!buoyantHere) {
                    // This side is dense ocean floor, so this side goes down.
                    // Slab pull is the negative buoyancy of the cold
                    // lithosphere, and it grows with age because the thermal
                    // boundary layer thickens as sqrt(t): old ocean pulls
                    // hardest, which is why the fastest plates are the ones
                    // with the oldest seafloor at their trenches.
                    const float lithosphere =
                        k.lithosphereThicknessCoeff * std::sqrt(std::max(0.0f, cell.age));
                    const float force = k.slabDensityContrast * gravity *
                                        lithosphere * k.slabMaxLength;

                    // The pull acts down-dip; its horizontal component drags
                    // the plate towards its own trench.
                    const glm::dvec3 f = glm::dvec3(direction) *
                                         (static_cast<double>(force) * segment);
                    const glm::dvec3 contribution = glm::cross(glm::dvec3(ri), f);
                    torque[p] += contribution;
                    plates[p].slabPullTorque += contribution;
                    plates[p].subductingLength += segment;
                } else if (buoyantThere) {
                    // Continent meeting continent. Neither will subduct, so
                    // the boundary locks and the convergence has to stop.
                    // Modelled as extra drag rather than an opposing force,
                    // which keeps it dissipative and cannot inject energy.
                    const double weight =
                        dragCoefficient * k.collisionDragFactor * segment * cellSpacing();
                    const glm::dvec3 r = glm::dvec3(ri);
                    const double rr = glm::dot(r, r);
                    for (int a = 0; a < 3; a++) {
                        for (int b = 0; b < 3; b++) {
                            drag[p][a][b] += weight * ((a == b ? rr : 0.0) - r[a] * r[b]);
                        }
                    }
                    plates[p].collidingLength += segment;
                }
            } else {
                // Diverging. Ridge push is the gravitational sliding of the
                // cooling lithosphere away from the elevated spreading centre,
                // so it also grows with age - but roughly a tenth of slab
                // pull, which is the balance seen on Earth.
                const float force = k.ridgePushPerMy * std::max(0.0f, cell.age);
                const glm::dvec3 f = glm::dvec3(-direction) *
                                     (static_cast<double>(force) * segment);
                const glm::dvec3 contribution = glm::cross(glm::dvec3(ri), f);
                torque[p] += contribution;
                plates[p].ridgePushTorque += contribution;
                plates[p].ridgeLength += segment;
            }
        }
    }

    // ------------------------------------------------------------------
    // Solve the balance and relax towards it
    // ------------------------------------------------------------------
    const float relaxation = glm::clamp(dt / std::max(0.1f, k.plateResponseTime), 0.0f, 1.0f);

    for (size_t p = 0; p < plates.size(); p++) {
        // Torques are in SI, so the solve gives radians per second; convert to
        // the per-million-year units the rest of the simulation runs in.
        const glm::dvec3 solvedSI = solve3x3(drag[p], torque[p]);
        const glm::dvec3 solved = solvedSI * SECONDS_PER_MY;

        if (!std::isfinite(solved.x) || !std::isfinite(solved.y) || !std::isfinite(solved.z)) {
            continue;
        }

        plates[p].omega = glm::mix(plates[p].omega, glm::vec3(solved), relaxation);
    }
}

// ============================================================================
// Reorganisation
// ============================================================================
//
// Rigid plates that only rotate settle into one arrangement and hold it
// forever. Real plates break up and weld together, and that is the Wilson
// cycle: oceans open, continents drift and collide, supercontinents assemble
// and rift apart again. Without this the planet has an arrangement rather than
// a history.

bool CrustGrid::trySplitPlate(uint16_t plateId) {
    if (plates.size() >= static_cast<size_t>(constants.maxPlates)) {
        return false;
    }

    // Gather the plate's cells and its centroid.
    std::vector<int> members;
    glm::dvec3 centroid(0.0);
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].plateId == plateId) {
            members.push_back(static_cast<int>(i));
            centroid += glm::dvec3(cellPositions[i]);
        }
    }
    if (static_cast<int>(members.size()) < constants.minPlateCells * 2) {
        return false;   // too small to be worth halving
    }
    centroid = glm::normalize(centroid);

    // A plate breaks where the forces on one part of it disagree with the
    // forces on another - that is why plates break at all. Try cutting it in
    // half along several planes through its centroid and ask what each half
    // would do if it were free. The plane where the two halves most want to
    // rotate differently is where it wants to come apart.
    const Plate original = plates[plateId];
    const float rate = std::max(1e-9f, original.angularVelocity());

    glm::vec3 bestNormal(0.0f);
    float bestDisagreement = 0.0f;

    // Build a tangent frame at the centroid to sweep cut orientations in.
    glm::dvec3 tangent = glm::cross(centroid, glm::dvec3(0.0, 1.0, 0.0));
    if (glm::length(tangent) < 1e-6) {
        tangent = glm::cross(centroid, glm::dvec3(1.0, 0.0, 0.0));
    }
    tangent = glm::normalize(tangent);
    const glm::dvec3 bitangent = glm::cross(centroid, tangent);

    for (int candidate = 0; candidate < 8; candidate++) {
        const double theta = PI_F * candidate / 8.0;
        const glm::vec3 normal =
            glm::vec3(glm::normalize(tangent * std::cos(theta) + bitangent * std::sin(theta)));

        // Torque each half would feel, approximated by the driving torques
        // already attributed to this plate, split by which side each cell is
        // on. Cheap, and enough to tell whether the two halves are being
        // pulled in genuinely different directions.
        glm::dvec3 torqueA(0.0), torqueB(0.0);
        double areaA = 0.0, areaB = 0.0;
        for (int index : members) {
            const bool sideA = glm::dot(cellPositions[index], normal) > 0.0f;
            (sideA ? areaA : areaB) += 1.0;
        }
        if (areaA < constants.minPlateCells || areaB < constants.minPlateCells) {
            continue;
        }

        // Attribute slab pull per cell so the halves can differ. A half with a
        // trench along it is being pulled; a half without one is only being
        // dragged.
        for (int index : members) {
            const bool sideA = glm::dot(cellPositions[index], normal) > 0.0f;
            const glm::dvec3 r = glm::dvec3(cellPositions[index]) * double(planetRadius);
            for (int m = 0; m < neighbourCount(index); m++) {
                const int j = neighbourAt(index, m);
                if (cells[j].plateId == plateId) {
                    continue;
                }
                const glm::vec3 sep = (cellPositions[j] - cellPositions[index]) * planetRadius;
                const float distance = glm::length(sep);
                if (distance < 1.0f) continue;
                const glm::dvec3 dir = glm::dvec3(sep / distance);

                const bool dense = cells[index].density >= constants.subductionDensity;
                const float lith = constants.lithosphereThicknessCoeff *
                                   std::sqrt(std::max(0.0f, cells[index].age));
                const double force = dense
                    ? double(constants.slabDensityContrast) * getSurfaceGravity() *
                      lith * constants.slabMaxLength
                    : 0.0;
                const glm::dvec3 contribution = glm::cross(r, dir * force);
                (sideA ? torqueA : torqueB) += contribution;
            }
        }

        // Normalise by area so a big half does not automatically dominate.
        const glm::dvec3 a = areaA > 0.0 ? torqueA / areaA : glm::dvec3(0.0);
        const glm::dvec3 b = areaB > 0.0 ? torqueB / areaB : glm::dvec3(0.0);
        const double disagreement = glm::length(a - b);
        if (disagreement > bestDisagreement) {
            bestDisagreement = static_cast<float>(disagreement);
            bestNormal = normal;
        }
    }

    if (bestDisagreement <= 0.0f || glm::length(bestNormal) < 0.5f) {
        return false;
    }

    // Scale the disagreement against what is already driving the plate, so the
    // threshold means the same thing for a fast plate as a slow one.
    double reference = 0.0;
    for (int index : members) {
        (void)index;
    }
    reference = glm::length(glm::dvec3(original.slabPullTorque)) /
                std::max(1.0, double(members.size()));
    if (reference <= 0.0 || bestDisagreement < reference * constants.splitDisagreementRatio) {
        return false;
    }

    // Break it. The new plate inherits the current motion and the solver takes
    // the two halves apart from there.
    const uint16_t newId = static_cast<uint16_t>(plates.size());
    plates.push_back(original);
    plates[newId].omega = original.omega * (1.0f + 0.05f * (rate > 0.0f ? 1.0f : 0.0f));

    for (Marker& marker : markers) {
        if (marker.plateId == plateId &&
            glm::dot(marker.position, bestNormal) > 0.0f) {
            marker.plateId = newId;
        }
    }
    for (int index : members) {
        if (glm::dot(cellPositions[index], bestNormal) > 0.0f) {
            cells[index].plateId = newId;
        }
    }

    splitCount++;
    return true;
}

void CrustGrid::weldLockedPlates() {
    if (plates.size() < 2) {
        return;
    }

    // Count what each pair of neighbouring plates shares. A boundary that is
    // mostly continental crust pressed against continental crust has locked -
    // neither side can subduct, so there is nothing left to take up relative
    // motion and the two are one plate in all but bookkeeping.
    const size_t count = plates.size();
    std::vector<int> shared(count * count, 0);
    std::vector<int> locked(count * count, 0);

    for (size_t i = 0; i < cells.size(); i++) {
        const uint16_t a = cells[i].plateId;
        if (a >= count) continue;
        const bool buoyantA = cells[i].density < constants.subductionDensity;

        for (int m = 0; m < neighbourCount(static_cast<int>(i)); m++) {
            const int j = neighbourAt(static_cast<int>(i), m);
            const uint16_t b = cells[j].plateId;
            if (b >= count || b == a) continue;

            shared[a * count + b]++;
            const bool buoyantB = cells[j].density < constants.subductionDensity;
            if (buoyantA && buoyantB) {
                const glm::vec3 sep = (cellPositions[j] - cellPositions[i]) * planetRadius;
                const float distance = glm::length(sep);
                if (distance < 1.0f) continue;
                const glm::vec3 dir = sep / distance;
                const float convergence =
                    glm::dot(plateVelocityAt(cellPositions[i], a) -
                             plateVelocityAt(cellPositions[j], b), dir);
                if (convergence > 0.0f) {
                    locked[a * count + b]++;
                }
            }
        }
    }

    for (size_t a = 0; a < count; a++) {
        for (size_t b = a + 1; b < count; b++) {
            const int total = shared[a * count + b] + shared[b * count + a];
            if (total < 8) continue;
            const int stuck = locked[a * count + b] + locked[b * count + a];
            if (static_cast<float>(stuck) / total < constants.weldCollisionFraction) {
                continue;
            }

            // Weld b into a. Momentum-weighted so the merged plate keeps a
            // sensible motion rather than snapping to one side's.
            const float areaA = std::max(1.0f, plates[a].area);
            const float areaB = std::max(1.0f, plates[b].area);
            plates[a].omega = (plates[a].omega * areaA + plates[b].omega * areaB) / (areaA + areaB);

            for (Marker& marker : markers) {
                if (marker.plateId == b) marker.plateId = static_cast<uint16_t>(a);
            }
            for (Cell& cell : cells) {
                if (cell.plateId == b) cell.plateId = static_cast<uint16_t>(a);
            }
            weldCount++;
            return;   // one weld per pass; the layout has changed underneath us
        }
    }
}

void CrustGrid::absorbTinyPlates() {
    if (plates.size() < 2) {
        return;
    }
    std::vector<int> population(plates.size(), 0);
    for (const Cell& cell : cells) {
        if (cell.plateId < population.size()) {
            population[cell.plateId]++;
        }
    }

    for (size_t p = 0; p < population.size(); p++) {
        if (population[p] == 0 || population[p] >= constants.minPlateCells) {
            continue;
        }
        // Give the fragment to whichever neighbour it shares most boundary
        // with - a scrap that small is a microplate being swallowed.
        std::vector<int> contact(plates.size(), 0);
        for (size_t i = 0; i < cells.size(); i++) {
            if (cells[i].plateId != p) continue;
            for (int m = 0; m < neighbourCount(static_cast<int>(i)); m++) {
                const uint16_t other = cells[neighbourAt(static_cast<int>(i), m)].plateId;
                if (other != p && other < contact.size()) {
                    contact[other]++;
                }
            }
        }
        int best = -1;
        int bestContact = 0;
        for (size_t q = 0; q < contact.size(); q++) {
            if (contact[q] > bestContact) {
                bestContact = contact[q];
                best = static_cast<int>(q);
            }
        }
        if (best < 0) continue;

        for (Marker& marker : markers) {
            if (marker.plateId == p) marker.plateId = static_cast<uint16_t>(best);
        }
        for (Cell& cell : cells) {
            if (cell.plateId == p) cell.plateId = static_cast<uint16_t>(best);
        }
    }
}

bool CrustGrid::riftSupercontinent() {
    if (plates.size() >= static_cast<size_t>(constants.maxPlates)) {
        return false;
    }

    // How much continental crust each plate is carrying, and how much there is
    // in total.
    std::vector<double> continental(plates.size(), 0.0);
    double planetTotal = 0.0;
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].density >= constants.subductionDensity) {
            continue;
        }
        if (cells[i].plateId < continental.size()) {
            continental[cells[i].plateId] += 1.0;
        }
        planetTotal += 1.0;
    }
    if (planetTotal <= 0.0) {
        return false;
    }

    int candidate = -1;
    double largest = constants.supercontinentFraction;
    for (size_t p = 0; p < continental.size(); p++) {
        const double share = continental[p] / planetTotal;
        if (share > largest) {
            largest = share;
            candidate = static_cast<int>(p);
        }
    }
    if (candidate < 0) {
        return false;
    }

    // Rift it. The cut runs through the continental interior, splitting the
    // trapped heat between the two halves - which is where a rift actually
    // opens, not around the edges.
    const uint16_t plateId = static_cast<uint16_t>(candidate);

    std::vector<int> members;
    glm::dvec3 continentalCentroid(0.0);
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].plateId != plateId) continue;
        members.push_back(static_cast<int>(i));
        if (cells[i].density < constants.subductionDensity) {
            continentalCentroid += glm::dvec3(cellPositions[i]);
        }
    }
    if (glm::length(continentalCentroid) < 1e-9 ||
        static_cast<int>(members.size()) < constants.minPlateCells * 2) {
        return false;
    }
    continentalCentroid = glm::normalize(continentalCentroid);

    glm::dvec3 tangent = glm::cross(continentalCentroid, glm::dvec3(0.0, 1.0, 0.0));
    if (glm::length(tangent) < 1e-6) {
        tangent = glm::cross(continentalCentroid, glm::dvec3(1.0, 0.0, 0.0));
    }
    tangent = glm::normalize(tangent);
    const glm::dvec3 bitangent = glm::cross(continentalCentroid, tangent);

    // Choose the cut that divides the continental area most evenly, so the
    // rift runs through the middle of the landmass rather than shaving a
    // sliver off one side.
    glm::vec3 bestNormal(0.0f);
    double bestBalance = 1e30;
    for (int c = 0; c < 12; c++) {
        const double theta = PI_F * c / 12.0;
        const glm::vec3 normal =
            glm::vec3(glm::normalize(tangent * std::cos(theta) + bitangent * std::sin(theta)));

        double sideA = 0.0, sideB = 0.0, cellsA = 0.0, cellsB = 0.0;
        for (int index : members) {
            const bool a = glm::dot(cellPositions[index], normal) > 0.0f;
            (a ? cellsA : cellsB) += 1.0;
            if (cells[index].density < constants.subductionDensity) {
                (a ? sideA : sideB) += 1.0;
            }
        }
        if (cellsA < constants.minPlateCells || cellsB < constants.minPlateCells) {
            continue;
        }
        const double balance = std::fabs(sideA - sideB);
        if (balance < bestBalance) {
            bestBalance = balance;
            bestNormal = normal;
        }
    }
    if (glm::length(bestNormal) < 0.5f) {
        return false;
    }

    const uint16_t newId = static_cast<uint16_t>(plates.size());
    plates.push_back(plates[plateId]);

    // Push the halves apart. The doming that drove the rift gives them
    // opposite senses of rotation about the rift axis, which is what turns a
    // rift into an ocean instead of letting it close again.
    const float rate = std::max(1e-4f, plates[plateId].angularVelocity());
    plates[newId].omega = plates[plateId].omega + bestNormal * (rate * 0.5f);
    plates[plateId].omega = plates[plateId].omega - bestNormal * (rate * 0.5f);

    for (Marker& marker : markers) {
        if (marker.plateId == plateId && glm::dot(marker.position, bestNormal) > 0.0f) {
            marker.plateId = newId;
        }
    }
    for (int index : members) {
        if (glm::dot(cellPositions[index], bestNormal) > 0.0f) {
            cells[index].plateId = newId;
        }
    }

    splitCount++;
    return true;
}

void CrustGrid::reorganisePlates() {
    weldLockedPlates();

    // A supercontinent rifting is the loudest thing that happens to a plate
    // layout, so give it priority over the quieter force-disagreement split.
    if (!riftSupercontinent()) {

        // Otherwise try to break whichever plate is under the most conflicting
        // pull - a plate being dragged towards two different trenches.
        int candidate = -1;
        double strongest = 0.0;
        for (size_t p = 0; p < plates.size(); p++) {
            const double pull = glm::length(plates[p].slabPullTorque);
            if (pull > strongest) {
                strongest = pull;
                candidate = static_cast<int>(p);
            }
        }
        if (candidate >= 0) {
            trySplitPlate(static_cast<uint16_t>(candidate));
        }
    }

    absorbTinyPlates();
}

} // namespace simulation
