// Tests for the plate tectonics simulation.
//
// These check mechanism, not appearance: that isostasy predicts the right
// continent/ocean elevation difference, that crust is conserved, that plates
// actually move, and that the planet evolves rather than sitting still.

#include "simulation/crust_grid.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("  ok:   %s\n", what);
    }
}

void testGridTopology() {
    std::printf("Geodesic grid is well formed\n");
    simulation::CrustGrid grid(1000000.0f, 42, 5, 10);

    // An icosphere at level n has 10*4^n + 2 vertices.
    const size_t expected = 10 * 1024 + 2;
    check(grid.getCells().size() == expected, "cell count matches icosphere formula");

    // Every cell has 5 or 6 neighbours; exactly twelve have 5, the original
    // icosahedron corners.
    int pentagons = 0;
    bool validDegrees = true;
    for (size_t i = 0; i < grid.getCells().size(); i++) {
        const int degree = grid.neighbourCount(static_cast<int>(i));
        if (degree == 5) pentagons++;
        else if (degree != 6) validDegrees = false;
    }
    check(validDegrees, "every cell has 5 or 6 neighbours");
    check(pentagons == 12, "exactly twelve pentagonal cells");

    // Adjacency must be symmetric or the strain calculation is meaningless.
    bool symmetric = true;
    for (size_t i = 0; i < grid.getCells().size() && symmetric; i++) {
        for (int n = 0; n < grid.neighbourCount(static_cast<int>(i)); n++) {
            const int j = grid.neighbourAt(static_cast<int>(i), n);
            bool found = false;
            for (int m = 0; m < grid.neighbourCount(j); m++) {
                if (grid.neighbourAt(j, m) == static_cast<int>(i)) { found = true; break; }
            }
            if (!found) { symmetric = false; break; }
        }
    }
    check(symmetric, "adjacency is symmetric");
}

void testNearestCellLookup() {
    std::printf("Spatial accelerator agrees with brute force\n");
    simulation::CrustGrid grid(1000000.0f, 7, 4, 8);
    const auto& cells = grid.getCells();

    bool allMatch = true;
    // Include the poles, where the lat/long bins are degenerate.
    const glm::vec3 probes[] = {
        glm::vec3(0, 1, 0), glm::vec3(0, -1, 0), glm::vec3(1, 0, 0),
        glm::vec3(0.3f, 0.9f, 0.1f), glm::vec3(-0.5f, -0.8f, 0.3f),
        glm::vec3(0.01f, 0.999f, 0.01f), glm::vec3(-0.02f, -0.998f, 0.05f)
    };
    for (const glm::vec3& probe : probes) {
        const glm::vec3 n = glm::normalize(probe);
        int brute = 0;
        float bestDot = -2.0f;
        for (size_t i = 0; i < cells.size(); i++) {
            const float d = glm::dot(cells[i].position, n);
            if (d > bestDot) { bestDot = d; brute = static_cast<int>(i); }
        }
        const int fast = grid.findNearestCell(n);
        // Ties are possible; compare by distance rather than by index.
        if (std::fabs(glm::dot(cells[fast].position, n) - bestDot) > 1e-6f) {
            allMatch = false;
        }
    }
    check(allMatch, "accelerated lookup matches brute force, poles included");
}

void testIsostasyPredictsRealElevations() {
    std::printf("Airy isostasy reproduces measured crustal elevations\n");
    simulation::CrustGrid grid(1000000.0f, 1, 4, 8);
    const auto& k = grid.getConstants();

    // Height above the compensation datum for the two standard column types.
    const float continental = k.continentalThickness * (1.0f - k.continentalDensity / k.mantleDensity);
    const float oceanic = k.oceanicThickness * (1.0f - k.oceanicDensity / k.mantleDensity);
    const float difference = continental - oceanic;

    std::printf("  continental column stands %.0f m, oceanic %.0f m, difference %.0f m\n",
                continental, oceanic, difference);

    // Earth's continents average ~840 m above sea level and its abyssal plains
    // ~3700 m below, so real crust shows a 4-5 km step. This falls out of the
    // densities and thicknesses; nothing here was fitted to it.
    check(difference > 3500.0f && difference < 6500.0f,
          "continent/ocean step is 3.5-6.5 km, as measured on Earth");

    // Thermal subsidence should account for the ridge-to-abyssal-plain drop.
    const float subsidence = k.thermalSubsidenceRate * std::sqrt(k.thermalSubsidenceMaxAge);
    std::printf("  seafloor subsides %.0f m from ridge to %.0f My\n",
                subsidence, k.thermalSubsidenceMaxAge);
    check(subsidence > 2000.0f && subsidence < 4500.0f,
          "sqrt(age) subsidence matches observed seafloor depth");
}

void testSeaLevelRespondsToCrust() {
    std::printf("Sea level follows water volume, not a constant\n");
    simulation::CrustGrid grid(1000000.0f, 3, 4, 10);

    const auto stats = grid.computeStats();
    std::printf("  land %.1f%%, elevation range [%.0f, %.0f] m\n",
                stats.landFraction * 100.0f, stats.minElevation, stats.maxElevation);

    check(stats.landFraction > 0.02f && stats.landFraction < 0.95f,
          "planet has both land and ocean");
    check(stats.maxElevation > stats.minElevation, "elevation varies across the planet");

    // Displacing water with more crust must raise sea level relative to the
    // datum. Thicken everything and check the solver responds.
    simulation::CrustGrid thick(1000000.0f, 3, 4, 10);
    const float before = thick.getSeaLevel();
    thick.getConstants().oceanWaterGEL *= 2.0f;
    // Re-solve by stepping zero time is not allowed, so step a tiny amount.
    thick.step(0.01f);
    const float after = thick.getSeaLevel();
    std::printf("  doubling water raises sea level %.0f m -> %.0f m\n", before, after);
    check(after > before, "more water means higher sea level");
}

void testPlatesActuallyMove() {
    std::printf("Plates move and the planet evolves\n");
    simulation::CrustGrid grid(1000000.0f, 11, 5, 12);

    // Record the plate layout and elevations, then run for some geological time.
    std::vector<uint16_t> before;
    std::vector<float> elevationBefore;
    for (const auto& cell : grid.getCells()) {
        before.push_back(cell.plateId);
        elevationBefore.push_back(cell.elevation);
    }

    for (int i = 0; i < 25; i++) {
        grid.step(2.0f);
    }

    int reassigned = 0;
    float elevationChange = 0.0f;
    const auto& cells = grid.getCells();
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].plateId != before[i]) reassigned++;
        elevationChange = std::max(elevationChange, std::fabs(cells[i].elevation - elevationBefore[i]));
    }

    const float movedPct = 100.0f * reassigned / static_cast<float>(cells.size());
    std::printf("  after %.0f My: %.1f%% of cells changed plate, max elevation change %.0f m\n",
                grid.getSimulationTime(), movedPct, elevationChange);

    check(reassigned > 0, "plate boundaries migrated");
    check(elevationChange > 100.0f, "terrain changed height as a result");
    // step() splits a request into as many sub-steps as plate speed requires,
    // so version counts sub-steps rather than calls. What matters is that it
    // advances, and that the requested geological time was actually simulated.
    check(grid.getVersion() >= 25, "version advances with every sub-step taken");
    std::printf("  %llu sub-steps for 25 calls, %.0f My simulated\n",
                static_cast<unsigned long long>(grid.getVersion()), grid.getSimulationTime());
    check(std::fabs(grid.getSimulationTime() - 50.0f) < 0.5f,
          "the requested geological time was simulated exactly once");
}

// Every gram of crust must be accounted for. Crust is created from mantle melt
// at ridges and arcs, and returned to the mantle by subduction; nothing else
// may appear or disappear. This is the check that catches a process quietly
// leaking material - which is exactly how continents were vanishing.
void testSilicateBooksBalance() {
    std::printf("Crust and mantle books balance exactly\n");
    simulation::CrustGrid grid(1000000.0f, 5, 4, 12);

    const double initial = grid.getInitialCrustVolume();
    for (int i = 0; i < 50; i++) {
        grid.step(2.0f);
    }

    const double crust = grid.computeCrustVolume();
    const double mantle = grid.getMantleReservoir();
    const double drift = grid.getAdvectionDrift();

    // crust + what went to the mantle - what the transport scheme leaked
    // must equal what we started with.
    const double residual = crust + mantle - drift - initial;
    const double relative = std::fabs(residual) / initial;

    std::printf("  initial %.4e, crust %.4e, mantle %.4e, advection drift %.4e\n",
                initial, crust, mantle, drift);
    std::printf("  unaccounted residual %.3e (%.2e relative)\n", residual, relative);

    check(relative < 1e-6, "no silicate is unaccounted for");

    // Transport is a forward scatter with weights summing to one, so it should
    // not leak at all - anything here is a bug, not a tolerance.
    std::printf("  transport leak %.3e m^3 (%.2e relative)\n",
                drift, std::fabs(drift) / initial);
    check(std::fabs(drift) / initial < 1e-9, "transport conserves volume exactly");
}

void testContinentsPersist() {
    std::printf("Continents survive because arc magmatism replaces them\n");
    simulation::CrustGrid grid(1000000.0f, 5, 4, 12);

    const auto before = grid.computeStats();
    for (int i = 0; i < 100; i++) {
        grid.step(2.0f);
    }
    const auto after = grid.computeStats();

    std::printf("  land %.1f%% -> %.1f%% over %.0f My\n",
                before.landFraction * 100.0f, after.landFraction * 100.0f,
                grid.getSimulationTime());
    std::printf("  continental crust %.3e -> %.3e m^3\n",
                before.continentalVolume, after.continentalVolume);
    std::printf("  created by arcs      %.3e m^3\n", grid.getContinentalCreatedByArcs());
    std::printf("  lost to rifting      %.3e m^3\n", grid.getContinentalLostToRifting());
    std::printf("  lost to delamination %.3e m^3\n", grid.getContinentalLostToDelamination());
    std::printf("  net change in transport phase %.3e m^3\n", grid.getContinentalDeltaFromTransport());

    // Continents are consumed at margins and rebuilt by arc magmatism. They
    // may grow or shrink, but they must not be wiped out - that was the bug.
    check(after.continentalVolume > before.continentalVolume * 0.5f,
          "continental crust is not consumed away");
    check(after.landFraction > 0.08f, "the planet still has substantial land");
    check(after.landFraction < 0.85f, "the planet has not become all land");
    check(std::isfinite(after.meanElevation), "elevations stay finite");
}

// How much does transport smear the planet?
//
// One plate covering the whole sphere has no boundaries and nothing to deform
// against, so rotating it is a pure coordinate change: after a full turn every
// column must be exactly where it started. Whatever contrast the field has lost
// is numerical diffusion and nothing else. This is the honest measure of how
// faithfully crust is carried, and it is the number that decides whether the
// scheme is good enough or has to be replaced.
void testRigidRotationPreservesContrast() {
    std::printf("Rigid rotation returns the planet to where it started\n");
    simulation::CrustGrid grid(1000000.0f, 42, 4, 1);

    const auto contrast = [](const std::vector<simulation::CrustGrid::Cell>& cells) {
        double mean = 0.0;
        for (const auto& c : cells) mean += c.thickness;
        mean /= static_cast<double>(cells.size());
        double variance = 0.0;
        for (const auto& c : cells) {
            const double d = c.thickness - mean;
            variance += d * d;
        }
        return std::sqrt(variance / static_cast<double>(cells.size()));
    };

    const double before = contrast(grid.getCells());

    const float omega = grid.getPlates()[0].angularVelocity();
    const float period = 2.0f * 3.14159265f / std::fabs(omega);
    const int steps = static_cast<int>(std::ceil(period / 2.0f));
    std::printf("  one full rotation is %.0f My, %d steps of 2 My\n", period, steps);

    for (int i = 0; i < steps; i++) {
        grid.step(2.0f);
    }

    const double after = contrast(grid.getCells());
    const double retained = before > 0.0 ? after / before : 0.0;
    std::printf("  thickness contrast %.0f m -> %.0f m (%.1f%% retained)\n",
                before, after, retained * 100.0);

    check(retained > 0.5, "a full rotation keeps most of the crustal contrast");
}

// The rock record is what makes the planet remember. These check that layers
// keep their order and their identity, and that the three ways crust is
// removed take it from the right end - which is the whole point of having a
// record rather than a single averaged number.
// Plate motion is solved from forces, not prescribed. These check the solve
// lands in the right physical regime and that the forces behave the way the
// measured ones do.
void testPlateForcesGiveRealisticSpeeds() {
    std::printf("Solved plate motion lands at observed speeds\n");
    simulation::CrustGrid grid(6371000.0f, 42, 4, 12);   // Earth sized

    std::printf("  surface gravity %.2f m/s2\n", grid.getSurfaceGravity());
    check(grid.getSurfaceGravity() > 9.0f && grid.getSurfaceGravity() < 10.5f,
          "gravity derived from radius and density matches Earth's");

    // Let the torque balance take over from the initial nudge.
    for (int i = 0; i < 30; i++) {
        grid.step(1.0f);
    }

    float fastest = 0.0f;
    double totalSpeed = 0.0;
    for (const auto& plate : grid.getPlates()) {
        // Surface speed in cm/yr: omega is rad/My, so omega*R is m/My.
        const float cmPerYear = plate.angularVelocity() * grid.getPlanetRadius() * 1e-4f;
        fastest = std::max(fastest, cmPerYear);
        totalSpeed += cmPerYear;
    }
    const double mean = totalSpeed / grid.getPlates().size();
    std::printf("  mean plate speed %.2f cm/yr, fastest %.2f cm/yr\n", mean, fastest);

    // Earth's plates run from under 1 to about 10 cm/yr. Landing in that band
    // from measured forces and a viscosity inside the observed range is the
    // real check that the force balance is set up correctly.
    check(mean > 0.2 && mean < 20.0, "mean plate speed is geologically plausible");
    check(fastest < 50.0f, "no plate runs away");
}

void testSlabPullDominates() {
    std::printf("Slab pull dominates the driving torque\n");
    simulation::CrustGrid grid(6371000.0f, 7, 4, 12);
    for (int i = 0; i < 20; i++) {
        grid.step(1.0f);
    }

    double slab = 0.0;
    double ridge = 0.0;
    for (const auto& plate : grid.getPlates()) {
        slab += glm::length(plate.slabPullTorque);
        ridge += glm::length(plate.ridgePushTorque);
    }
    std::printf("  slab pull torque %.3e, ridge push torque %.3e, ratio %.1f\n",
                slab, ridge, ridge > 0.0 ? slab / ridge : 0.0);

    // On Earth slab pull is roughly an order of magnitude above ridge push,
    // and it is why plates with long trenches are the fast ones.
    check(slab > ridge, "slab pull is the larger driving force");
}

