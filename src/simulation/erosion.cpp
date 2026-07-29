#include "simulation/crust_grid.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

// Erosion: rivers wearing the land down and putting it somewhere else.
//
// The order matters and is the standard one for landscape evolution models:
//
//   1. fill depressions, so water has somewhere to go
//   2. route each cell downhill to a single receiver
//   3. accumulate drainage area from the top of the landscape down
//   4. incise by stream power, and carry the debris downstream until the
//      river runs out of the energy to hold it
//
// Nothing is destroyed. Every cubic metre cut out of a hillside is tracked
// until it is laid down again somewhere lower, and the two totals are asserted
// against each other in the tests. That discipline is why this is worth
// writing carefully rather than approximately: an erosion model that leaks is
// indistinguishable from one that works, right up until continents evaporate.

namespace simulation {

void CrustGrid::erodeSurface(float dt) {
    if (dt <= 0.0f || cells.empty()) {
        return;
    }

    const Constants& k = constants;
    const int n = static_cast<int>(cells.size());
    const float cellArea = getCellArea();
    const float spacing = cellSpacing();

    // Elevation relative to sea level. Below zero is underwater, and rivers do
    // not run there.
    std::vector<float> surface(n);
    for (int i = 0; i < n; i++) {
        surface[i] = cells[i].elevation - seaLevel;
    }

    // ------------------------------------------------------------------
    // 1. Fill depressions
    // ------------------------------------------------------------------
    //
    // A pit with no outlet stops the flow routing dead, and a geodesic surface
    // built from noise and tectonics has plenty of them. Priority flood raises
    // each basin to the height of its lowest spill point, working inward from
    // the sea - so a filled basin is a lake, which is where sediment should
    // settle anyway.
    std::vector<float> filled(surface);
    std::vector<char> visited(n, 0);

    using Entry = std::pair<float, int>;   // (level, cell)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;

    for (int i = 0; i < n; i++) {
        if (surface[i] <= 0.0f) {
            visited[i] = 1;
            queue.emplace(surface[i], i);
        }
    }
    if (queue.empty()) {
        // No ocean at all: start from the single lowest point so there is
        // still an outlet.
        const int lowest = static_cast<int>(
            std::min_element(surface.begin(), surface.end()) - surface.begin());
        visited[lowest] = 1;
        queue.emplace(surface[lowest], lowest);
    }

    while (!queue.empty()) {
        const auto [level, cell] = queue.top();
        queue.pop();
        for (int m = 0; m < neighbourCount(cell); m++) {
            const int j = neighbourAt(cell, m);
            if (visited[j]) {
                continue;
            }
            visited[j] = 1;
            filled[j] = std::max(surface[j], level);
            queue.emplace(filled[j], j);
        }
    }

    // ------------------------------------------------------------------
    // 2. Route downhill
    // ------------------------------------------------------------------
    std::vector<int> receiver(n, -1);
    std::vector<float> slope(n, 0.0f);

    for (int i = 0; i < n; i++) {
        if (filled[i] <= 0.0f) {
            continue;   // already at or below sea level
        }
        int best = -1;
        float steepest = 0.0f;
        for (int m = 0; m < neighbourCount(i); m++) {
            const int j = neighbourAt(i, m);
            const float drop = filled[i] - filled[j];
            if (drop > steepest) {
                steepest = drop;
                best = j;
            }
        }
        receiver[i] = best;
        slope[i] = best >= 0 ? steepest / spacing : 0.0f;
    }

    // ------------------------------------------------------------------
    // 3. Accumulate drainage from the top down
    // ------------------------------------------------------------------
    std::vector<int> order(n);
    for (int i = 0; i < n; i++) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return filled[a] > filled[b]; });

    std::vector<double> drainage(n, 0.0);
    for (int i = 0; i < n; i++) {
        // Every cell contributes its own catch of rain - and how much rain
        // that is now varies across the planet. A range in the westerlies
        // gathers a river system on its windward flank and almost nothing on
        // its lee, so the two faces wear down at different rates and the
        // divide migrates. With one number everywhere both sides eroded
        // identically, which is the one thing real mountains never do.
        drainage[i] = static_cast<double>(cellArea) * k.precipitation *
                      climate.relativePrecipitation(i);
    }
    for (int i : order) {
        if (receiver[i] >= 0) {
            drainage[receiver[i]] += drainage[i];
        }
    }

    // ------------------------------------------------------------------
    // 4. Incise, carry, deposit
    // ------------------------------------------------------------------
    std::vector<double> load(n, 0.0);      // sediment in transit, m^3
    std::vector<double> change(n, 0.0);    // metres of column gained or lost

    double eroded = 0.0;
    double deposited = 0.0;

    for (int i : order) {
        const int down = receiver[i];

        if (down < 0 || filled[i] <= 0.0f) {
            // The sea, or a closed basin with nowhere left to go. Everything
            // being carried settles here - this is what builds deltas, shelves
            // and the floors of lakes.
            if (load[i] > 0.0) {
                change[i] += load[i] / cellArea;
                deposited += load[i];
                load[i] = 0.0;
            }
            continue;
        }

        // Erodibility follows what is actually exposed at the surface. A hard
        // basement under a soft cover slows down once the cover is gone.
        float erodibility = 1.0f;
        if (!cellMarkers[i].empty()) {
            double weight = 0.0;
            double sum = 0.0;
            for (int index : cellMarkers[i]) {
                const Marker& marker = markers[index];
                if (marker.layerCount == 0) continue;
                const Layer& top = marker.layers[marker.layerCount - 1];
                sum += rockErodibility(top.rock) * marker.volume;
                weight += marker.volume;
            }
            if (weight > 0.0) {
                erodibility = static_cast<float>(sum / weight);
            }
        }

        // Stream power. More water and steeper ground cut faster.
        const double incision = k.streamPowerCoefficient * erodibility *
                                std::pow(drainage[i], k.drainageExponent) *
                                std::pow(static_cast<double>(slope[i]), k.slopeExponent) * dt;

        // Never cut below the cell being drained into, or the landscape turns
        // inside out and the flow routing stops meaning anything.
        const double headroom = std::max(0.0, static_cast<double>(filled[i] - filled[down]));
        const double lowering = std::min(incision, headroom * 0.5);

        double cut = lowering * cellArea;
        if (cut > 0.0) {
            change[i] -= lowering;
            load[i] += cut;
            eroded += cut;
        }

        // How much the river can still hold. Below that, the rest drops out.
        const double capacity = k.transportCapacity * k.streamPowerCoefficient *
                                std::pow(drainage[i], k.drainageExponent) *
                                std::pow(static_cast<double>(slope[i]), k.slopeExponent) *
                                dt * cellArea;

        if (load[i] > capacity) {
            const double drop = load[i] - capacity;
            change[i] += drop / cellArea;
            deposited += drop;
            load[i] -= drop;
        }

        load[down] += load[i];
        load[i] = 0.0;
    }

    // Anything still in transit at the end has nowhere left to go.
    for (int i = 0; i < n; i++) {
        if (load[i] > 0.0) {
            change[i] += load[i] / cellArea;
            deposited += load[i];
            load[i] = 0.0;
        }
    }

    // ------------------------------------------------------------------
    // 5. Hillslope creep
    // ------------------------------------------------------------------
    //
    // Symmetric flux per edge, so it conserves by construction. At this cell
    // size its effect is tiny; it is here because it is real, not because it
    // shows.
    const double creep = static_cast<double>(k.hillslopeDiffusivity) * dt /
                         (static_cast<double>(spacing) * spacing);
    if (creep > 0.0) {
        std::vector<double> creepChange(n, 0.0);
        for (int i = 0; i < n; i++) {
            for (int m = 0; m < neighbourCount(i); m++) {
                const int j = neighbourAt(i, m);
                if (j <= i) continue;   // each edge once
                const double flux = creep * (surface[i] - surface[j]) * 0.5;
                creepChange[i] -= flux;
                creepChange[j] += flux;
            }
        }
        for (int i = 0; i < n; i++) {
            change[i] += creepChange[i];
        }
    }

    // ------------------------------------------------------------------
    // 6. Apply it to the rock
    // ------------------------------------------------------------------
    //
    // The grid decided how much moves; the parcels are what actually moves.
    // Erosion strips the youngest rock off the top; deposition lays down a new
    // sediment layer. Both are recorded, so a basin keeps the order in which
    // it filled.
    // Erode first and measure what actually came off. A column can refuse to
    // give up as much as the grid asked for - it may already be down to the
    // minimum crust can be - and the deposition downstream has to be scaled to
    // match what was really removed. Committing the planned deposition instead
    // would quietly manufacture rock, which is exactly the failure this whole
    // model is built to make impossible.
    double actualErosion = 0.0;
    double plannedDeposition = 0.0;

    for (int i = 0; i < n; i++) {
        if (cellMarkers[i].empty()) {
            continue;
        }
        const double volume = change[i] * cellArea;
        if (volume >= 0.0) {
            plannedDeposition += volume;
            continue;
        }

        double total = 0.0;
        for (int index : cellMarkers[i]) {
            total += markers[index].volume;
        }
        if (total <= 0.0) {
            continue;
        }

        // Never strip a column below what crust can be.
        const double floorVolume = static_cast<double>(k.minCrustThickness) * cellArea /
                                   static_cast<double>(cellMarkers[i].size());

        for (int index : cellMarkers[i]) {
            Marker& marker = markers[index];
            const double share = -volume * (marker.volume / total);
            const double allowed = std::max(0.0, marker.volume - floorVolume);
            actualErosion += marker.erodeFromTop(std::min(share, allowed));
        }
    }

    // Lay down exactly what was picked up, no more.
    const double scale = plannedDeposition > 0.0 ? actualErosion / plannedDeposition : 0.0;
    double actualDeposition = 0.0;

    if (scale > 0.0) {
        for (int i = 0; i < n; i++) {
            if (cellMarkers[i].empty() || change[i] <= 0.0) {
                continue;
            }
            const double volume = change[i] * cellArea * scale;
            double total = 0.0;
            for (int index : cellMarkers[i]) {
                total += markers[index].volume;
            }
            if (total <= 0.0) {
                continue;
            }
            for (int index : cellMarkers[i]) {
                Marker& marker = markers[index];
                const double share = volume * (marker.volume / total);
                // Fresh sediment, so its clock starts now.
                marker.deposit(RockType::Sediment, share, 0.0f);
                actualDeposition += share;
            }
        }
    }

    // Whatever the deposition pass could not place - a cell that turned out to
    // hold no parcels, or none with any volume left - still has to go
    // somewhere, because it has already been cut out of a hillside. Put it in
    // the deepest basin that can take it, which is where it would end up
    // anyway. Without this the shortfall is simply lost, and the silicate
    // budget drifts by a few parts in 100,000 per run.
    double unplaced = actualErosion - actualDeposition;
    if (unplaced > 1.0) {
        int sink = -1;
        float lowest = std::numeric_limits<float>::max();
        for (int i = 0; i < n; i++) {
            if (!cellMarkers[i].empty() && surface[i] < lowest) {
                lowest = surface[i];
                sink = i;
            }
        }
        if (sink >= 0) {
            double total = 0.0;
            for (int index : cellMarkers[sink]) {
                total += markers[index].volume;
            }
            if (total > 0.0) {
                for (int index : cellMarkers[sink]) {
                    Marker& marker = markers[index];
                    marker.deposit(RockType::Sediment,
                                   unplaced * (marker.volume / total), 0.0f);
                }
                actualDeposition += unplaced;
                unplaced = 0.0;
            }
        }
    }

    erodedVolume += actualErosion;
    depositedVolume += actualDeposition;

    (void)eroded;
    (void)deposited;
}

} // namespace simulation
