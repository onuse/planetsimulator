#include "simulation/crust_grid.hpp"
#include "utils/parallel.hpp"

#include <chrono>
#include <mutex>

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

void CrustGrid::erodeSurface(float dt, bool networkOnly) {
    if (dt <= 0.0f || cells.empty() || cellMarkers.size() != cells.size()) {
        return;
    }

    const Constants& k = constants;
    const int n = static_cast<int>(cells.size());
    const float cellArea = getCellArea();
    const float spacing = cellSpacing();

    using Clock = std::chrono::steady_clock;
    auto mark = Clock::now();
    const auto lap = [&mark]() {
        const auto now = Clock::now();
        const float ms = std::chrono::duration<float, std::milli>(now - mark).count();
        mark = now;
        return ms;
    };

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

    // Where each cell spills to, and the order the flood reached them.
    //
    // The flood already knows both, and throwing them away was what left more
    // than half the land draining nowhere. Priority flood raises a basin to its
    // spill point, which makes the basin flat - and steepest descent across a
    // flat finds no lower neighbour and gives up, so every flow path died at
    // the first lake it met. The longest river on the planet was seven cells.
    //
    // The cell that first reached j during the flood *is* j's way out: the
    // flood works inward from the sea in increasing order of filled level, so
    // that cell is downhill-or-level and nearer the outlet by construction.
    // Following it cannot loop, because every cell's spill target was popped
    // before the cell itself.
    std::vector<int> spillTo(n, -1);
    std::vector<int> popOrder;
    popOrder.reserve(n);

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
        popOrder.push_back(cell);
        for (int m = 0; m < neighbourCount(cell); m++) {
            const int j = neighbourAt(cell, m);
            if (visited[j]) {
                continue;
            }
            visited[j] = 1;
            filled[j] = std::max(surface[j], level);
            spillTo[j] = cell;
            queue.emplace(filled[j], j);
        }
    }

    timings.erosionFill = lap();

    // ------------------------------------------------------------------
    // 2. Route downhill
    // ------------------------------------------------------------------
    std::vector<int> receiver(n, -1);
    std::vector<float> slope(n, 0.0f);

    // Steepest descent, with the channel the river is already in given the
    // advantage its own depth earns it.
    //
    // The incumbent receiver is remembered from the last time this ran, and a
    // rival has to be lower by more than the channel is deep to take the flow.
    // Depth comes from the discharge that cut it, so a headwater stream is
    // nearly free to wander and a trunk river is not. Without this the routing
    // has no memory at all and a millimetre of difference moves a whole river:
    // ninety-two per cent of the network changed receiver inside one million
    // year, which is churn rather than reorganisation.
    const bool haveHistory = lastFlowsInto.size() == static_cast<size_t>(n) &&
                             lastDischarge.size() == static_cast<size_t>(n);

    for (int i = 0; i < n; i++) {
        if (filled[i] <= 0.0f) {
            continue;   // already at or below sea level
        }

        // What it would cost to abandon the existing channel.
        float entrenchment = 0.0f;
        int incumbent = -1;
        if (haveHistory) {
            incumbent = lastFlowsInto[i];
            if (incumbent >= 0 && incumbent < n) {
                const float catchments = lastDischarge[i];
                if (catchments > 1.0f) {
                    entrenchment = k.channelDepthPerCatchment *
                                   std::pow(catchments, k.channelDepthExponent);
                }
            } else {
                incumbent = -1;
            }
        }

        int best = -1;
        float steepest = 0.0f;

        // The incumbent first, so it holds ties as well as near-ties.
        if (incumbent >= 0) {
            const float drop = filled[i] - filled[incumbent];
            if (drop > 0.0f) {
                steepest = drop + entrenchment;
                best = incumbent;
            }
        }

        for (int m = 0; m < neighbourCount(i); m++) {
            const int j = neighbourAt(i, m);
            if (j == incumbent) {
                continue;
            }
            const float drop = filled[i] - filled[j];
            if (drop > steepest) {
                steepest = drop;
                best = j;
            }
        }

        // Nothing lower anywhere: this cell is inside a filled basin, and the
        // flood already worked out the way out of it.
        if (best < 0 && spillTo[i] >= 0) {
            best = spillTo[i];
        }

        receiver[i] = best;

        // The slope that drives incision is the real one, not the one the
        // entrenchment bonus produced - that bonus decides where the water
        // goes, and inventing gradient from it would make rivers cut faster
        // for having stayed put.
        const float trueDrop = best >= 0 ? std::max(0.0f, filled[i] - filled[best]) : 0.0f;
        slope[i] = best >= 0 ? trueDrop / spacing : 0.0f;
    }

    timings.erosionRoute = lap();

    // ------------------------------------------------------------------
    // 3. Accumulate drainage from the top down
    // ------------------------------------------------------------------
    // Highest first, so a cell has collected everything above it before it
    // passes the total on. Reverse pop order gives exactly that and costs
    // nothing: the flood pops in increasing filled level, and every receiver -
    // whether chosen by descent or by the spill path - was popped before the
    // cell that drains into it. Sorting by elevation instead was subtly wrong
    // as well as slower, because it cannot order cells inside a flat.
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
    for (auto it = popOrder.rbegin(); it != popOrder.rend(); ++it) {
        const int i = *it;
        if (receiver[i] >= 0) {
            drainage[receiver[i]] += drainage[i];
        }
    }

    // ------------------------------------------------------------------
    // Keep the network. This is the only place it exists, and it is a river
    // system: where the water collects, and which way it goes.
    lastDischarge.resize(n);
    lastFlowsInto.resize(n);
    lastLakeDepth.resize(n);
    lastRoutedSurface.resize(n);
    // Upstream cells, not cubic metres.
    //
    // Accumulation works in volume because stream power needs volume, but every
    // reader of this field wants a catchment size - how many cells' worth of
    // rain arrives here - and each of them was dividing by the square of the
    // cell spacing to get one. That is not the cell's area (the cells are
    // hexagons) and the spacing does not track the grid resolution, so the
    // count came out several times too small and every threshold built on it
    // was wrong by the same factor. Dividing here, once, by the volume a single
    // average cell contributes, makes the field mean what its name says.
    const double perCell =
        std::max(1e-9, static_cast<double>(cellArea) * static_cast<double>(k.precipitation));

    for (int i = 0; i < n; i++) {
        lastDischarge[i] = static_cast<float>(drainage[i] / perCell);
        lastFlowsInto[i] = receiver[i];
        // What the depression fill added: zero on a slope, positive in a lake.
        lastLakeDepth[i] = std::max(0.0f, filled[i] - surface[i]);
        lastRoutedSurface[i] = filled[i];
    }

    if (networkOnly) {
        return;
    }

    // 4. Incise, carry, deposit
    // ------------------------------------------------------------------
    std::vector<double> load(n, 0.0);      // sediment in transit, m^3
    std::vector<double> change(n, 0.0);    // metres of column gained or lost

    double eroded = 0.0;
    double deposited = 0.0;

    // Highest first again, so debris is picked up before the cell it lands in
    // is visited and can carry it further down the same pass.
    for (auto it = popOrder.rbegin(); it != popOrder.rend(); ++it) {
        const int i = *it;
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

    timings.erosionIncise = lap();

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

    // The grid decided how much moves; the parcels are what actually moves.
    // Shared by both erosion models, because conservation is the part that must
    // not depend on which one of them ran.
    timings.erosionCreep = lap();

    applyErosionChange(change, eroded, deposited);
    timings.erosionApply = lap();
}