void testContinentalPlatesAreSlower() {
    std::printf("Continental keels slow their plates down\n");
    simulation::CrustGrid grid(6371000.0f, 3, 4, 14);
    for (int i = 0; i < 30; i++) {
        grid.step(1.0f);
    }

    // Work out how continental each plate is, and compare speeds.
    const auto& cells = grid.getCells();
    std::vector<double> buoyantArea(grid.getPlates().size(), 0.0);
    std::vector<double> totalArea(grid.getPlates().size(), 0.0);
    for (const auto& cell : cells) {
        if (cell.plateId >= totalArea.size()) continue;
        totalArea[cell.plateId] += 1.0;
        if (cell.density < grid.getConstants().subductionDensity) {
            buoyantArea[cell.plateId] += 1.0;
        }
    }

    double continentalSpeed = 0.0, continentalCount = 0.0;
    double oceanicSpeed = 0.0, oceanicCount = 0.0;
    for (size_t p = 0; p < grid.getPlates().size(); p++) {
        if (totalArea[p] < 20.0) continue;
        const double fraction = buoyantArea[p] / totalArea[p];
        const double speed = grid.getPlates()[p].angularVelocity() * grid.getPlanetRadius() * 1e-4;
        if (fraction > 0.5) { continentalSpeed += speed; continentalCount += 1.0; }
        else                { oceanicSpeed += speed;     oceanicCount += 1.0; }
    }

    if (continentalCount > 0.0 && oceanicCount > 0.0) {
        const double continental = continentalSpeed / continentalCount;
        const double oceanic = oceanicSpeed / oceanicCount;
        std::printf("  continent-dominated %.2f cm/yr, ocean-dominated %.2f cm/yr\n",
                    continental, oceanic);
        check(continental < oceanic, "continent-heavy plates move slower, as on Earth");
    } else {
        std::printf("  (no clean split of plate types in this configuration)\n");
    }
}

void testPlateMotionEvolves() {
    std::printf("Plate motion changes as the planet reorganises\n");
    simulation::CrustGrid grid(6371000.0f, 11, 4, 12);

    // Settle into a force-driven state first.
    for (int i = 0; i < 20; i++) {
        grid.step(1.0f);
    }
    std::vector<glm::vec3> before;
    for (const auto& plate : grid.getPlates()) {
        before.push_back(plate.omega);
    }

    for (int i = 0; i < 40; i++) {
        grid.step(2.0f);
    }

    float largestChange = 0.0f;
    for (size_t p = 0; p < before.size(); p++) {
        largestChange = std::max(largestChange,
                                 glm::length(grid.getPlates()[p].omega - before[p]));
    }
    const float relative = largestChange /
        std::max(1e-9f, glm::length(before.empty() ? glm::vec3(1.0f) : before[0]));
    std::printf("  largest change in angular velocity %.3e rad/My over 200 My\n", largestChange);

    // This is the whole point of solving rather than prescribing: as trenches
    // open and close the forces change, and the plates respond. Prescribed
    // motion would give exactly zero here forever.
    check(largestChange > 1e-5f, "plates change their motion as boundaries evolve");
    (void)relative;
}

// The Wilson cycle: plates break up and weld together, so the arrangement of
// the planet is itself part of the history rather than a fixed input.
void testPlatesReorganise() {
    std::printf("Plate layout reorganises over geological time\n");
    // Coarse grid on purpose: this needs to cover more than a billion years to
    // catch a full cycle, and the reorganisation logic does not care about
    // resolution.
    simulation::CrustGrid grid(6371000.0f, 5, 4, 10);

    // A Wilson cycle on Earth runs about 500 My from assembly to breakup, so a
    // shorter run than this can miss the whole thing and prove nothing.
    const size_t startingPlates = grid.getPlates().size();
    for (int i = 0; i < 600; i++) {
        grid.step(2.0f);
    }

    std::printf("  %.0f My: %u splits, %u welds, %zu plates now (from %zu)\n",
                grid.getSimulationTime(), grid.getSplitCount(), grid.getWeldCount(),
                grid.getPlates().size(), startingPlates);

    // How concentrated is the continental crust? A supercontinent forming is
    // what should trigger a rift, so if this never gets large the cycle can
    // never start.
    {
        std::vector<double> continental(grid.getPlates().size(), 0.0);
        const double total = static_cast<double>(grid.getCells().size());
        for (const auto& cell : grid.getCells()) {
            if (cell.density >= grid.getConstants().subductionDensity) continue;
            if (cell.plateId < continental.size()) continental[cell.plateId] += 1.0;
        }
        double biggest = 0.0;
        for (double c : continental) biggest = std::max(biggest, c);
        std::printf("  largest continental blanket covers %.0f%% of the surface (rifts at %.0f%%)\n",
                    total > 0.0 ? 100.0 * biggest / total : 0.0,
                    100.0 * grid.getConstants().supercontinentFraction);
    }

    // Frozen kinematics gives exactly zero of both, forever. Both directions
    // have to happen for a Wilson cycle: continents gather into a
    // supercontinent, the trapped heat rifts it, and the pieces disperse.
    check(grid.getWeldCount() > 0, "plates welded together as continents collided");
    check(grid.getSplitCount() > 0, "plates broke apart, so supercontinents do not last");

    // Count how many plates actually hold territory - splitting leaves entries
    // behind when a fragment is later absorbed.
    std::vector<int> population(grid.getPlates().size(), 0);
    for (const auto& cell : grid.getCells()) {
        if (cell.plateId < population.size()) population[cell.plateId]++;
    }
    int occupied = 0;
    for (int count : population) {
        if (count > 0) occupied++;
    }
    std::printf("  %d plates hold territory\n", occupied);
    check(occupied >= 2, "the planet did not collapse to a single plate");
    check(grid.getPlates().size() <= 40, "plate count stays bounded");

    const auto stats = grid.computeStats();
    check(std::isfinite(stats.meanElevation), "elevations stay finite through reorganisation");
    check(stats.landFraction > 0.02f && stats.landFraction < 0.98f,
          "the planet still has land and ocean after 600 My");
}

// Erosion moves rock; it does not create or destroy it. These check the
// mechanism: that mountains wear down, that what comes off them arrives
// somewhere lower as sediment, and that the two quantities match.
void testErosionConservesRock() {
    std::printf("Erosion moves rock rather than destroying it\n");
    simulation::CrustGrid grid(6371000.0f, 21, 4, 10);

    for (int i = 0; i < 100; i++) {
        grid.step(2.0f);
    }

    const double eroded = grid.getErodedVolume();
    const double deposited = grid.getDepositedVolume();
    std::printf("  eroded %.4e m^3, deposited %.4e m^3\n", eroded, deposited);

    check(eroded > 0.0, "erosion actually happened");

    const double mismatch = eroded > 0.0 ? std::fabs(eroded - deposited) / eroded : 0.0;
    std::printf("  mismatch %.2e relative\n", mismatch);
    check(mismatch < 1e-6, "every cubic metre eroded was deposited somewhere");

    // Denudation rate, for a sanity check against the real world. Continents
    // lower at something like 0.01-0.1 mm/yr on average, faster in active
    // orogens - so 10 to 100 m per million years.
    const auto stats = grid.computeStats();
    const double landArea = stats.landFraction * 4.0 * 3.14159265 *
                            std::pow(grid.getPlanetRadius(), 2.0);
    if (landArea > 0.0) {
        const double metresPerMy = eroded / landArea / grid.getSimulationTime();
        std::printf("  mean denudation %.1f m/My over %.0f My\n",
                    metresPerMy, grid.getSimulationTime());
        check(metresPerMy > 0.5 && metresPerMy < 2000.0,
              "denudation rate is geologically plausible");
    }
}

void testErosionMovesRockDownhill() {
    std::printf("Sediment ends up lower than where it came from\n");
    simulation::CrustGrid grid(6371000.0f, 33, 4, 10);

    for (int i = 0; i < 60; i++) {
        grid.step(2.0f);
    }

    // Where is the sediment? It should sit below the land it came off, which
    // is the meaningful comparison - not below the planet's mean elevation,
    // since most of that is abyssal plain that rivers never reach.
    double sedimentElevation = 0.0, sedimentVolume = 0.0;

    const auto& cells = grid.getCells();
    for (const auto& marker : grid.getMarkers()) {
        const int cell = grid.findNearestCell(marker.position);
        if (cell < 0) continue;
        const double elevation = cells[cell].elevation - grid.getSeaLevel();
        for (int L = 0; L < marker.layerCount; L++) {
            if (marker.layers[L].rock == simulation::CrustGrid::RockType::Sediment) {
                sedimentElevation += elevation * marker.layers[L].volume;
                sedimentVolume += marker.layers[L].volume;
            }
        }
    }

    double landElevation = 0.0;
    int landCells = 0;
    for (const auto& cell : cells) {
        const double elevation = cell.elevation - grid.getSeaLevel();
        if (elevation > 0.0) {
            landElevation += elevation;
            landCells++;
        }
    }

    check(sedimentVolume > 0.0, "sediment was laid down as a distinct rock type");

    if (sedimentVolume > 0.0 && landCells > 0) {
        const double sedimentMean = sedimentElevation / sedimentVolume;
        const double landMean = landElevation / landCells;
        std::printf("  sediment sits at %.0f m, land averages %.0f m\n",
                    sedimentMean, landMean);
        check(sedimentMean < landMean,
              "sediment ended up below the land it eroded from");
    }
}

void testErosionLimitsMountains() {
    std::printf("Erosion caps how high mountains get\n");

    // Same planet twice, once with rivers switched off.
    simulation::CrustGrid eroding(6371000.0f, 5, 4, 10);
    simulation::CrustGrid pristine(6371000.0f, 5, 4, 10);
    pristine.getConstants().streamPowerCoefficient = 0.0f;
    pristine.getConstants().hillslopeDiffusivity = 0.0f;

    for (int i = 0; i < 120; i++) {
        eroding.step(2.0f);
        pristine.step(2.0f);
    }

    const float withRivers = eroding.getMaxElevation();
    const float without = pristine.getMaxElevation();
    std::printf("  highest point %.0f m with erosion, %.0f m without\n", withRivers, without);

    // This is the coupling that matters: an orogen is limited as much by its
    // top being stripped off as by its root foundering into the mantle.
    check(withRivers < without, "rivers hold the mountains down");

    std::printf("  delamination lost %.3e m^3 with erosion, %.3e without\n",
                eroding.getContinentalLostToDelamination(),
                pristine.getContinentalLostToDelamination());
}

// Plate tectonics has few closed-form answers to check against, so the way to
// know it is behaving is to name what must never happen and watch for it.
void testNothingImpossibleHappens() {
    std::printf("Nothing physically impossible is going on\n");
    simulation::CrustGrid grid(6371000.0f, 17, 4, 10);

    for (int i = 0; i < 150; i++) {
        grid.step(2.0f);
    }

    const auto d = grid.computeDiagnostics();
    std::printf("  overlap %.2f%% of crust, up to %d plates sharing a cell\n",
                d.overlapFraction * 100.0f, d.maxPlatesInOneCell);
    std::printf("  largest single-step elevation jump %.0f m\n", d.maxElevationJump);
    std::printf("  %d empty cells, %d microplates, fastest plate %.1f cm/yr\n",
                d.emptyCells, d.microPlates, d.fastestPlateCmPerYear);

    // Two plates cannot be in the same place. Some overlap is expected at
    // margins, which are a cell wide and where subduction is actually
    // happening, but it should be a few percent - not a landmass sliding
    // across another one.
    check(d.overlapFraction < 0.15f, "plates are not passing through each other");

    // Tectonics and erosion are slow. A cell jumping kilometres between steps
    // is a discretisation artefact, not geology.
    check(d.maxElevationJump < 3000.0f, "the surface never jumps implausibly far");

    check(d.emptyCells < static_cast<int>(grid.getCells().size()) / 20,
          "transport is not losing track of the crust");
    check(d.fastestPlateCmPerYear < 60.0f, "no plate is running away");
}

void testStratigraphy() {
    std::printf("Columns record their history in order\n");
    using Grid = simulation::CrustGrid;

    Grid::Marker marker;
    marker.deposit(Grid::RockType::Granite, 1000.0, 500.0f);
    marker.deposit(Grid::RockType::Sediment, 400.0, 200.0f);
    marker.deposit(Grid::RockType::Basalt, 200.0, 50.0f);

    check(marker.layerCount == 3, "three distinct episodes are kept separate");
    check(marker.layers[0].rock == Grid::RockType::Granite, "oldest rock is at the bottom");
    check(marker.layers[2].rock == Grid::RockType::Basalt, "youngest rock is on top");
    check(std::fabs(marker.volume - 1600.0) < 1e-9, "volume is the sum of the record");

    // Density is the volume weighted mix, so a sediment basin really is more
    // buoyant than the basalt under it.
    const double expected = (1000.0 * Grid::rockDensity(Grid::RockType::Granite) +
                             400.0 * Grid::rockDensity(Grid::RockType::Sediment) +
                             200.0 * Grid::rockDensity(Grid::RockType::Basalt)) / 1600.0;
    std::printf("  column density %.1f kg/m3, expected %.1f\n", marker.density, expected);
    check(std::fabs(marker.density - expected) < 0.5, "density is the mass weighted mix");

    // Erosion strips the youngest rock first.
    Grid::Marker eroding = marker;
    const double stripped = eroding.erodeFromTop(250.0);
    check(std::fabs(stripped - 250.0) < 1e-9, "erosion removes what was asked for");
    check(eroding.layers[eroding.layerCount - 1].rock == Grid::RockType::Sediment,
          "erosion cut through the basalt into the sediment below");
    check(eroding.layers[0].rock == Grid::RockType::Granite,
          "erosion left the basement untouched");

    // Delamination takes the root instead.
    Grid::Marker foundering = marker;
    const double shed = foundering.removeFromBottom(1000.0);
    check(std::fabs(shed - 1000.0) < 1e-9, "delamination removes what was asked for");
    check(foundering.layers[0].rock == Grid::RockType::Sediment,
          "delamination removed the deep basement, not the surface");
    check(foundering.layers[foundering.layerCount - 1].rock == Grid::RockType::Basalt,
          "the youngest rock survived delamination");

    // A subducting slab goes down entire.
    Grid::Marker slab = marker;
    const double consumed = slab.consumeProportionally(800.0);
    check(std::fabs(consumed - 800.0) < 1e-9, "subduction consumes what was asked for");
    check(slab.layerCount == 3, "subduction thins every episode rather than peeling any");
    check(std::fabs(slab.layers[0].volume - 500.0) < 1e-6,
          "each episode lost the same fraction");

    // Same rock arriving on top of itself is one episode, not two.
    Grid::Marker merging;
    merging.deposit(Grid::RockType::Sediment, 100.0, 10.0f);
    merging.deposit(Grid::RockType::Sediment, 100.0, 20.0f);
    check(merging.layerCount == 1, "successive deposits of one rock type coalesce");

    // Overflowing the record must not lose rock.
    Grid::Marker deep;
    double placed = 0.0;
    for (int i = 0; i < 40; i++) {
        const auto rock = (i % 2) ? Grid::RockType::Sediment : Grid::RockType::Basalt;
        deep.deposit(rock, 10.0, static_cast<float>(i));
        placed += 10.0;
    }
    std::printf("  %d episodes recorded after 40 deposits, volume %.1f of %.1f\n",
                deep.layerCount, deep.volume, placed);
    check(deep.layerCount <= simulation::CrustGrid::MAX_LAYERS, "the record stays bounded");
    check(std::fabs(deep.volume - placed) < 1e-6, "merging deep episodes loses no rock");
}

