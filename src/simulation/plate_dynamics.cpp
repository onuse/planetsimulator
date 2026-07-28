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

} // namespace simulation