void CrustGrid::erodeBulk(float dt) {
    if (dt <= 0.0f || cells.empty() || cellMarkers.size() != cells.size()) {
        return;
    }

    const Constants& k = constants;
    const int n = static_cast<int>(cells.size());
    const float cellArea = getCellArea();

    // Denudation without routing.
    //
    // Over a million years what matters is that high ground wears down and the
    // material ends up in basins. Which channel carried it is not a question
    // this timestep can answer, and the channel will have moved several times
    // before the step is over.
    //
    // The rate follows local relief rather than absolute height, because what
    // drives erosion is the gradient available - a high plateau with nothing
    // around it lower erodes slowly, and that is why plateaus exist.
    std::vector<double> change(n, 0.0);
    double removed = 0.0;

    for (int i = 0; i < n; i++) {
        const float here = cells[i].elevation - seaLevel;
        if (here <= 0.0f) {
            continue;   // already in the sea
        }

        float lowest = here;
        for (int m = 0; m < neighbourCount(i); m++) {
            lowest = std::min(lowest, cells[neighbourAt(i, m)].elevation - seaLevel);
        }
        const float relief = std::max(0.0f, here - lowest);
        if (relief <= 0.0f) {
            continue;
        }

        // The same stream power law, with the discharge estimated instead of
        // routed.
        //
        // This is not a separate model with its own rate - that was the first
        // attempt and it was wrong twice over. It ignored the coefficient that
        // controls erosion, so switching erosion off left the mountains
        // eroding anyway; and its conversion factor was guessed, which put it
        // four orders of magnitude out and flattened the planet to under six
        // hundred metres in a few hundred million years.
        //
        // Written as the physics it is approximating, the only thing left to
        // assume is how much water passes through a cell. Routing answers that
        // exactly; without it, a few cells' worth is the honest estimate, and
        // it is bounded - most cells really do drain only themselves and their
        // immediate neighbours, and the few trunks that carry far more are
        // precisely the detail this regime cannot resolve.
        constexpr double ASSUMED_CATCHMENT = 4.0;
        const double discharge = static_cast<double>(cellArea) * k.precipitation *
                                 climate.relativePrecipitation(i) * ASSUMED_CATCHMENT;
        const double slope = static_cast<double>(relief) / std::max(cellSpacing(), 1.0f);

        const double rate = k.streamPowerCoefficient *
                            std::pow(discharge, k.drainageExponent) *
                            std::pow(slope, k.slopeExponent) * dt;

        // Never below the lowest neighbour, or the surface turns inside out.
        const double lowering = std::min(rate, static_cast<double>(relief) * 0.5);
        change[i] -= lowering;
        removed += lowering * cellArea;
    }

    // Everything taken off the land goes to the sea floor, weighted by depth -
    // the deeper the basin, the more room it has and the more of the load it
    // takes.
    double weight = 0.0;
    for (int i = 0; i < n; i++) {
        const float depth = seaLevel - cells[i].elevation;
        if (depth > 0.0f) {
            weight += depth;
        }
    }

    if (weight > 0.0 && removed > 0.0) {
        for (int i = 0; i < n; i++) {
            const float depth = seaLevel - cells[i].elevation;
            if (depth > 0.0f) {
                change[i] += (removed * (depth / weight)) / cellArea;
            }
        }
    }

    applyErosionChange(change, removed, removed);
}