void testSurfaceReconstruction() {
    std::printf("Surface reconstructs between cells, not just at them\n");
    simulation::CrustGrid grid(1000000.0f, 42, 5, 10);

    const auto& cells = grid.getCells();
    int corner[3];
    float weight[3];

    // At a cell centre the answer must be that cell alone. Anything else means
    // the reconstruction does not honour the data it is built from.
    bool exactAtCentres = true;
    bool weightsSumToOne = true;
    bool weightsNonNegative = true;

    for (size_t i = 0; i < cells.size(); i += 197) {
        if (!grid.barycentricCells(cells[i].position, corner, weight)) {
            exactAtCentres = false;
            break;
        }
        const float sum = weight[0] + weight[1] + weight[2];
        if (std::abs(sum - 1.0f) > 1e-3f) {
            weightsSumToOne = false;
        }
        for (int k = 0; k < 3; k++) {
            if (weight[k] < -1e-3f) {
                weightsNonNegative = false;
            }
        }

        float own = 0.0f;
        for (int k = 0; k < 3; k++) {
            if (corner[k] == static_cast<int>(i)) {
                own = weight[k];
            }
        }
        if (own < 0.999f) {
            exactAtCentres = false;
        }
    }

    check(weightsSumToOne, "weights sum to one");
    check(weightsNonNegative, "no negative weights");
    check(exactAtCentres, "a cell centre resolves to that cell alone");

    // The one that matters, and the one the old inverse-square weighting
    // failed. Walking from one cell centre to a neighbour, the contribution
    // should hand over linearly. Under a weight that is singular at the
    // centres it instead sticks near 1 for most of the way and then steps
    // across - which is what drew the hexagons.
    const int a = 0;
    const int b = grid.neighbourAt(a, 0);
    const glm::vec3 from = cells[a].position;
    const glm::vec3 to = cells[b].position;

    float worstError = 0.0f;
    for (int step = 1; step < 10; step++) {
        const float t = static_cast<float>(step) / 10.0f;
        const glm::vec3 p = glm::normalize(from * (1.0f - t) + to * t);

        if (!grid.barycentricCells(p, corner, weight)) {
            worstError = 1.0f;
            break;
        }
        float towardsB = 0.0f;
        for (int k = 0; k < 3; k++) {
            if (corner[k] == b) {
                towardsB = weight[k];
            }
        }
        worstError = std::max(worstError, std::abs(towardsB - t));
    }

    std::printf("    worst departure from linear along an edge: %.4f\n", worstError);
    check(worstError < 0.02f, "contribution hands over linearly between cells");

    // And the consequence, measured the way it is seen: sampling across a few
    // cells must not produce runs of identical values.
    int plateauRun = 0;
    int worstRun = 0;
    float previous = grid.sampleElevation(cells[a].position);
    for (int step = 1; step <= 400; step++) {
        const float t = static_cast<float>(step) / 400.0f;
        const glm::vec3 p = glm::normalize(from * (1.0f - t) + to * t * 4.0f);
        const float here = grid.sampleElevation(p);
        plateauRun = (std::abs(here - previous) < 1e-4f) ? plateauRun + 1 : 0;
        worstRun = std::max(worstRun, plateauRun);
        previous = here;
    }
    std::printf("    longest run of identical samples: %d of 400\n", worstRun);
    check(worstRun < 40, "no flat plateaus across the surface");

    // The surface must not crease where one triangle meets the next.
    //
    // Blending three corner values linearly reproduces them exactly and is
    // flat everywhere in between, so the slope changes abruptly at every edge
    // - which is what makes a rendered planet look like a faceted shell. That
    // shows up here as a spike in the second difference along a path, and it
    // is invisible to any test that only checks values at and between cells.
    //
    // Measured against the typical bend of the field itself, so the number
    // means "how much sharper is the worst kink than ordinary terrain".
    const glm::vec3 axis = glm::normalize(glm::cross(from, glm::vec3(0.0f, 0.0f, 1.0f)));
    constexpr int STEPS = 3000;

    // Measured against the flat blend rather than against a number picked out
    // of the air, because what matters is whether curving between cells
    // actually removed the creases the flat blend leaves.
    std::vector<float> curved(STEPS);
    std::vector<float> flat(STEPS);

    for (int i = 0; i < STEPS; i++) {
        // A quarter of the way round the planet, crossing many cells.
        const float angle = (static_cast<float>(i) / STEPS) * 1.57f;
        const glm::vec3 p = glm::normalize(from * std::cos(angle) + axis * std::sin(angle));
        curved[i] = grid.sampleElevation(p);

        int corner[3];
        float weight[3];
        flat[i] = 0.0f;
        if (grid.barycentricCells(p, corner, weight)) {
            for (int k = 0; k < 3; k++) {
                flat[i] += weight[k] * cells[corner[k]].elevation;
            }
        }
    }

    const auto sharpnessOf = [](const std::vector<float>& walk) {
        double total = 0.0;
        double worst = 0.0;
        for (size_t i = 1; i + 1 < walk.size(); i++) {
            const double bend = std::abs(walk[i + 1] - 2.0 * walk[i] + walk[i - 1]);
            total += bend;
            worst = std::max(worst, bend);
        }
        const double mean = total / (walk.size() - 2);
        return mean > 1e-9 ? worst / mean : 0.0;
    };

    const double flatSharpness = sharpnessOf(flat);
    const double curvedSharpness = sharpnessOf(curved);

    std::printf("    worst kink: %.1fx average bend flat, %.1fx curved\n",
                flatSharpness, curvedSharpness);
    check(curvedSharpness < flatSharpness * 0.5,
          "curving between cells at least halves the worst crease");
}

void testRivers() {
    std::printf("Rivers run downhill and reach the sea\n");
    simulation::CrustGrid grid(1000000.0f, 17, 5, 12);

    // Erosion has to have run for a network to exist.
    for (int i = 0; i < 6; i++) {
        grid.step(1.0f);
    }

    auto snapshot = grid.publishSnapshot();
    const auto& cells = grid.getCells();

    check(snapshot->discharge.size() == cells.size(), "the network was published");
    check(snapshot->flowsInto.size() == cells.size(), "with its flow directions");

    // Every cell must send water to somewhere lower, or to nowhere at all -
    // unless it is under a lake, where the water surface is level and flow
    // follows that rather than the rock beneath it. Depression filling is what
    // makes lakes, so routing "uphill" out of one is correct; routing uphill
    // on dry ground would mean the fill and the routing disagree.
    // Against the land, not against the planet. Water on the seafloor is
    // already in the sea and has nowhere to be routed to, so most of a cell
    // count that includes ocean will never drain and should not be expected
    // to - the first version of this measured against the whole surface and
    // failed on a planet that was behaving correctly.
    int land = 0;
    int routed = 0;
    int uphill = 0;
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].elevation - grid.getSeaLevel() > 0.0f) {
            land++;
        }
        const int into = snapshot->flowsInto[i];
        if (into < 0) {
            continue;
        }
        routed++;

        // Checked against the surface the router actually used, not against
        // the current elevations. The network is rebuilt every couple of
        // million years and plates keep moving in between, so comparing it to
        // today's ground shows hundreds of cells apparently draining uphill by
        // hundreds of metres with nothing wrong in the routing - it is two
        // different moments being compared.
        //
        // Against the routed surface this is an invariant that must hold by
        // construction: the router chose the lowest neighbour on that surface.
        // If it ever fails, the router is genuinely broken.
        if (snapshot->routedSurface[into] > snapshot->routedSurface[i] + 1.0f) {
            uphill++;
            if (uphill <= 4) {
                std::printf("    uphill: %.1f m -> %.1f m on the routed surface\n",
                            snapshot->routedSurface[i], snapshot->routedSurface[into]);
            }
        }
    }

    int lakeCells = 0;
    for (size_t i = 0; i < cells.size(); i++) {
        if (snapshot->lakeDepth[i] > 0.0f) {
            lakeCells++;
        }
    }
    std::printf("  %d of %d land cells route somewhere; %d lakes; %d uphill on dry ground\n",
                routed, land, lakeCells, uphill);
    check(routed > land / 2, "most of the land drains");
    check(uphill == 0, "no cell drains uphill");

    // Discharge has to grow downstream. A river is the collecting of water,
    // so a cell must carry at least what it receives - if it does not, the
    // accumulation is walking the network in the wrong order.
    int shrinks = 0;
    for (size_t i = 0; i < cells.size(); i++) {
        const int into = snapshot->flowsInto[i];
        if (into < 0) {
            continue;
        }
        if (snapshot->discharge[into] < snapshot->discharge[i] * 0.999f) {
            shrinks++;
        }
    }
    std::printf("  %d cells carry less than what flows into them\n", shrinks);
    check(shrinks == 0, "discharge grows downstream");

    // And the consequence the renderer depends on: channels have to be narrow.
    // Sampling along a line across the land, only a small fraction of it
    // should be river - a model that called a fifth of the surface "river"
    // would draw floodplains, not rivers.
    int wet = 0;
    int dry = 0;
    for (size_t i = 0; i < cells.size(); i += 3) {
        if (cells[i].elevation - grid.getSeaLevel() < 0.0f) {
            continue;
        }
        const float strength = grid.sampleRiver(*snapshot, cells[i].position);
        if (strength > 0.25f) {
            wet++;
        } else {
            dry++;
        }
    }
    const float share = (wet + dry) > 0 ? static_cast<float>(wet) / (wet + dry) : 0.0f;
    std::printf("  %.1f%% of sampled land sits in a channel\n", share * 100.0f);
    check(share > 0.0005f, "rivers exist somewhere on the land");
    check(share < 0.25f, "rivers are channels, not floodplains");
}

void testClimate() {
    std::printf("Climate follows from where the continents are\n");
    simulation::CrustGrid grid(1000000.0f, 5, 5, 12);

    const auto& climate = grid.getClimate();
    const auto& fields = climate.getFields();
    const auto& cells = grid.getCells();

    // Temperatures have to be a planet's, not a number that came out of an
    // equation. The energy balance is solved, not fitted, so this is a real
    // check on the coefficients rather than a restatement of them.
    float coldest = 1e9f;
    float warmest = -1e9f;
    double tropicSum = 0.0, polarSum = 0.0;
    int tropicCount = 0, polarCount = 0;

    for (size_t i = 0; i < cells.size(); i++) {
        const float t = fields.temperature[i];
        coldest = std::min(coldest, t);
        warmest = std::max(warmest, t);

        const float absLatitude = std::abs(cells[i].position.y);
        if (absLatitude < 0.34f) {           // within 20 degrees of the equator
            tropicSum += t;
            tropicCount++;
        } else if (absLatitude > 0.94f) {    // beyond 70 degrees
            polarSum += t;
            polarCount++;
        }
    }

    const float tropics = static_cast<float>(tropicSum / std::max(tropicCount, 1));
    const float poles = static_cast<float>(polarSum / std::max(polarCount, 1));

    std::printf("  mean %.1f C, range %.1f to %.1f, tropics %.1f, poles %.1f\n",
                fields.meanTemperature, coldest, warmest, tropics, poles);
    std::printf("  ice covers %.1f%% of the surface\n", fields.iceFraction * 100.0f);

    check(fields.meanTemperature > -20.0f && fields.meanTemperature < 40.0f,
          "global mean temperature is habitable, not frozen or boiling");
    check(tropics > poles + 20.0f, "the tropics are much warmer than the poles");
    check(fields.iceFraction > 0.005f && fields.iceFraction < 0.60f,
          "ice caps exist without swallowing the planet");

    // The reason any of this was built. Air climbing a range rains on the way
    // up and arrives dry on the far side, so the two flanks of a mountain get
    // different amounts of water - and therefore erode at different rates.
    // With a single precipitation number they eroded identically, which is the
    // one thing real mountains never do.
    double rainTotal = 0.0;
    for (size_t i = 0; i < cells.size(); i++) {
        rainTotal += fields.precipitation[i];
    }
    const float meanRain = static_cast<float>(rainTotal / cells.size());

    int sampled = 0;
    int windwardWetter = 0;
    double ratioSum = 0.0;

    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].elevation - grid.getSeaLevel() < 500.0f) {
            continue;   // only where there is relief to lift the air over
        }
        const glm::vec3 wind = fields.wind[i];
        if (glm::dot(wind, wind) < 0.5f) {
            continue;
        }

        // The neighbour the wind comes from, and the one it goes to.
        int upwind = -1, downwind = -1;
        float bestUp = 0.3f, bestDown = 0.3f;
        for (int k = 0; k < grid.neighbourCount(static_cast<int>(i)); k++) {
            const int j = grid.neighbourAt(static_cast<int>(i), k);
            const glm::vec3 toward = glm::normalize(cells[j].position - cells[i].position);
            const float alignment = glm::dot(toward, wind);
            if (alignment > bestDown) { bestDown = alignment; downwind = j; }
            if (-alignment > bestUp)  { bestUp = -alignment;  upwind = j; }
        }
        if (upwind < 0 || downwind < 0) {
            continue;
        }

        const float wet = fields.precipitation[upwind];
        const float dry = fields.precipitation[downwind];

        // Deep in a continent the air arrives with nothing left and both
        // sides are dry, which says nothing about rain shadows either way.
        // Counting those as failures is what dragged the first version of
        // this measurement below chance.
        if (wet + dry < 0.05f * meanRain) {
            continue;
        }

        sampled++;
        if (wet > dry) {
            windwardWetter++;
        }
        ratioSum += (wet + 1e-6) / (dry + 1e-6);
    }

    const float share = sampled > 0 ? static_cast<float>(windwardWetter) / sampled : 0.0f;
    std::printf("  of %d upland cells, %.0f%% are wetter upwind than downwind\n",
                sampled, share * 100.0f);

    check(sampled > 50, "there is upland terrain to test rain shadows on");
    check(share > 0.6f, "high ground is wetter on its windward side");

    // And the consequence: erosion has to see it. Rainfall varies across the
    // planet by more than the roughly two-fold that a uniform field would
    // give, or nothing downstream can tell the difference.
    float driest = 1e9f;
    float wettest = 0.0f;
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].elevation - grid.getSeaLevel() < 0.0f) {
            continue;
        }
        driest = std::min(driest, climate.relativePrecipitation(static_cast<int>(i)));
        wettest = std::max(wettest, climate.relativePrecipitation(static_cast<int>(i)));
    }
    std::printf("  land rainfall spans %.2fx to %.2fx the planetary mean\n", driest, wettest);
    check(wettest > driest * 4.0f, "rainfall varies enough across land to shape erosion");
}

