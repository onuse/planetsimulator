// Tests for the plate tectonics simulation.
//
// These check mechanism, not appearance: that isostasy predicts the right
// continent/ocean elevation difference, that crust is conserved, that plates
// actually move, and that the planet evolves rather than sitting still.

#include "simulation/crust_grid.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>

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
    simulation::CrustGrid grid(1000000.0f, 3, 5, 10);

    const auto stats = grid.computeStats();
    std::printf("  land %.1f%%, elevation range [%.0f, %.0f] m\n",
                stats.landFraction * 100.0f, stats.minElevation, stats.maxElevation);

    check(stats.landFraction > 0.02f && stats.landFraction < 0.95f,
          "planet has both land and ocean");
    check(stats.maxElevation > stats.minElevation, "elevation varies across the planet");

    // Displacing water with more crust must raise sea level relative to the
    // datum. Thicken everything and check the solver responds.
    simulation::CrustGrid thick(1000000.0f, 3, 5, 10);
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
    check(grid.getVersion() == 25, "version tracks steps taken");
}

// Every gram of crust must be accounted for. Crust is created from mantle melt
// at ridges and arcs, and returned to the mantle by subduction; nothing else
// may appear or disappear. This is the check that catches a process quietly
// leaking material - which is exactly how continents were vanishing.
void testSilicateBooksBalance() {
    std::printf("Crust and mantle books balance exactly\n");
    simulation::CrustGrid grid(1000000.0f, 5, 5, 12);

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
    simulation::CrustGrid grid(1000000.0f, 5, 5, 12);

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
    simulation::CrustGrid grid(1000000.0f, 42, 5, 1);

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

    const float omega = grid.getPlates()[0].angularVelocity;
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
    testIsostasyPredictsRealElevations();
    testSeaLevelRespondsToCrust();
    testPlatesActuallyMove();
    testSilicateBooksBalance();
    testContinentsPersist();
    testStratigraphy();
    testRigidRotationPreservesContrast();
    testStepPerformance();

    std::printf("\n");
    if (failures == 0) {
        std::printf("All CrustGrid tests passed.\n");
        return 0;
    }
    std::printf("%d CrustGrid check(s) failed.\n", failures);
    return 1;
}
