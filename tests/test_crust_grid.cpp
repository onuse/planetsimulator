// Tests for the plate tectonics simulation.
//
// These check mechanism, not appearance: that isostasy predicts the right
// continent/ocean elevation difference, that crust is conserved, that plates
// actually move, and that the planet evolves rather than sitting still.

#include "simulation/crust_grid.hpp"

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

    testGridTopology();
    testNearestCellLookup();
    testSurfaceReconstruction();
    testIsostasyPredictsRealElevations();
    testSeaLevelRespondsToCrust();
    testPlatesActuallyMove();
    testSilicateBooksBalance();
    testContinentsPersist();
    testPlateForcesGiveRealisticSpeeds();
    testSlabPullDominates();
    testContinentalPlatesAreSlower();
    testPlateMotionEvolves();
    testPlatesReorganise();
    testNothingImpossibleHappens();
    testErosionConservesRock();
    testErosionMovesRockDownhill();
    testErosionLimitsMountains();
    testStratigraphy();
    testRigidRotationPreservesContrast();
    testRivers();
    testClimate();
    testWhereTheTimeGoes();
    testStepPerformance();

    std::printf("\n");
    if (failures == 0) {
        std::printf("All CrustGrid tests passed.\n");
        return 0;
    }
    std::printf("%d CrustGrid check(s) failed.\n", failures);
    return 1;
}