void testTimeSlicingDoesNotChangeThePlanet() {
    std::printf("Whether how time is sliced changes what the planet becomes\n");

    // The same simulated time, cut into different sized steps. A landscape
    // after twenty million years should be the same landscape whichever way
    // the twenty million years were counted out - if it is not, the answer
    // depends on the frame rate, and nothing built on it can be trusted.
    struct Result {
        float step;
        float highest;
        float land;
        float crust;
        int rivers;
    };

    const float steps[] = {0.5f, 2.0f, 5.0f};
    Result results[3];

    for (int variant = 0; variant < 3; variant++) {
        simulation::CrustGrid grid(1000000.0f, 61, 5, 12);

        const float slice = steps[variant];
        const int count = static_cast<int>(20.0f / slice);
        for (int i = 0; i < count; i++) {
            grid.step(slice);
        }

        const auto stats = grid.computeStats();
        auto snapshot = grid.publishSnapshot();

        int rivers = 0;
        for (size_t i = 0; i < snapshot->discharge.size(); i++) {
            if (snapshot->flowsInto[i] >= 0) {
                rivers++;
            }
        }

        results[variant] = {slice, stats.maxElevation, stats.landFraction,
                            stats.crustVolume, rivers};

        std::printf("    %.1f My steps: highest %.0f m, land %.1f%%, crust %.4e, "
                    "%d draining cells\n",
                    slice, stats.maxElevation, stats.landFraction * 100.0f,
                    stats.crustVolume, rivers);
    }

    // Compared against the finest slicing, which is the most trustworthy.
    const Result& reference = results[0];
    float worstHeight = 0.0f;
    float worstLand = 0.0f;
    for (int variant = 1; variant < 3; variant++) {
        worstHeight = std::max(worstHeight,
                               std::fabs(results[variant].highest - reference.highest) /
                                   std::max(reference.highest, 1.0f));
        worstLand = std::max(worstLand, std::fabs(results[variant].land - reference.land));
    }

    std::printf("    against the finest slicing: highest differs by up to %.0f%%, "
                "land by %.1f points\n", worstHeight * 100.0f, worstLand * 100.0f);

    check(worstHeight < 0.35f, "peak height does not depend on the step size");
    check(worstLand < 0.10f, "how much land there is does not depend on the step size");
}

void testRiversComeInSizes() {
    std::printf("Whether rivers come in a range of sizes or all look alike\n");

    // The complaint was that every river is the same width. Colouring can only
    // show a range the simulation actually produces, so measure the range
    // before deciding how to draw it.
    simulation::CrustGrid grid(1000000.0f, 61, 6, 12);
    for (int i = 0; i < 40; i++) {
        grid.step(0.5f);
    }
    auto snapshot = grid.publishSnapshot();

    // Catchment counts along the drawn network, in the buckets the renderer
    // cares about: invisible, a corridor with no open water, and a trunk.
    int buckets[4] = {0, 0, 0, 0};
    float widest = 0.0f;
    float narrowest = 1e30f;
    int drawn = 0;
    for (size_t i = 0; i < snapshot->discharge.size(); i++) {
        if (snapshot->flowsInto[i] < 0) {
            continue;
        }
        const float catchments = snapshot->discharge[i];
        if (catchments < 3.0f) {
            continue;
        }
        drawn++;
        const float width = grid.channelWidthFor(catchments);
        widest = std::max(widest, width);
        narrowest = std::min(narrowest, width);

        if (catchments < 20.0f) {
            buckets[0]++;
        } else if (catchments < 220.0f) {
            buckets[1]++;
        } else if (catchments < 2000.0f) {
            buckets[2]++;
        } else {
            buckets[3]++;
        }
    }

    std::printf("    %d channels drawn, widths %.0f m to %.0f m\n",
                drawn, narrowest, widest);
    std::printf("    stream %d (%.0f%%), small river %d (%.0f%%), "
                "large %d (%.0f%%), trunk %d (%.0f%%)\n",
                buckets[0], 100.0f * buckets[0] / std::max(drawn, 1),
                buckets[1], 100.0f * buckets[1] / std::max(drawn, 1),
                buckets[2], 100.0f * buckets[2] / std::max(drawn, 1),
                buckets[3], 100.0f * buckets[3] / std::max(drawn, 1));

    // Why the sizes come out as they do. A network with no trunk cannot have a
    // range of widths, so if the widths are uniform the structure is the thing
    // to look at, not the width rule.
    const int n = static_cast<int>(snapshot->discharge.size());
    int land = 0;
    int drainsToSea = 0;
    int endsInland = 0;
    int longest = 0;
    float biggestCatchment = 0.0f;

    // Against the surface the network was routed on, not the current one. The
    // network is rebuilt on an interval and can be two million years older than
    // the elevations, and judging it by ground it has never seen reports rivers
    // stranded inland that drain perfectly well on the map they were drawn for.
    const std::vector<float>& routed =
        snapshot->routedSurface.size() == snapshot->elevation.size()
            ? snapshot->routedSurface
            : snapshot->elevation;

    for (int i = 0; i < n; i++) {
        if (routed[i] <= 0.0f) {
            continue;
        }
        land++;
        biggestCatchment = std::max(biggestCatchment, snapshot->discharge[i]);

        // Walk downstream and see where it ends up.
        int at = i;
        int steps = 0;
        while (steps < n) {
            const int into = snapshot->flowsInto[at];
            if (into < 0) {
                break;
            }
            at = into;
            steps++;
            if (routed[at] <= 0.0f) {
                break;
            }
        }
        longest = std::max(longest, steps);
        if (routed[at] <= 0.0f) {
            drainsToSea++;
        } else {
            endsInland++;
        }
    }

    // How big the basins actually are, counted rather than accumulated. If the
    // largest basin holds far more cells than the largest catchment reports,
    // the accumulation is losing water somewhere; if they agree, the network is
    // simply as small as the grid allows and no rule about widths will help.
    std::vector<int> basinSize(n, 0);
    for (int i = 0; i < n; i++) {
        if (routed[i] <= 0.0f) {
            continue;
        }
        int at = i;
        int guard = 0;
        while (guard++ < n) {
            const int into = snapshot->flowsInto[at];
            if (into < 0 || routed[at] <= 0.0f) {
                break;
            }
            at = into;
        }
        basinSize[at]++;
    }
    const int biggestBasin = *std::max_element(basinSize.begin(), basinSize.end());
    int mouths = 0;
    for (int i = 0; i < n; i++) {
        if (basinSize[i] > 0) {
            mouths++;
        }
    }

    std::printf("    %d land cells: %d reach the sea, %d end inland\n",
                land, drainsToSea, endsInland);
    std::printf("    %d river mouths, biggest basin %d cells, area per cell %.3e m2, "
                "spacing squared %.3e m2\n",
                mouths, biggestBasin, grid.getCellArea(),
                grid.cellSpacing() * grid.cellSpacing());
    std::printf("    longest path to the sea %d cells, biggest catchment %.0f cells\n",
                longest, biggestCatchment);

    check(drawn > 100, "there is a network to look at");

    // Every land cell, not most of them. After the depressions are filled there
    // is no such thing as a cell with nowhere to go - the fill exists precisely
    // to guarantee that - so any cell stranded inland is a bug in the routing
    // and not a feature of the terrain. This held at zero exceptions out of
    // nine and a half thousand, and it is the invariant worth guarding: it was
    // false for more than half the planet until the flood's own spill paths
    // were used to cross the flats it creates.
    check(drainsToSea == land, "every land cell drains to the sea");
    check(longest > 25, "the network is deep enough to have long rivers in it");

    // Only three to one, and that is the honest number. Rivers here are much
    // the same size because the hierarchy is shallow - sixteen hundred separate
    // outlets for nine thousand land cells - and no width rule can invent a
    // range the drainage does not have. Guarding it stops the range collapsing
    // further without pretending it is Earth's.
    check(widest > narrowest * 2.5f, "channels differ in width by more than a factor of two");
}

void testChannelsCarveAndPersist() {
    std::printf("Whether rivers cut channels, and whether the channels outlast them\n");

    // The point of tracking channel depth in the simulation rather than
    // synthesising it in the renderer is that it becomes a thing the ground
    // remembers. Two claims follow, and both are checked here: rivers cut in
    // proportion to what passes through them, and a channel left behind by a
    // river that has moved does not disappear with it.
    simulation::CrustGrid grid(1000000.0f, 61, 6, 12);

    // The landscape regime, where the routed model runs and channels are cut.
    const float slice = 0.02f;
    for (int i = 0; i < 60; i++) {
        grid.step(slice);
    }

    auto snapshot = grid.publishSnapshot();
    const int n = static_cast<int>(snapshot->channelDepth.size());
    check(n == static_cast<int>(snapshot->elevation.size()), "channel depth is published");

    int cut = 0;
    float deepest = 0.0f;
    double smallSum = 0.0, largeSum = 0.0;
    int smallCount = 0, largeCount = 0;
    int belowReceiver = 0;

    for (int i = 0; i < n; i++) {
        const float depth = snapshot->channelDepth[i];
        if (depth <= 0.0f) {
            continue;
        }
        cut++;
        deepest = std::max(deepest, depth);

        // Split by size only to report it. Expecting big rivers to cut deeper
        // was wrong and the measurement said so: a big river is by definition
        // near the bottom of its own network, and a river cannot cut below the
        // sea it is running into. So trunks near the coast run in shallow
        // valleys and the deep gorges are cut by moderate rivers still high
        // above base level - the Amazon against the Colorado. Depth follows
        // height above base level, not discharge.
        if (snapshot->discharge[i] > 15.0f) {
            largeSum += depth;
            largeCount++;
        } else {
            smallSum += depth;
            smallCount++;
        }

        // A channel cut below the bed of the river it flows into would mean
        // water climbing out of its own channel to leave the cell. Only where
        // there is water: a valley whose river has gone is a landform, and
        // landforms are allowed to be deeper than the ground next door.
        const int down = snapshot->flowsInto[i];
        if (down >= 0 && down < n &&
            snapshot->discharge[i] >= grid.getConstants().channelThreshold) {
            const float floorHere = snapshot->elevation[i] - depth;
            const float floorThere = snapshot->elevation[down] - snapshot->channelDepth[down];
            if (floorHere < floorThere - 1.0f) {
                belowReceiver++;
            }
        }
    }

    const float smallMean = smallCount > 0 ? static_cast<float>(smallSum / smallCount) : 0.0f;
    const float largeMean = largeCount > 0 ? static_cast<float>(largeSum / largeCount) : 0.0f;

    std::printf("    %d cells carry a channel, deepest %.0f m\n", cut, deepest);
    std::printf("    mean depth: headwaters %.0f m (%d), rivers %.0f m (%d)\n",
                smallMean, smallCount, largeMean, largeCount);
    std::printf("    %d cut below the channel they drain into\n", belowReceiver);

    check(cut > 50, "rivers cut channels");
    check(deepest > 5.0f, "the channels amount to something");
    // Not zero, and it should not be. The limit is applied when the channels
    // are cut, and the ground keeps moving afterwards - tectonics can lift a
    // cell out from under its own river between one step and the next. That is
    // antecedent drainage, which is a real thing rivers do. What would not be
    // real is a network riddled with it, so this bounds it rather than
    // forbidding it.
    check(belowReceiver * 100 < cut, "almost no channel sits below the one it flows into");

    // No channel floor below sea level. The river has no energy left to cut
    // with once it gets there, and without this the depth grows with however
    // long the integration runs rather than with the landscape.
    int belowSea = 0;
    for (int i = 0; i < n; i++) {
        if (snapshot->channelDepth[i] > 0.0f &&
            snapshot->channelDepth[i] > snapshot->elevation[i] + 1.0f) {
            belowSea++;
        }
    }
    std::printf("    %d channels cut below sea level\n", belowSea);
    check(belowSea == 0, "no channel is cut below base level");

    // Now take the water away entirely and see what the ground keeps.
    //
    // Erosion off, so nothing is cut and nothing is deposited. Whatever depth
    // survives is what an abandoned valley would look like after the same time,
    // and it is the difference between a capture leaving a mark and a capture
    // being invisible.
    std::vector<float> before = snapshot->channelDepth;
    grid.getConstants().streamPowerCoefficient = 0.0f;

    for (int i = 0; i < 60; i++) {
        grid.step(slice);
    }
    auto later = grid.publishSnapshot();

    double kept = 0.0, had = 0.0;
    int survivors = 0;
    for (int i = 0; i < n && i < static_cast<int>(later->channelDepth.size()); i++) {
        if (before[i] <= 0.0f) {
            continue;
        }
        had += before[i];
        kept += later->channelDepth[i];
        if (later->channelDepth[i] > before[i] * 0.5f) {
            survivors++;
        }
    }

    const float retained = had > 0.0 ? static_cast<float>(kept / had) : 0.0f;
    std::printf("    after %.1f My with no water: %.0f%% of the depth remains, "
                "%d channels still half cut\n",
                60.0f * slice, retained * 100.0f, survivors);

    check(retained > 0.5f, "an abandoned channel is still there long after the river left");
}

