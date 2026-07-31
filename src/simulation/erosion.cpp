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

    // When each cell was reached, as a position in the flood. Any receiver with
    // a smaller number was settled earlier, which is what makes following
    // receivers downstream guaranteed to terminate.
    std::vector<int> popIndex(n, -1);

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
        popIndex[cell] = static_cast<int>(popOrder.size());
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
    const bool haveChannels = channelDepth.size() == static_cast<size_t>(n);

    for (int i = 0; i < n; i++) {
        if (filled[i] <= 0.0f) {
            continue;   // already at or below sea level
        }

        // What it would cost to abandon the existing channel: exactly how deep
        // that channel is.
        //
        // This used to be inferred from discharge through two fitted constants,
        // on the reasoning that a channel is as deep as the water that cut it.
        // The simulation now tracks the depth itself, so the inference is not
        // needed and was never as good - a channel cut into hard rock and one
        // cut into an alluvial fan carry the same water and are not the same
        // depth, and only the tracked value knows the difference.
        float entrenchment = 0.0f;
        int incumbent = -1;
        if (haveHistory) {
            incumbent = lastFlowsInto[i];
            if (incumbent >= 0 && incumbent < n) {
                entrenchment = haveChannels ? channelDepth[i] : 0.0f;
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

        // Nothing lower anywhere: this cell is inside a filled basin.
        //
        // A lake surface is flat by construction - the fill puts every cell in
        // the basin at exactly the spill height - so there is no lower
        // neighbour and nothing in the topography to choose between them. The
        // flood's own path out is a correct answer, but it is not a stable one:
        // it comes from the order a priority queue happened to pop cells whose
        // keys are all equal, and that order is re-derived from scratch every
        // step. Half of every drainage change on the planet was this, a tie
        // broken differently each time on ground that had not moved.
        //
        // Where the surface says nothing, the previous answer stands. Water
        // crossing a lake keeps the course it had until the lake itself
        // changes, which is both the stable choice and the physical one - a
        // current across still water does not rearrange itself for no reason.
        //
        // Only if that course still leads outward. The flood settles cells in
        // order and a receiver settled earlier is nearer the outlet, so
        // requiring the old receiver to have been reached before this cell is
        // exactly the condition that stops a remembered path from closing into
        // a ring.
        if (best < 0) {
            const int previous = haveHistory ? lastFlowsInto[i] : -1;
            const bool stillLeadsOut =
                previous >= 0 && previous < n && popIndex[previous] >= 0 &&
                popIndex[i] >= 0 && popIndex[previous] < popIndex[i] &&
                filled[previous] <= filled[i];

            if (stillLeadsOut) {
                bool adjacent = false;
                for (int m = 0; m < neighbourCount(i) && !adjacent; m++) {
                    adjacent = neighbourAt(i, m) == previous;
                }
                if (adjacent) {
                    best = previous;
                }
            }
            if (best < 0 && spillTo[i] >= 0) {
                best = spillTo[i];
            }
        }

        // Audit only - this changes nothing about the routing.
        if (best >= 0) {
            drainageAudit.routed++;
            const int was = haveHistory ? lastFlowsInto[i] : -1;
            if (was >= 0 && was < n && was != best) {
                drainageAudit.changed++;
                const float depth =
                    channelDepth.size() == static_cast<size_t>(n) ? channelDepth[i] : 0.0f;
                if (filled[was] - filled[best] > depth) {
                    drainageAudit.warranted++;
                }
                drainageAudit.abandonedDepth += depth;
            }
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

    // The fluvial part of that, on its own. Only what rivers do cuts channels -
    // hillslope creep works on the whole cell and fills them in.
    std::vector<double> fluvial(n, 0.0);

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
                fluvial[i] += load[i] / cellArea;
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
            fluvial[i] -= lowering;
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
            fluvial[i] += drop / cellArea;
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
            fluvial[i] += load[i] / cellArea;
            deposited += load[i];
            load[i] = 0.0;
        }
    }

    // ------------------------------------------------------------------
    // The channel, which is below the grid
    // ------------------------------------------------------------------
    evolveChannels(fluvial, dt);

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

    // A channel cannot be cut below the one it flows into, or the water would
    // have to climb out of its own bed to leave the cell.
    //
    // Applied after the elevations have moved, not before. Clamping against the
    // surface the routing used leaves the limit referring to ground that no
    // longer exists by the time anything reads it, which showed up as a handful
    // of channels sitting below their own outlet every step. Forward flood
    // order visits every receiver before the cell draining into it, so each
    // limit is final when it is used.
    // Only where water is actually running.
    //
    // This is a constraint on flow, not on landforms: it says water cannot
    // climb out of its own bed to leave the cell. A dry valley is under no such
    // obligation - it is a hole in the ground, and holes in the ground are
    // allowed to be deeper than their neighbours.
    //
    // Applying it everywhere destroyed exactly what it was supposed to be
    // protecting. An abandoned valley's neighbours are usually abandoned too,
    // so their depth is zero, and the limit collapsed the valley to the bare
    // elevation drop between two cells - metres, on gentle ground. Four out of
    // five valleys that lost their river were erased this way even after the
    // depth itself stopped being zeroed.
    if (channelDepth.size() == static_cast<size_t>(n) &&
        lastDischarge.size() == static_cast<size_t>(n)) {
        for (int i : popOrder) {
            const int down = receiver[i];
            if (down < 0 || lastDischarge[i] < constants.channelThreshold) {
                continue;
            }
            const float drop = std::max(0.0f, cells[i].elevation - cells[down].elevation);
            channelDepth[i] = std::min(channelDepth[i], drop + channelDepth[down]);
        }
    }

    timings.erosionApply = lap();
}

void CrustGrid::evolveChannels(const std::vector<double>& fluvial, float dt) {
    const int n = static_cast<int>(cells.size());
    if (dt <= 0.0f || static_cast<int>(fluvial.size()) != n ||
        static_cast<int>(lastDischarge.size()) != n) {
        return;
    }
    channelDepth.resize(n, 0.0f);

    const Constants& k = constants;
    const float spacing = cellSpacing();
    if (spacing <= 0.0f) {
        return;
    }

    for (int i = 0; i < n; i++) {
        const float catchments = lastDischarge[i];

        // Any water at all cuts something, and what it cuts is remembered. The
        // threshold decides where a channel is worth drawing, which is a
        // question about pixels; using it here made it a question about
        // physics, and left every headwater with no memory of its own course.
        const bool flowing = catchments > 0.0f && lastFlowsInto[i] >= 0;

        // Nothing here, and nothing to remember.
        if (!flowing && channelDepth[i] <= 0.0f) {
            continue;
        }

        // The channel is as deep as the river has cut and the hillsides have
        // not yet filled in. That is the whole model, and it needs no factor.
        //
        // The first attempt multiplied the incision up by the ratio of the cell
        // width to the channel width, reasoning that the material comes out of
        // a narrow strip rather than off the whole cell. That double-counts:
        // stream power already *is* the rate a channel cuts down, which is why
        // landscape models take the grid elevation to be the valley floor. The
        // factor was fifty for a headwater and seventeen for a trunk, so it
        // also had small streams cutting three times deeper than big rivers,
        // and produced gorges seven kilometres deep.
        //
        // Only where there is water to cut with. A dry valley keeps whatever
        // was cut into it and goes on to the filling in below.
        if (flowing) {
            channelDepth[i] -= static_cast<float>(fluvial[i]);
        }

        // And it fills back in, because the valley sides creep into it.
        //
        // The timescale is not a number to pick - it is diffusion across the
        // width of the valley, which the diffusivity and the width already
        // give. A gully closes in a geological instant and a major valley
        // outlasts the river that cut it, from one rule.
        //
        // This runs whether or not there is still a river in it, and that is
        // the point. Setting the depth to zero the moment the discharge fell
        // below the threshold - which is what this did - meant a river that had
        // spent two million years cutting a valley took the valley with it the
        // instant the network reorganised. Nothing was left on the continent to
        // show it had ever been there, which is precisely backwards: the dry
        // valley is the longest-lived thing a river makes, and the only
        // evidence that a capture ever happened.
        // Wide enough for walls that deep to stand up, plus the channel itself
        // where there still is one. The same relation the valley is drawn with,
        // so what fills in is the shape that was there.
        // A valley does not get narrower when the river leaves it.
        //
        // Taking the width as zero once the water had gone made the valley as
        // narrow as its own depth allowed, and the infill time goes as the
        // square of the width - so a ten metre valley was treated as thirty
        // metres across and filled in within five thousand years. Everything
        // shallow disappeared at once. The floor is the width the smallest
        // channel worth calling a river cuts, because that is the narrowest
        // thing that can have been there.
        const float cutBy = std::max(catchments, k.channelThreshold);
        const float flowWidth = std::max(channelWidthFor(cutBy), 1.0f);
        const float valleyWidth = std::max(
            flowWidth + 2.0f * channelDepth[i] / VALLEY_WALL_SLOPE, 1.0f);
        const double infillTime =
            static_cast<double>(valleyWidth) * valleyWidth /
            std::max(4.0 * static_cast<double>(k.hillslopeDiffusivity), 1e-6);
        channelDepth[i] *= static_cast<float>(std::exp(-static_cast<double>(dt) / infillTime));

        // Base level. A river cutting down towards the sea stops when it gets
        // there - it has no energy left to cut with - so the channel floor
        // cannot pass below sea level, and the depth cannot exceed the height
        // of the ground it is cut into. This is what keeps the depth in
        // proportion to the landscape instead of running away with the
        // integration, and it is the reason a lowland river runs in a shallow
        // valley while the same discharge in a highland cuts a gorge.
        const float aboveBaseLevel = cells[i].elevation - seaLevel;
        channelDepth[i] = std::min(channelDepth[i], std::max(0.0f, aboveBaseLevel));

        if (!(channelDepth[i] > 0.0f)) {
            channelDepth[i] = 0.0f;   // also catches any NaN
        }
    }
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

    // Channels deepen here as well. They are drawn at every speed, so if only
    // the routed model maintained them they would freeze at whatever depth they
    // had when the simulation was last slowed down, and a valley would stop
    // responding to the ground moving underneath it.
    evolveChannels(change, dt);

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