void CrustGrid::applyErosionChange(const std::vector<double>& change,
                                   double eroded, double deposited) {
    // Nothing to move rock into or out of until the parcels have been indexed
    // against the cells.
    if (cellMarkers.size() != cells.size() || change.size() != cells.size()) {
        return;
    }

    const Constants& k = constants;
    const int n = static_cast<int>(cells.size());
    const float cellArea = getCellArea();

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

    // Parallel over cells, which is safe for a reason worth stating: every
    // parcel belongs to exactly one cell, because cellMarkers is built by
    // giving each parcel to its nearest one. So two threads working on two
    // cells can never reach the same parcel, and the rock can be moved without
    // any coordination at all.
    //
    // Only the running totals are shared, and they are summed once per range
    // rather than once per parcel - a quarter of a million lock acquisitions
    // would cost more than the work they guard, where thirty cost nothing.
    std::mutex totalsMutex;

    util::parallelFor(static_cast<size_t>(n), [&](size_t begin, size_t end) {
        double localErosion = 0.0;
        double localPlanned = 0.0;

        for (size_t i = begin; i < end; i++) {
            if (cellMarkers[i].empty()) {
                continue;
            }
            const double volume = change[i] * cellArea;
            if (volume >= 0.0) {
                localPlanned += volume;
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
                localErosion += marker.erodeFromTop(std::min(share, allowed));
            }
        }

        std::lock_guard<std::mutex> lock(totalsMutex);
        actualErosion += localErosion;
        plannedDeposition += localPlanned;
    });

    // Lay down exactly what was picked up, no more.
    const double scale = plannedDeposition > 0.0 ? actualErosion / plannedDeposition : 0.0;
    double actualDeposition = 0.0;

    if (scale > 0.0) {
        std::mutex depositMutex;

        util::parallelFor(static_cast<size_t>(n), [&](size_t begin, size_t end) {
            double local = 0.0;

            for (size_t i = begin; i < end; i++) {
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
                    local += share;
                }
            }

            std::lock_guard<std::mutex> lock(depositMutex);
            actualDeposition += local;
        });
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
            const float here = cells[i].elevation - seaLevel;
            if (!cellMarkers[i].empty() && here < lowest) {
                lowest = here;
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