void testAbandonedValleysSurviveTheRiver() {
    std::printf("What a river leaves behind when it stops running there\n");

    // The one that matters, and the one the previous test missed.
    //
    // Persistence was checked by switching erosion off and watching the depth
    // decay. That leaves the network routing, so every channel still had its
    // water and what was measured was a slow fade of channels that were still
    // rivers. It never tested abandonment at all - and abandonment was the case
    // that was broken, because a cell whose discharge fell below the threshold
    // had its depth set straight to zero. A river could spend two million years
    // cutting a valley and take the valley with it the instant the network
    // moved, leaving nothing on the continent to show it was ever there.
    //
    // So this finds cells that genuinely lose their river between one moment
    // and another, and asks what is left in the ground afterwards.
    simulation::CrustGrid grid(1000000.0f, 61, 6, 12);

    const float slice = 0.02f;
    for (int i = 0; i < 60; i++) {
        grid.step(slice);
    }

    auto before = grid.publishSnapshot();
    const int n = static_cast<int>(before->channelDepth.size());
    std::vector<float> depthBefore = before->channelDepth;
    std::vector<float> flowBefore = before->discharge;

    for (int i = 0; i < 40; i++) {
        grid.step(slice);
    }
    auto after = grid.publishSnapshot();

    // Cells that had a channel and a river, and now have no river.
    const float threshold = grid.getConstants().channelThreshold;
    int abandoned = 0;
    int erased = 0;
    int realValleysErased = 0;
    float deepestErased = 0.0f;
    double erasedSum = 0.0;
    double keptSum = 0.0;
    double hadSum = 0.0;

    for (int i = 0; i < n; i++) {
        // Still dry land afterwards. A valley that has been drowned by the sea
        // or buried under its own delta has genuinely lost its channel, and
        // counting those as erasures would hide whatever else is going on.
        const bool hadRiver = flowBefore[i] >= threshold && depthBefore[i] > 1.0f;
        const bool hasRiver = after->discharge[i] >= threshold;
        const bool stillLand = after->elevation[i] > 0.0f;
        if (!hadRiver || hasRiver || !stillLand) {
            continue;
        }
        abandoned++;
        hadSum += depthBefore[i];
        keptSum += after->channelDepth[i];
        if (after->channelDepth[i] <= 0.01f) {
            erased++;
            deepestErased = std::max(deepestErased, depthBefore[i]);
            erasedSum += depthBefore[i];
            if (depthBefore[i] > 20.0f) {
                realValleysErased++;
            }
        }
    }

    const float retained = hadSum > 0.0 ? static_cast<float>(keptSum / hadSum) : 0.0f;
    std::printf("    %d valleys lost their river over %.1f My\n", abandoned, 40.0f * slice);
    std::printf("    %.0f%% of their depth is still in the ground, %d erased outright\n",
                retained * 100.0f, erased);

    std::printf("    of the erased: mean %.1f m, deepest %.0f m, %d were over 20 m\n",
                erased > 0 ? static_cast<float>(erasedSum / erased) : 0.0f,
                deepestErased, realValleysErased);

    check(abandoned > 5, "rivers do move, so there is something to measure");
    // Shallow scratches do vanish, and should: a few metres of incision in a
    // channel a few hundred metres wide really is filled in by the hillsides
    // within a few tens of thousands of years. What must not vanish is anything
    // that was a valley. Before the width floor was fixed the deepest thing
    // erased was a hundred and sixty metres deep.
    check(deepestErased < 30.0f, "nothing that was actually a valley is wiped out");
    check(retained > 0.6f, "an abandoned valley is still mostly there");
}

void testDividesDoNotFlap() {
    std::printf("Whether drainage divides settle or oscillate\n");

    // Capture is supposed to be a one-way event. Once a stream cuts below its
    // neighbour and takes its headwaters, the neighbour cannot take them back
    // without cutting lower still - so a divide moves, stays moved, and the
    // catchment it handed over stays handed over.
    //
    // What the largest river actually does is gain and lose half its catchment
    // repeatedly without moving anywhere, which is a divide flapping rather
    // than migrating. This separates the two: a receiver change that goes back
    // to where it was is an oscillation, and a receiver change that does not is
    // reorganisation. Only the first is a bug.
    simulation::CrustGrid grid(1000000.0f, 61, 6, 12);

    const float slice = 0.02f;
    for (int i = 0; i < 60; i++) {
        grid.step(slice);
    }

    const int n = static_cast<int>(grid.getCells().size());
    std::vector<int> previous(n, -2);
    std::vector<int> beforeThat(n, -2);
    std::vector<double> dischargeSum(n, 0.0);

    long long changes = 0;
    long long reversals = 0;

    // By how much the new receiver actually beat the old one. If a drainage
    // network is rewiring because one neighbour is genuinely lower, the margin
    // is metres of relief; if it is rewiring because two neighbours are level
    // to within the noise of the last erosion step, the margin is centimetres
    // and the choice is a coin toss that happens to be made afresh every step.
    long long tinyMargin = 0;
    double marginSum = 0.0;

    // How much of the churn is inside filled depressions.
    //
    // A lake surface is perfectly flat by construction - the fill puts every
    // cell in the basin at exactly the spill height - so steepest descent has
    // nothing to choose between and the receiver comes from whichever way the
    // flood happened to arrive. That order is re-derived from scratch every
    // step, and a tie broken arbitrarily is a tie broken differently each time.
    long long lakeChanges = 0;

    // What any of this is for.
    //
    // Counting receiver changes is a proxy. The symptom is that the largest
    // river on the planet gains and loses half its catchment while staying
    // exactly where it is, so that is the number to judge by: how much the
    // biggest catchment moves from one step to the next, as a fraction of
    // itself. A drainage network that has settled should change size slowly
    // even while individual headwaters are still being traded.
    float previousBiggest = 0.0f;
    double swingSum = 0.0;
    float worstSwing = 0.0f;
    int swingSamples = 0;
    long long headwaterChanges = 0, headwaterReversals = 0;
    long long trunkChanges = 0, trunkReversals = 0;

    const int steps = 40;
    for (int step = 0; step < steps; step++) {
        grid.step(slice);
        auto snapshot = grid.publishSnapshot();
        if (static_cast<int>(snapshot->flowsInto.size()) != n) {
            continue;
        }

        float biggest = 0.0f;
        for (float flow : snapshot->discharge) {
            biggest = std::max(biggest, flow);
        }
        if (previousBiggest > 0.0f && biggest > 0.0f) {
            const float swing = std::fabs(biggest - previousBiggest) /
                                std::max(previousBiggest, 1.0f);
            swingSum += swing;
            worstSwing = std::max(worstSwing, swing);
            swingSamples++;
        }
        previousBiggest = biggest;

        for (int i = 0; i < n; i++) {
            const int now = snapshot->flowsInto[i];
            const float flow = i < static_cast<int>(snapshot->discharge.size())
                                   ? snapshot->discharge[i]
                                   : 0.0f;
            dischargeSum[i] += flow;

            if (previous[i] != -2 && now != previous[i]) {
                changes++;
                if (i < static_cast<int>(snapshot->lakeDepth.size()) &&
                    snapshot->lakeDepth[i] > 0.0f) {
                    lakeChanges++;
                }
                // Back to the receiver it had before the last change: the
                // signature of a divide that cannot make up its mind.
                const bool wentBack = now == beforeThat[i];
                if (wentBack) {
                    reversals++;
                }

                // How much lower the new receiver is than the one abandoned,
                // on the surface the routing actually saw.
                if (previous[i] >= 0 && now >= 0 &&
                    previous[i] < n && now < n &&
                    static_cast<int>(snapshot->routedSurface.size()) == n) {
                    const float margin =
                        snapshot->routedSurface[previous[i]] - snapshot->routedSurface[now];
                    marginSum += std::fabs(margin);
                    if (std::fabs(margin) < 1.0f) {
                        tinyMargin++;
                    }
                }
                if (flow < 10.0f) {
                    headwaterChanges++;
                    if (wentBack) headwaterReversals++;
                } else {
                    trunkChanges++;
                    if (wentBack) trunkReversals++;
                }
                beforeThat[i] = previous[i];
            }
            previous[i] = now;
        }
    }

    const double reversalShare = changes > 0 ? double(reversals) / changes : 0.0;
    std::printf("    %lld receiver changes over %.1f My, %lld of them reversals (%.0f%%)\n",
                changes, steps * slice, reversals, reversalShare * 100.0);
    std::printf("    headwaters: %lld changes, %.0f%% reversals\n",
                headwaterChanges,
                headwaterChanges > 0 ? 100.0 * headwaterReversals / headwaterChanges : 0.0);
    std::printf("    trunks:     %lld changes, %.0f%% reversals\n",
                trunkChanges,
                trunkChanges > 0 ? 100.0 * trunkReversals / trunkChanges : 0.0);

    std::printf("    %lld changes (%.0f%%) are inside filled depressions\n",
                lakeChanges, changes > 0 ? 100.0 * lakeChanges / changes : 0.0);
    std::printf("    mean margin %.2f m, %lld changes (%.0f%%) decided by under a metre\n",
                changes > 0 ? marginSum / changes : 0.0, tinyMargin,
                changes > 0 ? 100.0 * tinyMargin / changes : 0.0);

    const double meanSwing = swingSamples > 0 ? swingSum / swingSamples : 0.0;
    std::printf("    largest catchment moves %.0f%% per step on average, worst %.0f%%\n",
                meanSwing * 100.0, worstSwing * 100.0);

    check(changes > 0, "the network does change, so there is something to judge");
    check(meanSwing < 0.12, "the biggest river keeps its size from one step to the next");

    // A network where most changes undo the previous one is not reorganising,
    // it is vibrating. Some reversals are legitimate - ground genuinely moves
    // back and forth as plates and isostasy work - but they should be the
    // minority.
    check(reversalShare < 0.35, "drainage changes mostly stick rather than undoing themselves");
}

void testRiversSurviveBeingRunFast() {
    std::printf("Whether a river keeps its size when time runs quickly\n");

    // The regime nobody was measuring.
    //
    // Every drainage measurement so far has used the landscape timestep, where
    // the routed model runs every step and the network is maintained
    // continuously. That is not the setting the planet is normally watched at.
    // At a million years a second the steps are large, erosion is the bulk
    // approximation, and the network is rebuilt from scratch on an interval -
    // so between one rebuild and the next, two million years of plate motion
    // and denudation happen to a network that is not being updated, and the
    // rebuild lands on terrain that has moved underneath it.
    //
    // If the size of the largest river jumps at each rebuild, that is a river
    // visibly appearing and disappearing, which is the complaint.
    struct Result {
        const char* name;
        float slice;
        double meanSwing;
        float worstSwing;
    };

    const float slices[] = {0.02f, 0.5f};
    const char* names[] = {"landscape", "geological"};
    Result results[2];

    for (int variant = 0; variant < 2; variant++) {
        simulation::CrustGrid grid(1000000.0f, 61, 6, 12);
        const float slice = slices[variant];

        // The same amount of simulated time either way, so the two are
        // comparable as histories rather than as step counts.
        const int warmup = static_cast<int>(1.2f / slice);
        for (int i = 0; i < warmup; i++) {
            grid.step(slice);
        }

        float previous = 0.0f;
        double swingSum = 0.0;
        float worst = 0.0f;
        int samples = 0;

        const int steps = static_cast<int>(4.0f / slice);
        for (int i = 0; i < steps; i++) {
            grid.step(slice);
            auto snapshot = grid.publishSnapshot();
            float biggest = 0.0f;
            for (float flow : snapshot->discharge) {
                biggest = std::max(biggest, flow);
            }
            if (previous > 0.0f && biggest > 0.0f) {
                const float swing = std::fabs(biggest - previous) / std::max(previous, 1.0f);
                swingSum += swing;
                worst = std::max(worst, swing);
                samples++;
            }
            previous = biggest;
        }

        results[variant] = {names[variant], slice,
                            samples > 0 ? swingSum / samples : 0.0, worst};
        std::printf("    %-11s %.2f My steps: largest catchment moves %.0f%% per step, "
                    "worst %.0f%%\n",
                    names[variant], slice, results[variant].meanSwing * 100.0,
                    worst * 100.0);
    }

    // Judged per million years rather than per step, because the two regimes
    // take very different numbers of steps to cover the same history and a
    // per-step figure would flatter whichever takes more of them.
    const double landscapePerMy = results[0].meanSwing / results[0].slice;
    const double geologicalPerMy = results[1].meanSwing / results[1].slice;
    std::printf("    per million years: landscape %.0f%%, geological %.0f%%\n",
                landscapePerMy * 100.0, geologicalPerMy * 100.0);

    check(results[0].meanSwing > 0.0, "there is a river to measure");
    check(geologicalPerMy < landscapePerMy * 3.0,
          "running time quickly does not make rivers far less stable per million years");
}

void testHowMuchRewiringIsWarranted() {
    std::printf("Whether the network changes its mind for a reason\n");

    // The question that decides whether the routing needs rebuilding or
    // replacing.
    //
    // Every step, every cell picks whichever neighbour is lowest. That is an
    // argmax over near-equal numbers taken from a field that moves every step,
    // so it will change its mind whether or not anything has happened. A change
    // is warranted only if the new receiver is lower than the abandoned one by
    // more than the depth of the channel the water is sitting in - otherwise
    // the flow could not have climbed out of its own bed to get there, and the
    // change is an artefact of recomputation rather than an event.
    //
    // If most changes are unwarranted, no amount of biasing a from-scratch
    // rebuild towards its previous answer will fix it, and the network has to
    // become state that is edited rather than output that is regenerated.
    simulation::CrustGrid grid(1000000.0f, 61, 6, 12);

    const float slice = 0.02f;
    for (int i = 0; i < 60; i++) {
        grid.step(slice);
    }

    grid.resetDrainageAudit();
    for (int i = 0; i < 40; i++) {
        grid.step(slice);
    }

    const auto& audit = grid.getDrainageAudit();
    const double changeShare =
        audit.routed > 0 ? double(audit.changed) / audit.routed : 0.0;
    const double warrantedShare =
        audit.changed > 0 ? double(audit.warranted) / audit.changed : 0.0;

    std::printf("    %lld routing decisions, %lld changed (%.1f%%)\n",
                audit.routed, audit.changed, changeShare * 100.0);
    std::printf("    %lld of those changes were warranted (%.0f%%), "
                "mean channel abandoned %.1f m\n",
                audit.warranted, warrantedShare * 100.0,
                audit.changed > 0 ? audit.abandonedDepth / audit.changed : 0.0);

    check(audit.routed > 0, "the routing ran");

    // Recorded rather than enforced for now. This is the measurement that
    // decides the design, and the number it reports is the case for changing
    // it.
    std::printf("    %s\n",
                warrantedShare < 0.5
                    ? "most rewiring is unwarranted: the network should be edited, not rebuilt"
                    : "most rewiring is warranted: rebuilding is defensible");
}

void testWhetherFinerCellsSteadyTheRivers() {
    std::printf("Whether rivers hold their size better on a finer grid\n");

    // The measurement that decides whether this is a bug or a resolution.
    //
    // Two thirds of drainage rewiring turns out to be warranted - the water
    // genuinely can climb out of the six metre channel it is in and go
    // elsewhere - and a cell rewires about once per million years, which is
    // roughly the rate at which real divides migrate seventeen kilometres. So
    // the network may not be moving too much at all.
    //
    // What it is doing is moving in units that are far too large. A network
    // fifty cells deep loses a visible fraction of itself every time one
    // headwater switches, because one cell is a large share of fifty. On a
    // finer grid the same physical divide migration moves the same amount of
    // ground and a much smaller share of the river.
    //
    // If that is right, the swing falls with resolution while the rewiring rate
    // stays put, and no amount of work on the routing will help.
    struct Result {
        int level;
        double swingPerMy;
        double changeShare;
        float typicalBiggest;
    };
    Result results[2];

    const int levels[] = {6, 7};
    for (int variant = 0; variant < 2; variant++) {
        simulation::CrustGrid grid(1000000.0f, 61, levels[variant], 12);

        const float slice = 0.05f;
        for (int i = 0; i < 20; i++) {
            grid.step(slice);
        }

        grid.resetDrainageAudit();
        float previous = 0.0f;
        double swingSum = 0.0, biggestSum = 0.0;
        int samples = 0;

        const int steps = 40;
        for (int i = 0; i < steps; i++) {
            grid.step(slice);
            auto snapshot = grid.publishSnapshot();
            float biggest = 0.0f;
            for (float flow : snapshot->discharge) {
                biggest = std::max(biggest, flow);
            }
            if (previous > 0.0f && biggest > 0.0f) {
                swingSum += std::fabs(biggest - previous) / std::max(previous, 1.0f);
                biggestSum += biggest;
                samples++;
            }
            previous = biggest;
        }

        const auto& audit = grid.getDrainageAudit();
        results[variant] = {
            levels[variant],
            samples > 0 ? (swingSum / samples) / slice : 0.0,
            audit.routed > 0 ? double(audit.changed) / audit.routed : 0.0,
            samples > 0 ? static_cast<float>(biggestSum / samples) : 0.0f};

        std::printf("    level %d: largest catchment %.0f cells, moves %.0f%% per My, "
                    "%.1f%% of cells rewire per step\n",
                    levels[variant], results[variant].typicalBiggest,
                    results[variant].swingPerMy * 100.0,
                    results[variant].changeShare * 100.0);
    }

    const double improvement =
        results[0].swingPerMy > 0.0 ? results[1].swingPerMy / results[0].swingPerMy : 1.0;
    std::printf("    finer grid swings %.0f%% as much, and rewires %.0fx as often per cell\n",
                improvement * 100.0,
                results[0].changeShare > 0.0 ? results[1].changeShare / results[0].changeShare
                                             : 0.0);

    check(results[0].swingPerMy > 0.0 && results[1].swingPerMy > 0.0,
          "both grids produced rivers to measure");
    std::printf("    %s\n",
                improvement < 0.7
                    ? "resolution steadies the rivers: this is a grid problem, not a routing one"
                    : "resolution does not steady the rivers: the instability is in the routing");
}

void testDenudationMatchesTheRealWorld() {
    std::printf("Whether continents wear down at the rate continents wear down\n");

    // The one number in this model that can be checked against the world.
    //
    // Everything built on top of erosion - valleys, sediment, where rivers run,
    // whether a mountain range is in steady state - assumes the rate is roughly
    // right, and until now that assumption had never been tested against
    // anything except my own judgement. Continental denudation is measured, not
    // modelled: thirty to a hundred metres per million years for ordinary
    // continental interiors, several hundred to a thousand for wet active
    // orogens. A planet-wide average should land in the lower part of that.
    //
    // If this is wrong the whole plan of treating fine detail as a cosmetic
    // front on sound coarse geology is built on sand, so it is worth knowing
    // before building anything else on it.
    // Level 5, not 6, and the reason is worth stating carefully because the
    // justification for it turned out to be half wrong.
    //
    // The argument was that a global mass budget cannot depend on how finely
    // the sphere is divided, so the finer grid buys nothing here and costs
    // eight times the work - four times the cells and half the stable timestep.
    // Denudation bears that out exactly: ninety-four metres per million years
    // at level 6, ninety-seven at level 5.
    //
    // The crustal budget does not. Continental crust changes by minus eighteen
    // per cent over a hundred million years at level 6 and plus two at level 5,
    // and sea level moves three hundred metres in opposite directions. A mass
    // balance should not do that, and the discrepancy is a defect in its own
    // right - most likely in the capacity rule, which sheds crust per cell and
    // so has an aggregate effect that scales with how many cells there are.
    // It is recorded in CLAUDE.md rather than chased here.
    //
    // These tests stay at level 5 anyway, because what they assert is
    // qualitative - that land does not drain away, and that it responds to arc
    // production - and that holds at both resolutions. What must not be read
    // off them is an absolute number: the arc production ratio was calibrated
    // on level 6, which is what the application runs.
    simulation::CrustGrid grid(1000000.0f, 61, 5, 12);

    // Long enough for the initial condition to stop dominating.
    for (int i = 0; i < 40; i++) {
        grid.step(0.5f);
    }

    grid.resetErosionBudget();
    const auto before = grid.computeStats();

    const float span = 20.0f;
    for (int i = 0; i < 40; i++) {
        grid.step(span / 40.0f);
    }

    const auto after = grid.computeStats();
    const auto& budget = grid.getErosionBudget();

    auto snapshot = grid.publishSnapshot();
    int land = 0;
    for (float elevation : snapshot->elevation) {
        if (elevation > 0.0f) {
            land++;
        }
    }
    const double landArea = double(land) * grid.getCellArea();

    // Gross removal spread over the land it came off.
    const double denudation =
        landArea > 0.0 && budget.simulatedTime > 0.0f
            ? budget.eroded / landArea / budget.simulatedTime
            : 0.0;

    // What the budget failed to put back down again. Erosion that leaks is
    // indistinguishable from erosion that works until continents evaporate.
    const double leak =
        budget.eroded > 0.0 ? std::fabs(budget.eroded - budget.deposited) / budget.eroded : 0.0;

    std::printf("    over %.1f My: %.3e m3 eroded, %.3e m3 deposited, %.4f%% unaccounted\n",
                budget.simulatedTime, budget.eroded, budget.deposited, leak * 100.0);
    std::printf("    %d land cells, denudation %.1f m/My\n", land, denudation);
    std::printf("    highest ground %.0f m -> %.0f m, land %.1f%% -> %.1f%%\n",
                before.maxElevation, after.maxElevation,
                before.landFraction * 100.0f, after.landFraction * 100.0f);

    check(budget.eroded > 0.0, "erosion is actually removing rock");
    check(leak < 0.01, "everything eroded is deposited again");

    // The observed range, generously bounded: this is a different planet with
    // its own rainfall and relief, so the claim is that it is the right order
    // of magnitude, not that it matches Earth.
    check(denudation > 5.0 && denudation < 500.0,
          "continents wear down at a rate the real world would recognise");

    // Steady state, not runaway. A range under sustained convergence should
    // settle where uplift and erosion balance; it should not grow without
    // limit, and it should not be planed flat either.
    const float heightChange =
        std::fabs(after.maxElevation - before.maxElevation) / std::max(before.maxElevation, 1.0f);
    std::printf("    highest ground moved %.0f%% over %.0f My\n", heightChange * 100.0f, span);
    check(heightChange < 0.5f, "mountains neither run away nor collapse");
}

void testContinentsDoNotDrown() {
    std::printf("Whether the planet quietly drowns itself\n");

    // Land fell from twenty-three per cent of the surface to eighteen over
    // twenty million years while measuring something else. Extrapolated that
    // empties the planet, and continents do not do that - erosion strips them
    // and arc magmatism builds them back, which is why there has been dry land
    // for four billion years.
    //
    // Three things could be responsible and they need separating before
    // anything is changed: the continents could be losing volume, the ocean
    // basins could be filling with the sediment that came off them and pushing
    // sea level up, or the land could simply be wearing flat. They imply
    // completely different fixes.
    // Level 5, not 6, and the reason is worth stating carefully because the
    // justification for it turned out to be half wrong.
    //
    // The argument was that a global mass budget cannot depend on how finely
    // the sphere is divided, so the finer grid buys nothing here and costs
    // eight times the work - four times the cells and half the stable timestep.
    // Denudation bears that out exactly: ninety-four metres per million years
    // at level 6, ninety-seven at level 5.
    //
    // The crustal budget does not. Continental crust changes by minus eighteen
    // per cent over a hundred million years at level 6 and plus two at level 5,
    // and sea level moves three hundred metres in opposite directions. A mass
    // balance should not do that, and the discrepancy is a defect in its own
    // right - most likely in the capacity rule, which sheds crust per cell and
    // so has an aggregate effect that scales with how many cells there are.
    // It is recorded in CLAUDE.md rather than chased here.
    //
    // These tests stay at level 5 anyway, because what they assert is
    // qualitative - that land does not drain away, and that it responds to arc
    // production - and that holds at both resolutions. What must not be read
    // off them is an absolute number: the arc production ratio was calibrated
    // on level 6, which is what the application runs.
    simulation::CrustGrid grid(1000000.0f, 61, 5, 12);

    for (int i = 0; i < 10; i++) {
        grid.step(2.0f);
    }

    grid.resetCrustBudget();

    struct Sample {
        float time;
        float land;
        float seaLevel;
        float continental;
        float oceanic;
        float meanElevation;
    };
    std::vector<Sample> history;

    const float slice = 2.0f;
    for (int block = 0; block <= 10; block++) {
        if (block > 0) {
            for (int i = 0; i < 5; i++) {
                grid.step(slice);
            }
        }
        const auto stats = grid.computeStats();
        auto snapshot = grid.publishSnapshot();
        history.push_back({grid.getSimulationTime(), stats.landFraction, snapshot->seaLevel,
                           stats.continentalVolume, stats.oceanicVolume,
                           stats.meanElevation});
    }

    for (const Sample& s : history) {
        std::printf("    %6.0f My  land %5.1f%%  sea %7.0f m  continental %.4e  "
                    "oceanic %.4e  mean %6.0f m\n",
                    s.time, s.land * 100.0f, s.seaLevel, s.continental, s.oceanic,
                    s.meanElevation);
    }

    const Sample& first = history.front();
    const Sample& last = history.back();
    const float span = last.time - first.time;

    const float landChange = last.land - first.land;
    const float seaRise = last.seaLevel - first.seaLevel;
    const float continentalChange =
        first.continental > 0.0f ? (last.continental - first.continental) / first.continental
                                 : 0.0f;

    std::printf("    over %.0f My: land %+.1f points, sea level %+.0f m (%.1f m/My), "
                "continental crust %+.1f%%\n",
                span, landChange * 100.0f, seaRise, seaRise / std::max(span, 1.0f),
                continentalChange * 100.0f);

    // Which path the crust actually took. Two guesses at this were wrong -
    // sediment routing and the sea level budget - so the answer comes from the
    // ledger rather than from a hypothesis.
    const auto& budget = grid.getCrustBudget();
    const double perMy = budget.simulatedTime > 0.0f ? 1.0 / budget.simulatedTime : 0.0;
    std::printf("    crust per My: subducted %.3e, delaminated %.3e, rifted %.3e, "
                "arc returned %.3e\n",
                budget.subducted * perMy, budget.delaminated * perMy,
                budget.riftedAway * perMy, budget.arcFromMantle * perMy);

    const double destroyed = budget.subducted + budget.delaminated + budget.riftedAway;
    const char* dominant = "subduction";
    double worst = budget.subducted;
    if (budget.delaminated > worst) { worst = budget.delaminated; dominant = "delamination"; }
    if (budget.riftedAway > worst) { worst = budget.riftedAway; dominant = "rifting"; }
    std::printf("    %.0f%% of crust destruction is %s; arcs return %.0f%% of the total\n",
                destroyed > 0.0 ? 100.0 * worst / destroyed : 0.0, dominant,
                destroyed > 0.0 ? 100.0 * budget.arcFromMantle / destroyed : 0.0);

    // Which term is doing it.
    std::printf("    %s\n",
                std::fabs(continentalChange) > 0.05f
                    ? "continental crust is being lost: the problem is the crustal budget"
                    : "continental crust is holding: the problem is sea level or relief");

    check(!history.empty(), "there is a history to judge");
    check(last.land > 0.05f, "there is still land at the end");

    // Continents survive on Earth for billions of years. Over a hundred million
    // they should not lose a large fraction of their area.
    check(landChange > -0.10f, "the continents are not draining away");
}

void testWhatKeepsContinentsAbove() {
    std::printf("Whether arc production is what sets how much land there is\n");

    // Continental crust falls by forty per cent over a hundred million years
    // and then steadies, so the planet is not leaking - it is relaxing to an
    // equilibrium, and the equilibrium is a tenth of the surface as land where
    // Earth manages nearly a third.
    //
    // Erosion strips continents and arc magmatism rebuilds them, so where that
    // balance settles is set by how much of what goes down comes back up. That
    // ratio is the least constrained number in the model - its own comment says
    // so - and the observed range on Earth is one to two cubic kilometres a
    // year returned against about three subducted, which is a third to two
    // thirds, not the quarter assumed here.
    //
    // Calibrating it against an outcome is legitimate in the way that guessing
    // it is not: it is the same standing as the asthenosphere viscosity, which
    // is set so that Earth-sized plates come out moving at the speeds we
    // measure. But it is only legitimate if the outcome actually responds to
    // it, which is what this checks first.
    const float ratios[] = {0.25f, 0.5f, 0.75f};
    float landAtEnd[3] = {0.0f, 0.0f, 0.0f};
    float continentalAtEnd[3] = {0.0f, 0.0f, 0.0f};

    for (int variant = 0; variant < 3; variant++) {
    // Level 5, not 6, and the reason is worth stating carefully because the
    // justification for it turned out to be half wrong.
    //
    // The argument was that a global mass budget cannot depend on how finely
    // the sphere is divided, so the finer grid buys nothing here and costs
    // eight times the work - four times the cells and half the stable timestep.
    // Denudation bears that out exactly: ninety-four metres per million years
    // at level 6, ninety-seven at level 5.
    //
    // The crustal budget does not. Continental crust changes by minus eighteen
    // per cent over a hundred million years at level 6 and plus two at level 5,
    // and sea level moves three hundred metres in opposite directions. A mass
    // balance should not do that, and the discrepancy is a defect in its own
    // right - most likely in the capacity rule, which sheds crust per cell and
    // so has an aggregate effect that scales with how many cells there are.
    // It is recorded in CLAUDE.md rather than chased here.
    //
    // These tests stay at level 5 anyway, because what they assert is
    // qualitative - that land does not drain away, and that it responds to arc
    // production - and that holds at both resolutions. What must not be read
    // off them is an absolute number: the arc production ratio was calibrated
    // on level 6, which is what the application runs.
        simulation::CrustGrid grid(1000000.0f, 61, 5, 12);
        grid.getConstants().arcProductionRatio = ratios[variant];

        for (int i = 0; i < 40; i++) {
            grid.step(2.0f);
        }

        const auto stats = grid.computeStats();
        landAtEnd[variant] = stats.landFraction;
        continentalAtEnd[variant] = static_cast<float>(stats.continentalVolume);

        std::printf("    arc ratio %.2f: land %.1f%% after 80 My, continental %.4e\n",
                    ratios[variant], stats.landFraction * 100.0f, stats.continentalVolume);
    }

    const float response = landAtEnd[2] - landAtEnd[0];
    std::printf("    tripling the return changes land by %+.1f points\n", response * 100.0f);

    check(landAtEnd[0] > 0.0f, "the planet still has land at the lowest ratio");
    std::printf("    %s\n",
                response > 0.03f
                    ? "land responds to arc production: this is a calibration, not a leak"
                    : "land barely responds: something else sets the equilibrium");
}

void testWhetherTheSheddingIsNoise() {
    std::printf("Whether the capacity rule is shedding noise or shedding rock\n");

    // The account so far is that cell thickness is projected from scattered
    // parcels, so it carries sampling noise, and a one-sided clamp on a noisy
    // quantity takes the upper tail and leaves the lower. It explains the
    // resolution dependence and it has never been tested.
    //
    // The direct test changes the noise without changing any physics. More
    // parcels per cell means the same rock read from a larger sample, so the
    // projection error falls as one over the square root of the count while
    // every rate and threshold in the model stays exactly where it was. If the
    // shedding is noise, it falls with the parcel count. If it does not, the
    // account is wrong and the resolution dependence is something else.
    struct Result {
        int markers;
        double shedPerMy;
        double marginalShare;
        double marginalVolumeShare;
    };
    Result results[2];

    const int counts[] = {6, 12};
    for (int variant = 0; variant < 2; variant++) {
        // Plate count held fixed. The first version of this varied the fourth
        // argument believing it was the parcel count; it is the number of
        // plates, so what got measured was three times the plate boundaries
        // giving three and a half times the subduction - entirely sensible, and
        // nothing at all to do with the question.
        simulation::CrustGrid grid(1000000.0f, 61, 5, 12, counts[variant]);

        for (int i = 0; i < 10; i++) {
            grid.step(1.0f);
        }
        grid.resetCrustBudget();
        for (int i = 0; i < 20; i++) {
            grid.step(1.0f);
        }

        const auto& b = grid.getCrustBudget();
        const double perMy = b.simulatedTime > 0.0f ? 1.0 / b.simulatedTime : 0.0;
        const double shed = b.subducted + b.delaminated;
        results[variant] = {
            counts[variant], shed * perMy,
            b.shedEvents > 0 ? double(b.marginalEvents) / double(b.shedEvents) : 0.0,
            shed > 0.0 ? b.marginalVolume / shed : 0.0};

        std::printf("    %2d parcels/cell: shed %.3e m3/My, %.0f%% of events marginal, "
                    "%.0f%% of volume\n",
                    counts[variant], results[variant].shedPerMy,
                    results[variant].marginalShare * 100.0,
                    results[variant].marginalVolumeShare * 100.0);
    }

    const double change =
        results[0].shedPerMy > 0.0 ? results[1].shedPerMy / results[0].shedPerMy : 1.0;
    std::printf("    doubling the parcels changes shedding to %.2fx\n", change);

    check(results[0].shedPerMy > 0.0, "crust is being shed at all");
    std::printf("    %s\n",
                change < 0.8
                    ? "shedding falls with the sample size: it is projection noise"
                    : "shedding is unmoved by the sample size: it is not noise, look elsewhere");
}

void testCrustBudgetDoesNotDependOnCellSize() {
    std::printf("Whether how finely the sphere is divided changes the crust budget\n");

    // Mass balance should not care about cell size, and this one does:
    // continental crust changes by minus eighteen per cent over a hundred
    // million years at level 6 and plus two at level 5.
    //
    // The suspicion is the three one-sided rules in reconcileCrust. Each looks
    // at a cell's thickness - which is a projection from scattered parcels, so
    // it carries sampling noise - and acts only when it falls on one side:
    // crust above the column's capacity is shed to the mantle, crust below the
    // thickness of ocean floor has melt injected, and a hole has new seafloor
    // created in it. A one-sided limit on a noisy estimate is a biased
    // operator by construction. It takes the upper tail and leaves the lower,
    // so the mean walks, and how fast it walks depends on the noise rather than
    // on any physics.
    //
    // If that is what is happening, the shedding and injection terms will be
    // larger per unit of planet at the finer grid, where each cell's reading is
    // drawn from a smaller sample of rock.
    struct Result {
        int level;
        double sheddingPerMy;
        double riftingPerMy;
        double meltPerMy;
        double arcPerMy;
    };
    Result results[2];

    const int levels[] = {5, 6};
    for (int variant = 0; variant < 2; variant++) {
        simulation::CrustGrid grid(1000000.0f, 61, levels[variant], 12);

        // Past the initial transient, so what is measured is the running state.
        for (int i = 0; i < 10; i++) {
            grid.step(1.0f);
        }
        grid.resetCrustBudget();
        for (int i = 0; i < 20; i++) {
            grid.step(1.0f);
        }

        const auto& b = grid.getCrustBudget();
        const double perMy = b.simulatedTime > 0.0f ? 1.0 / b.simulatedTime : 0.0;
        results[variant] = {levels[variant],
                            (b.subducted + b.delaminated) * perMy,
                            b.riftedAway * perMy,
                            b.meltFromMantle * perMy,
                            b.arcFromMantle * perMy};

        // How far over capacity a shedding column typically is.
        //
        // If the resolution dependence were noise, this would be about the same
        // at both grids and there would simply be more events at the finer one.
        // If it is convergence concentrating - over-thickening happens along
        // plate boundaries, which is a line, while capacity is tested per cell,
        // so halving the cell width doubles the thickness the same convergence
        // produces - then the typical excess scales with the grid and this is
        // where it shows.
        const double meanExcess =
            b.shedEvents > 0 ? b.excessThickness / double(b.shedEvents) : 0.0;
        std::printf("    level %d: shed %.3e, rifted %.3e, arc %.3e m3/My; "
                    "%lld events, mean excess %.0f m\n",
                    levels[variant], results[variant].sheddingPerMy,
                    results[variant].riftingPerMy, results[variant].arcPerMy,
                    b.shedEvents, meanExcess);
    }

    const auto ratio = [](double fine, double coarse) {
        return coarse > 0.0 ? fine / coarse : 0.0;
    };
    const double shedRatio = ratio(results[1].sheddingPerMy, results[0].sheddingPerMy);
    const double riftRatio = ratio(results[1].riftingPerMy, results[0].riftingPerMy);

    std::printf("    level 6 against level 5: shedding %.2fx, rifting %.2fx\n",
                shedRatio, riftRatio);

    check(results[0].sheddingPerMy > 0.0, "crust is being shed at all");

    // The same planet, so the same amount of rock should be moving whichever
    // way it is divided up. Recorded rather than enforced while the cause is
    // still open - the number is the finding.
    std::printf("    %s\n",
                (shedRatio > 1.5 || riftRatio > 1.5)
                    ? "the finer grid destroys more crust: the one-sided rules are the cause"
                    : "the rules are not obviously resolution-dependent: look elsewhere");
}

void testDrainageReorganises() {
    std::printf("Whether drainage networks reorganise on their own\n");
    simulation::CrustGrid grid(1000000.0f, 19, 6, 12);

    // Long enough for relief to exist to drain.
    for (int i = 0; i < 6; i++) {
        grid.step(1.0f);
    }

    // Steps short enough for the routed erosion model, so incision is
    // discharge-driven and the network is rebuilt from the ground it just cut.
    const float slice = grid.getConstants().routedErosionBelow * 0.5f;

    auto first = grid.publishSnapshot();
    std::vector<int32_t> receiverBefore = first->flowsInto;
    std::vector<float> dischargeBefore = first->discharge;

    const size_t n = grid.getCells().size();

    // Changes per step, early and late, rather than how many cells ever
    // changed.
    //
    // The cumulative count cannot tell a network settling once from a network
    // churning forever, and those are opposite findings. Introducing channel
    // entrenchment shifts every course that was being held only by a
    // millimetre of gradient - once - and a metric that counts each cell once
    // reports that single settling as though it were permanent instability.
    const auto churnPerStep = [&](simulation::CrustGrid& g, int steps, int sampleLast) {
        auto snap = g.publishSnapshot();
        std::vector<int32_t> previous = snap->flowsInto;

        int earlyChanges = 0, earlySteps = 0;
        int lateChanges = 0, lateSteps = 0;

        for (int s = 0; s < steps; s++) {
            g.step(slice);
            auto now = g.publishSnapshot();

            int changes = 0;
            for (size_t i = 0; i < previous.size(); i++) {
                if (now->flowsInto[i] >= 0 && previous[i] >= 0 &&
                    now->flowsInto[i] != previous[i]) {
                    changes++;
                }
            }
            if (s < 3) {
                earlyChanges += changes;
                earlySteps++;
            } else if (s >= steps - sampleLast) {
                lateChanges += changes;
                lateSteps++;
            }
            previous = now->flowsInto;
        }

        int draining = 0;
        for (int32_t r : previous) {
            if (r >= 0) draining++;
        }
        return std::array<float, 3>{
            earlySteps > 0 ? static_cast<float>(earlyChanges) / earlySteps : 0.0f,
            lateSteps > 0 ? static_cast<float>(lateChanges) / lateSteps : 0.0f,
            static_cast<float>(draining)};
    };

    constexpr int STEPS = 40;
    const auto moving = churnPerStep(grid, STEPS, 8);

    std::printf("  crust moving:      %.0f changes/step early, %.0f late, of %.0f draining\n",
                moving[0], moving[1], moving[2]);

    check(moving[2] > 0.0f, "there is a network to reorganise");

    // The same measurement with the crust held still.
    //
    // A drainage network cannot be more persistent than the ground it drains.
    // Plates move half a cell per step by construction, and the projection is
    // rebuilt from the parcels each time, so the terrain itself changes by
    // hundreds of metres in places - and a river cannot be expected to keep a
    // course over ground that is no longer there. Stopping the crust separates
    // a network responding to real change from a network that cannot hold a
    // course over stable ground.
    simulation::CrustGrid still(1000000.0f, 19, 6, 12);
    for (int i = 0; i < 6; i++) {
        still.step(1.0f);
    }
    // Far enough ahead that the crust never moves again during the measurement.
    still.getConstants().tectonicInterval = 1.0e6f;

    const auto held = churnPerStep(still, STEPS, 8);

    std::printf("  crust held still:  %.0f changes/step early, %.0f late, of %.0f draining\n",
                held[0], held[1], held[2]);

    // Over ground that is not moving, a settled network should barely change at
    // all: erosion only deepens the channels it already has. What churn there is
    // in the early steps is the network settling into its entrenched courses
    // once, which is a different thing from churning forever - and telling those
    // two apart is the whole reason this measures changes per step rather than
    // how many cells ever changed.
    check(held[1] < held[2] * 0.02f + 1.0f,
          "a settled network over stable ground holds its courses");
}

void testWhatMakesTheGroundJump() {
    std::printf("What a cell that jumps kilometres in one step is doing\n");
    simulation::CrustGrid grid(1000000.0f, 31, 6, 14);

    for (int i = 0; i < 4; i++) {
        grid.step(1.0f);
    }

    const auto& cells = grid.getCells();
    const size_t n = cells.size();

    std::vector<float> elevationBefore(n), thicknessBefore(n), densityBefore(n),
        ageBefore(n);
    std::vector<uint16_t> plateBefore(n);
    for (size_t i = 0; i < n; i++) {
        elevationBefore[i] = cells[i].elevation;
        thicknessBefore[i] = cells[i].thickness;
        densityBefore[i] = cells[i].density;
        ageBefore[i] = cells[i].age;
        plateBefore[i] = cells[i].plateId;
    }

    grid.step(1.0f);

    // Rank the cells by how far the ground moved, then ask what else changed
    // about them. A jump is only an artefact if nothing arrived to justify it.
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; i++) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::fabs(cells[a].elevation - elevationBefore[a]) >
               std::fabs(cells[b].elevation - elevationBefore[b]);
    });

    std::printf("    the eight largest movements this step:\n");
    std::printf("      %8s %10s %10s %8s\n", "d elev", "d thick", "d density", "plate");
    for (int r = 0; r < 8; r++) {
        const size_t i = order[r];
        std::printf("      %8.0f %10.0f %10.1f %8s\n",
                    cells[i].elevation - elevationBefore[i],
                    cells[i].thickness - thicknessBefore[i],
                    cells[i].density - densityBefore[i],
                    cells[i].plateId == plateBefore[i] ? "same" : "changed");
    }

    // Airy isostasy says elevation is thickness times one minus the density
    // ratio, so a change in elevation with no change in the column is the
    // thing that would be wrong. Measured as how much of the movement the
    // column accounts for.
    // The whole formula, not half of it.
    //
    // Airy buoyancy is only one of two terms. Oceanic lithosphere also subsides
    // as the square root of its age, by up to three kilometres between a ridge
    // and eighty million years - and age changes when crust of a different age
    // arrives, which is exactly what happens at a moving boundary. Predicting
    // from thickness and density alone left a quarter of the movements looking
    // unaccounted for, and every one of them was the term I had left out.
    const auto& k = grid.getConstants();
    const auto predict = [&](float thickness, float density, float age) {
        float height = thickness * (1.0f - density / k.mantleDensity);
        const float oceanic = std::min(
            std::max((density - k.continentalDensity) /
                         (k.oceanicDensity - k.continentalDensity), 0.0f), 1.0f);
        const float capped = std::min(age, k.thermalSubsidenceMaxAge);
        height -= oceanic * k.thermalSubsidenceRate * std::sqrt(std::max(0.0f, capped));
        return height;
    };

    int explained = 0;
    int unexplained = 0;
    float worstUnexplained = 0.0f;

    for (size_t i = 0; i < n; i++) {
        const float moved = std::fabs(cells[i].elevation - elevationBefore[i]);
        if (moved < 200.0f) {
            continue;
        }

        const float predictedBefore =
            predict(thicknessBefore[i], densityBefore[i], ageBefore[i]);
        const float predictedAfter =
            predict(cells[i].thickness, cells[i].density, cells[i].age);
        const float fromColumn = std::fabs(predictedAfter - predictedBefore);

        // Elevation is a function of the column alone, so this should match
        // almost exactly; the tolerance is for float and for erosion having
        // moved a little rock in between.
        if (fromColumn > moved * 0.9f - 20.0f) {
            explained++;
        } else {
            unexplained++;
            worstUnexplained = std::max(worstUnexplained, moved);
        }
    }

    std::printf("    of the cells that moved over 200 m: %d explained by the column, "
                "%d not (worst %.0f m)\n", explained, unexplained, worstUnexplained);

    check(explained > 0, "some cells moved because their column changed");
    check(unexplained * 50 < explained + 1,
          "movement is accounted for by the crust arriving, not invented");
}

void testResolutionChoice() {
    std::printf("What a finer grid costs and what it buys\n");

    // Side by side, because "is a finer grid worth it" is not answerable from
    // one measurement. What matters is not the cost per step but the cost per
    // million years simulated - halving the cell spacing halves the stable
    // timestep as well as quadrupling the cells, so the two compound.
    // Several seeds per level, because plate layout is not the same at two
    // cell counts - assignPlates works from cell positions - so a single
    // measurement of plate speed compares two different planets and cannot
    // tell resolution dependence from ordinary variation between them.
    const int seeds[] = {31, 77, 129};

    for (int subdivisions = 6; subdivisions <= 7; subdivisions++) {
      double speedAcross = 0.0;
      double costAcross = 0.0;

      for (int seed : seeds) {
        simulation::CrustGrid grid(1000000.0f, seed, subdivisions, 14);

        const size_t cells = grid.getCells().size();
        const float spacing = grid.cellSpacing();
        const float stable = grid.maxStableTimestep();

        // Warm up so the marker population and plate layout are realistic.
        for (int i = 0; i < 2; i++) {
            grid.step(1.0f);
        }

        const auto before = grid.computeStats();

        const auto began = std::chrono::steady_clock::now();
        constexpr int STEPS = 2;
        for (int i = 0; i < STEPS; i++) {
            grid.step(1.0f);
        }
        const float elapsed =
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - began)
                .count();

        const auto after = grid.computeStats();
        const auto diagnostics = grid.computeDiagnostics();

        float fastest = 0.0f;
        double speedSum = 0.0;
        for (const auto& plate : grid.getPlates()) {
            // Angular velocity to centimetres a year at the surface.
            const float cmPerYear = plate.angularVelocity() * 1000000.0f * 100.0f / 1.0e6f;
            fastest = std::max(fastest, cmPerYear);
            speedSum += cmPerYear;
        }
        const float meanSpeed =
            grid.getPlates().empty() ? 0.0f
                                     : static_cast<float>(speedSum / grid.getPlates().size());

        const float perStep = elapsed / STEPS;
        const float perMy = elapsed / (STEPS * 1.0f);

        speedAcross += meanSpeed;
        costAcross += perMy;

        std::printf("\n  level %d seed %d: %zu cells, %.1f km apart\n", subdivisions, seed,
                    cells, spacing / 1000.0f);
        std::printf("    markers            %zu\n", grid.getMarkers().size());
        std::printf("    stable timestep    %.3f My\n", stable);
        std::printf("    cost               %.1f ms per step, %.0f ms per My simulated\n",
                    perStep, perMy);
        std::printf("    land               %.1f%%\n", after.landFraction * 100.0f);
        std::printf("    elevation range    %.0f to %.0f m\n", after.minElevation,
                    after.maxElevation);
        std::printf("    plate speed        %.1f cm/yr mean, %.1f fastest\n", meanSpeed,
                    fastest);
        std::printf("    plate overlap      %.2f%%\n", diagnostics.overlapFraction * 100.0f);
        std::printf("    largest jump       %.0f m in one step\n",
                    diagnostics.maxElevationJump);

        // Conservation is the thing that must not depend on resolution. The
        // force balance divides by cell area and the transport is bounded by
        // cell spacing, so a finer grid exercises both differently.
        const float drift = before.crustVolume > 0.0f
                                ? std::fabs(after.crustVolume - before.crustVolume) /
                                      before.crustVolume
                                : 0.0f;
        std::printf("    crust volume drift %.4f%% over %d My\n", drift * 100.0f, STEPS);

        check(after.landFraction > 0.05f && after.landFraction < 0.80f,
              "the planet has land and ocean at this resolution");
        check(meanSpeed > 0.5f && meanSpeed < 30.0f,
              "plate speeds stay physical at this resolution");
        check(diagnostics.overlapFraction < 0.05f,
              "plates do not pass through each other at this resolution");
      }

      const double n = static_cast<double>(sizeof(seeds) / sizeof(seeds[0]));
      std::printf("\n  level %d across %d seeds: %.2f cm/yr mean, %.0f ms per My\n\n",
                  subdivisions, static_cast<int>(n), speedAcross / n, costAcross / n);
    }
}

void testWhereTheTimeGoes() {
    std::printf("Where a step spends its time\n");
    simulation::CrustGrid grid(1000000.0f, 23, 6, 14);

    // Warm up, so the marker population and plate layout are realistic rather
    // than the initial condition.
    for (int i = 0; i < 4; i++) {
        grid.step(1.0f);
    }

    // Averaged over several steps: any one of them can catch a plate
    // reorganisation or a climate solve and read as an outlier.
    simulation::CrustGrid::Timings sum{};
    constexpr int SAMPLES = 6;
    for (int i = 0; i < SAMPLES; i++) {
        grid.step(1.0f);
        const auto& t = grid.getTimings();
        sum.plateMotion += t.plateMotion;
        sum.advection += t.advection;
        sum.reconcile += t.reconcile;
        sum.isostasy += t.isostasy;
        sum.climate += t.climate;
        sum.erosion += t.erosion;
        sum.erosionFill += t.erosionFill;
        sum.erosionRoute += t.erosionRoute;
        sum.erosionAccumulate += t.erosionAccumulate;
        sum.erosionIncise += t.erosionIncise;
        sum.erosionCreep += t.erosionCreep;
        sum.erosionApply += t.erosionApply;
        sum.rebalance += t.rebalance;
        sum.gradients += t.gradients;
        sum.total += t.total;
    }

    const float n = static_cast<float>(SAMPLES);
    const float total = sum.total / n;
    const auto share = [&](const char* name, float ms) {
        std::printf("    %-14s %7.2f ms  %5.1f%%\n", name, ms / n,
                    total > 0.0f ? 100.0f * (ms / n) / total : 0.0f);
    };

    std::printf("  %zu cells, %.2f ms per step\n", grid.getCells().size(), total);
    share("plate motion", sum.plateMotion);
    share("advection", sum.advection);
    share("reconcile", sum.reconcile);
    share("isostasy", sum.isostasy);
    share("climate", sum.climate);
    share("erosion", sum.erosion);
    share("  fill", sum.erosionFill);
    share("  route", sum.erosionRoute);
    share("  accumulate", sum.erosionAccumulate);
    share("  incise", sum.erosionIncise);
    share("  creep", sum.erosionCreep);
    share("  apply", sum.erosionApply);
    share("rebalance", sum.rebalance);
    share("gradients", sum.gradients);

    check(total > 0.0f, "a step was measured");
}

void testStepPerformance() {
    std::printf("A step is fast enough to run interactively\n");
    simulation::CrustGrid grid(1000000.0f, 42, 6, 12);
    std::printf("  grid has %zu cells\n", grid.getCells().size());

    const auto start = std::chrono::steady_clock::now();
    grid.step(2.0f);
    const auto elapsed = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    std::printf("  one 2 My step took %.1f ms\n", elapsed);
    check(elapsed < 2000.0f, "a step completes in under two seconds");
}

} // namespace

int main() {
    std::printf("=== CrustGrid tectonics tests ===\n\n");

    { const auto t0 = std::chrono::steady_clock::now();
      testGridTopology();
      std::printf("[time] %-42s %6.0f ms\n", "testGridTopology",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testNearestCellLookup();
      std::printf("[time] %-42s %6.0f ms\n", "testNearestCellLookup",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testSurfaceReconstruction();
      std::printf("[time] %-42s %6.0f ms\n", "testSurfaceReconstruction",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testIsostasyPredictsRealElevations();
      std::printf("[time] %-42s %6.0f ms\n", "testIsostasyPredictsRealElevations",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testSeaLevelRespondsToCrust();
      std::printf("[time] %-42s %6.0f ms\n", "testSeaLevelRespondsToCrust",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testPlatesActuallyMove();
      std::printf("[time] %-42s %6.0f ms\n", "testPlatesActuallyMove",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testSilicateBooksBalance();
      std::printf("[time] %-42s %6.0f ms\n", "testSilicateBooksBalance",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testContinentsPersist();
      std::printf("[time] %-42s %6.0f ms\n", "testContinentsPersist",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testPlateForcesGiveRealisticSpeeds();
      std::printf("[time] %-42s %6.0f ms\n", "testPlateForcesGiveRealisticSpeeds",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testSlabPullDominates();
      std::printf("[time] %-42s %6.0f ms\n", "testSlabPullDominates",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testContinentalPlatesAreSlower();
      std::printf("[time] %-42s %6.0f ms\n", "testContinentalPlatesAreSlower",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testPlateMotionEvolves();
      std::printf("[time] %-42s %6.0f ms\n", "testPlateMotionEvolves",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testPlatesReorganise();
      std::printf("[time] %-42s %6.0f ms\n", "testPlatesReorganise",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testNothingImpossibleHappens();
      std::printf("[time] %-42s %6.0f ms\n", "testNothingImpossibleHappens",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testErosionConservesRock();
      std::printf("[time] %-42s %6.0f ms\n", "testErosionConservesRock",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testErosionMovesRockDownhill();
      std::printf("[time] %-42s %6.0f ms\n", "testErosionMovesRockDownhill",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testErosionLimitsMountains();
      std::printf("[time] %-42s %6.0f ms\n", "testErosionLimitsMountains",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testStratigraphy();
      std::printf("[time] %-42s %6.0f ms\n", "testStratigraphy",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testRigidRotationPreservesContrast();
      std::printf("[time] %-42s %6.0f ms\n", "testRigidRotationPreservesContrast",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testRivers();
      std::printf("[time] %-42s %6.0f ms\n", "testRivers",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testClimate();
      std::printf("[time] %-42s %6.0f ms\n", "testClimate",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testWhereTheTimeGoes();
      std::printf("[time] %-42s %6.0f ms\n", "testWhereTheTimeGoes",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testTimeSlicingDoesNotChangeThePlanet();
      std::printf("[time] %-42s %6.0f ms\n", "testTimeSlicingDoesNotChangeThePlanet",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testRiversComeInSizes();
      std::printf("[time] %-42s %6.0f ms\n", "testRiversComeInSizes",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testChannelsCarveAndPersist();
      std::printf("[time] %-42s %6.0f ms\n", "testChannelsCarveAndPersist",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testAbandonedValleysSurviveTheRiver();
      std::printf("[time] %-42s %6.0f ms\n", "testAbandonedValleysSurviveTheRiver",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testDividesDoNotFlap();
      std::printf("[time] %-42s %6.0f ms\n", "testDividesDoNotFlap",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testRiversSurviveBeingRunFast();
      std::printf("[time] %-42s %6.0f ms\n", "testRiversSurviveBeingRunFast",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testHowMuchRewiringIsWarranted();
      std::printf("[time] %-42s %6.0f ms\n", "testHowMuchRewiringIsWarranted",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testWhetherFinerCellsSteadyTheRivers();
      std::printf("[time] %-42s %6.0f ms\n", "testWhetherFinerCellsSteadyTheRivers",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testDenudationMatchesTheRealWorld();
      std::printf("[time] %-42s %6.0f ms\n", "testDenudationMatchesTheRealWorld",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testContinentsDoNotDrown();
      std::printf("[time] %-42s %6.0f ms\n", "testContinentsDoNotDrown",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testWhatKeepsContinentsAbove();
      std::printf("[time] %-42s %6.0f ms\n", "testWhatKeepsContinentsAbove",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testWhetherTheSheddingIsNoise();
      std::printf("[time] %-42s %6.0f ms\n", "testWhetherTheSheddingIsNoise",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
      testCrustBudgetDoesNotDependOnCellSize();
      std::printf("[time] %-42s %6.0f ms\n", "testCrustBudgetDoesNotDependOnCellSize",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testDrainageReorganises();
      std::printf("[time] %-42s %6.0f ms\n", "testDrainageReorganises",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testWhatMakesTheGroundJump();
      std::printf("[time] %-42s %6.0f ms\n", "testWhatMakesTheGroundJump",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testResolutionChoice();
      std::printf("[time] %-42s %6.0f ms\n", "testResolutionChoice",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
    { const auto t0 = std::chrono::steady_clock::now();
      testStepPerformance();
      std::printf("[time] %-42s %6.0f ms\n", "testStepPerformance",
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }

    std::printf("\n");
    if (failures == 0) {
        std::printf("All CrustGrid tests passed.\n");
        return 0;
    }
    std::printf("%d CrustGrid check(s) failed.\n", failures);
    return 1;
}
