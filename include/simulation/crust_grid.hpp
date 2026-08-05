#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <vector>

#include "simulation/climate.hpp"

namespace simulation {

// Plate tectonics on a geodesic grid.
//
// This is the simulation state of the planet's crust. Large-scale elevation is
// an OUTPUT of it, not an input: crust thickness and density float on the
// mantle, and Airy isostasy decides what is continent and what is ocean floor.
// Noise appears only in the initial condition, as the primordial thickness
// heterogeneity that decides where the first cratons sit.
//
// Why a surface grid and not the voxel octree: crustal thickness runs from
// 7 km (oceanic) to 70 km (orogens), so resolving it in voxels would need
// kilometre-scale cells over the whole planet - of order 10^7 columns stepped
// every tick. The physics is columnar anyway. Voxels remain the detail and
// local-edit representation, derived from this.
class CrustGrid {
public:
    // Physical constants. These are measured quantities, not tuning knobs.
    struct Constants {
        float mantleDensity      = 3300.0f;   // kg/m^3, asthenosphere
        float continentalDensity = 2750.0f;   // kg/m^3
        float oceanicDensity     = 2950.0f;   // kg/m^3

        float continentalThickness = 35000.0f; // m, typical craton
        float oceanicThickness     =  7000.0f; // m, typical mature seafloor
        float maxCrustThickness    = 70000.0f; // m, Himalayan-scale orogen
        float minCrustThickness    =  5000.0f; // m, hyper-extended margin

        // Half-space cooling: oceanic lithosphere subsides as sqrt(age).
        // Earth's seafloor drops ~3.1 km between the ridge and 80 Ma.
        float thermalSubsidenceRate = 347.0f;  // m per sqrt(My)
        float thermalSubsidenceMaxAge = 80.0f; // My, subsidence plateaus

        // Total ocean water expressed as a global equivalent layer - the depth
        // it would form if spread over a smooth planet. Earth's is ~2.7 km.
        // Sea level is solved from this, so growing continents raise it.
        float oceanWaterGEL = 2700.0f;         // m

        // How much of the surface carries continental crust at t = 0, i.e. how
        // much felsic crust differentiated early. Earth's continental crust
        // covers ~40% of the surface, much of it submerged shelf. This is an
        // initial condition, not a target for the finished planet - tectonics
        // moves it from here.
        float initialContinentalFraction = 0.38f;

        // Crust denser than this founders and subducts; lighter crust is too
        // buoyant to be pulled under, which is why continents survive while
        // ocean floor is recycled.
        float subductionDensity = 2850.0f;     // kg/m^3

        // Fraction of subducted crustal volume that comes back as new
        // continental crust through arc magmatism: the slab dehydrates, the
        // mantle wedge melts, and andesite is emplaced on the overriding
        // plate. This is how continents grow, and without it they can only
        // shrink. The least well constrained number here - Earth's continental
        // growth is order 1-2 km^3/yr against ~3 km^3/yr subducted, but much
        // arc crust is itself recycled.
        //
        // That range is a third to two thirds. This was a quarter, which is
        // below the range its own comment cites, and the planet showed it:
        // continental crust fell by forty per cent over a hundred million years
        // and settled at a tenth of the surface as land, against Earth's near
        // third. Arcs were returning thirteen per cent of everything the crust
        // lost.
        //
        // Measured across the range before changing it, because a constant that
        // is adjusted until the output looks right is worth nothing. Land after
        // a hundred and twenty million years comes out at nine per cent for a
        // quarter, fourteen and a half for a half, and fifteen and a half for
        // three quarters - so it responds, and it saturates, which means this
        // is what sets the equilibrium rather than merely correlating with it.
        // At a half the continental budget roughly balances instead of draining.
        //
        // Calibrated, then, in the same way and with the same justification as
        // the asthenosphere viscosity: an admittedly unconstrained quantity set
        // so that a known outcome comes out right. The midpoint of the observed
        // range is also the value that balances the budget, which is the only
        // reason to trust it.
        float arcProductionRatio = 0.5f;

        // Crust parcels per grid cell. More markers means finer material
        // detail and a smoother projection, at linear cost.
        int markersPerCell = 6;
        int maxMarkersPerCell = 16;

        // --- What actually moves the plates -------------------------------
        //
        // Plate motion is not prescribed. Each plate is in instantaneous
        // torque balance - inertia is meaningless at these viscosities - so
        // the driving torques are summed over its boundaries and the drag
        // tensor is inverted to get its rotation. Everything below is a
        // measured quantity or a standard estimate, not a dial.

        // Mean density of the whole body, used to derive surface gravity.
        // Earth's is 5515 kg/m^3.
        float bodyDensity = 5500.0f;           // kg/m^3

        // A cooling slab is denser than the mantle it sinks through, by about
        // rho * alpha * dT averaged over the thermal boundary layer.
        float slabDensityContrast = 64.0f;     // kg/m^3

        // Half-space cooling gives a lithosphere thickness of 2.32*sqrt(kappa t),
        // which is ~116 km at 80 Ma - close to what seismology sees.
        float lithosphereThicknessCoeff = 13034.0f;  // m per sqrt(My)

        // How far a slab descends before the transition zone supports it and
        // it stops pulling any harder.
        float slabMaxLength = 600000.0f;       // m

        // Ridge push is the integrated buoyancy of the cooling column, so it
        // grows with the age of the lithosphere being pushed. This coefficient
        // gives ~3e12 N/m at 80 Ma, an order of magnitude below slab pull,
        // which is the observed ratio.
        float ridgePushPerMy = 3.75e10f;       // N/m per My

        // Asthenosphere viscosity over channel thickness sets the basal drag.
        // Observational estimates span 1e19 to 1e21 Pa s; this sits inside
        // that range and is the one number here fixed by calibration - it is
        // chosen so that Earth-sized plates under Earth-sized forces come out
        // moving at the few cm/yr we actually measure.
        float asthenosphereViscosity = 3.0e20f;   // Pa s
        float asthenosphereThickness = 150000.0f; // m

        // Continental lithosphere has deep roots that grip the mantle harder,
        // which is why continent-heavy plates are the slow ones.
        float keelDragFactor = 2.5f;

        // Continental crust arriving at a trench cannot subduct, so the
        // boundary locks and convergence has to stop. This is what ends an
        // orogeny.
        float collisionDragFactor = 12.0f;

        // Plates take time to respond as slabs detach and boundaries
        // reorganise; they do not snap to a new velocity the instant the
        // forces change.
        float plateResponseTime = 10.0f;       // My

        // Crust past what the column can support is shed the instant it is
        // noticed, and that is known to be wrong without it being known what is
        // right.
        //
        // How much crust the planet destroys depends on how finely it is
        // divided - level 6 sheds one and a half times as much per million
        // years as level 5 on the same planet, which is why the crustal budget
        // comes out at minus eighteen per cent over a hundred million years on
        // one grid and plus two on the other. A one-sided limit applied to a
        // projected, and therefore noisy, thickness takes the upper tail and
        // leaves the lower, so the mean walks at a rate set by the noise.
        //
        // The obvious fix does not work. Shedding the excess over a foundering
        // timescale of a few million years, so the amount removed follows
        // elapsed time rather than the sampling schedule, made every measure
        // worse: the resolution ratio went from 1.50 to 1.78, denudation rose
        // from ninety-seven metres per million year to a hundred and seventy
        // five, and two unrelated invariants broke. Crust that is not shed
        // stays, so columns sit above capacity, stand higher and erode harder,
        // and the excess that accumulates is itself resolution-dependent.
        //
        // So the bias is real and the timescale is not the answer. Left as it
        // was rather than left half-changed.

        // --- Plate reorganisation ------------------------------------------
        //
        // Rigid plates that only ever rotate settle into one arrangement and
        // stay there. Real plates break up and weld together, and that is the
        // whole Wilson cycle: oceans open, continents drift and collide,
        // supercontinents assemble and then rift apart again.

        // How often to look for reorganisation. Boundaries do not rearrange
        // every few hundred thousand years, and the search is not free.
        float reorganisationInterval = 20.0f;  // My

        // A plate breaks when the forces on one part of it disagree strongly
        // enough with the forces on another. Expressed relative to the pull
        // already acting on the plate.
        float splitDisagreementRatio = 0.35f;

        // Continental crust is a thermal blanket. Gather enough of it into one
        // plate and the mantle beneath cannot shed its heat, the region domes
        // up, and the interior goes into tension until it rifts. This is why
        // supercontinents do not last - Pangaea assembled and broke apart, and
        // Rodinia before it.
        //
        // Measured as the fraction of the planet's surface the plate's
        // continental crust covers, because what traps the heat is the size of
        // the blanket - how far the interior sits from any edge. This was
        // originally a share of all continental crust, which erosion exposed
        // as the wrong measure: sediment aprons on passive margins are light
        // enough to count as continental, so the total grows over time and
        // every plate's share falls even as the landmasses gather.
        // Earth's continents cover ~29% of the surface and Pangaea held nearly
        // all of them in one plate, so a blanket of this size is genuinely a
        // supercontinent rather than just a large landmass.
        float supercontinentFraction = 0.30f;

        // Below this share of the planet's surface a fragment is not worth
        // tracking as its own plate and gets absorbed by a neighbour.
        //
        // A fraction rather than a cell count, which is what this was. Sixty
        // cells is 0.15% of the surface at 40,962 cells and 0.04% at 163,842,
        // so refining the grid quietly licensed plates a quarter of the area -
        // and a small plate is a fast one. Driving force scales with a plate's
        // perimeter and basal drag with its area, so for a compact patch the
        // ratio goes as one over the square root of its cell count times the
        // cell spacing: halve the spacing and a plate of the same cell count
        // moves twice as fast.
        //
        // That is how a resolution change turned into a runaway. A microplate
        // reaching metres per year sets the stable timestep for the whole
        // planet, because no plate may cross more than half a cell in a step -
        // so one fragment nobody can see stops geological time for everything.
        float minPlateFraction = 0.0015f;

        // Plates whose shared boundary is mostly locked continental collision
        // have stopped moving relative to each other, so they are one plate.
        float weldCollisionFraction = 0.6f;

        // A ceiling so a pathological configuration cannot spawn plates
        // without limit.
        int maxPlates = 40;

        // --- Erosion -------------------------------------------------------
        //
        // Rivers do the work. Incision follows the stream power law,
        // E = K A^m S^n, which is the standard geomorphic transport law: the
        // more water passing through and the steeper the ground, the faster
        // the channel cuts down.
        //
        // At 17 km cells this resolves continental denudation rather than
        // individual rivers - a "channel" here is a whole drainage basin.

        // Erodibility. This is a measured field quantity that spans orders of
        // magnitude with rock type and rainfall; the value here is set so that
        // mean continental denudation lands near the 30-100 m/My that is
        // actually observed, which is the same kind of calibration as the
        // asthenosphere viscosity.
        float streamPowerCoefficient = 0.03f;  // K
        float drainageExponent = 0.5f;         // m, the usual value
        float slopeExponent = 1.0f;            // n, the usual value

        // How much sediment a river can carry relative to what it is cutting.
        // High enough that steep rivers carry their load through to the sea,
        // low enough that it drops out when the gradient flattens - which is
        // what builds floodplains, deltas and continental shelves.
        float transportCapacity = 25.0f;

        // Hillslope creep. Real diffusivity is 0.01-0.1 m^2/yr, which matters
        // at the scale of a hillside and is nearly invisible across a 17 km
        // cell; it is here for completeness rather than effect.
        float hillslopeDiffusivity = 5.0e4f;   // m^2/My

        // Sets the overall scale of erosion. Where the rain actually falls
        // now comes from the climate model, which returns a multiplier on
        // this per cell - so this carries the absolute rate and climate
        // carries the pattern.
        float precipitation = 1.0f;

        // --- What runs at which speed ---------------------------------------
        //
        // Below this step length, erosion is routed in full. Above it, the
        // bulk model runs instead. Fifty thousand years is roughly where a
        // drainage network stops being a thing that changes within a step and
        // starts being a thing that is simply there.
        float routedErosionBelow = 0.05f;   // My

        // Marker advection, crustal reconciliation and parcel rebalancing run
        // on their own schedule rather than every step, and are given the
        // accumulated time when they do.
        //
        // A plate at six centimetres a year moves sixty metres in a thousand
        // years, against a cell seventeen kilometres across. Advecting parcels
        // every step at that scale is not an approximation of anything - it is
        // arithmetic on a third of a per cent of a cell, and it is half of what
        // a step costs. Given the accumulated time instead, the parcels end up
        // in exactly the same place.
        float tectonicInterval = 0.05f;     // My


        // How much geological time the routed erosion model covers in one
        // pass, however finely the caller happens to be stepping.
        //
        // Rebuilding the drainage network means taking, for each cell, whichever
        // neighbour is lowest - an argmax over near-equal numbers drawn from a
        // field that moves every step. It changes its mind whether or not
        // anything has happened, and it does so once per rebuild rather than
        // once per million years.
        //
        // Measured from two directions. The largest catchment moves eighty per
        // cent per million years when the network is rebuilt every twenty
        // thousand years and four per cent when it is rebuilt every two
        // million. And going from level 6 to level 7 halves the stable timestep,
        // so it doubles the number of rebuilds for the same geology - and
        // doubles the rewiring per cell, which is why the finer grid came out
        // less steady rather than more.
        //
        // So how fast rivers reorganised was set by how often they were asked,
        // which is not a property of the planet. Banking the time and running
        // the pass on its own schedule makes it one again. It costs no fidelity:
        // twenty million years cut into half-million and five-million year
        // slices already lands within one per cent on peak height.
        float erosionInterval = 0.1f;   // My

        // How often to rebuild the drainage network when the routed model is
        // not running. A river system outlives a couple of million years, and
        // rebuilding it costs one erosion pass without the incision - so this
        // is what keeps rivers on the map at tectonic speeds for almost
        // nothing.
        float networkInterval = 2.0f;       // My

        // How much water has to gather before there is a channel at all.
        //
        // Below this a cell erodes as a hillslope: the material comes off the
        // whole cell evenly and no channel is cut. Above it the same volume is
        // taken out of a strip a few hundred metres wide, which is what makes a
        // valley rather than a dip. One number, shared by the routing, the
        // channel bookkeeping and the drawing, so all three agree on where
        // rivers exist.
        float channelThreshold = 3.0f;   // upstream cells

        // How often to re-solve the climate. Continents move slowly, so what
        // they do to the winds changes slowly; resolving it every sub-step
        // would cost as much as the tectonics and change almost nothing.
        float climateInterval = 10.0f;   // My

        // How quickly ground held by two plates at once is resolved - the
        // dense side descending, the buoyant side docking. Not instant,
        // because a slab takes time to go down, but fast enough that plates
        // never visibly occupy the same place.
        float overlapResolutionTime = 2.0f;    // My
    };

    // Surface gravity, derived from radius and mean density rather than
    // assumed. A smaller world pulls its slabs down more weakly, so its
    // tectonics really are slower in absolute force - this falls out instead
    // of being asserted.
    float getSurfaceGravity() const;

    struct Plate {
        // Angular velocity as a single vector, radians per million years:
        // direction is the Euler pole, magnitude the rotation rate. Stored
        // this way because it is what the torque balance solves for - keeping
        // an axis and a rate separately would mean renormalising a quantity
        // that has no reason to stay unit length.
        glm::vec3 omega{0.0f};
        bool oceanic = true;

        // Diagnostics from the last solve, so the forces driving a plate can
        // be inspected rather than inferred. Torques in N m, and in double
        // because they run to 1e28 and beyond - the same range problem that
        // made the solve itself silently fail.
        glm::dvec3 slabPullTorque{0.0};
        glm::dvec3 ridgePushTorque{0.0};
        float area = 0.0f;                     // m^2
        float subductingLength = 0.0f;         // m of trench
        float ridgeLength = 0.0f;              // m of spreading centre
        float collidingLength = 0.0f;          // m of continental collision

        glm::vec3 eulerPole() const {
            const float rate = glm::length(omega);
            return rate > 1e-12f ? omega / rate : glm::vec3(0.0f, 1.0f, 0.0f);
        }
        float angularVelocity() const { return glm::length(omega); }
    };

    struct Cell {
        glm::vec3 position{0.0f};  // unit vector, fixed - the grid is Eulerian
        uint16_t plateId = 0;
        float thickness = 7000.0f; // m
        float density = 2950.0f;   // kg/m^3
        float age = 0.0f;          // My since this crust formed
        float elevation = 0.0f;    // m above the isostatic datum, derived
    };

    // What a layer of rock is made of. Densities are the measured values for
    // each rock type and are what isostasy floats the column on, so a basin
    // full of sediment really is more buoyant than the basalt beneath it.
    enum class RockType : uint8_t {
        Basalt   = 0,   // oceanic crust, erupted at ridges
        Granite  = 1,   // ancient continental crust
        Andesite = 2,   // arc crust, built above subduction zones
        Sediment = 3,   // eroded rock, transported and redeposited
        Count    = 4
    };

    static float rockDensity(RockType rock) {
        switch (rock) {
            case RockType::Basalt:   return 2950.0f;
            case RockType::Granite:  return 2750.0f;
            case RockType::Andesite: return 2800.0f;
            case RockType::Sediment: return 2400.0f;
            default:                 return 2900.0f;
        }
    }

    // How readily each rock erodes, relative to granite. Loose sediment goes
    // first; crystalline basement resists. This is the stratigraphy earning
    // its keep - what a column is made of decides how fast it wears down, so
    // stripping a soft cover off hard basement slows erosion by itself.
    static float rockErodibility(RockType rock) {
        switch (rock) {
            case RockType::Sediment: return 3.0f;
            case RockType::Basalt:   return 1.3f;
            case RockType::Andesite: return 1.1f;
            case RockType::Granite:  return 1.0f;
            default:                 return 1.0f;
        }
    }

    // One episode in a column's history: rock of a given type, emplaced at a
    // given time. Layers are what makes the planet remember - dig into an
    // orogen and the marine sediment that was once a seabed is still there,
    // because it was recorded rather than reconstructed.
    struct Layer {
        double volume = 0.0;                 // m^3 of rock in this layer
        float age = 0.0f;                    // My since it was emplaced
        RockType rock = RockType::Basalt;
    };

    // How many episodes a parcel remembers before the deepest ones are merged.
    // Bounded so the store stays flat and GPU-friendly; merging the deepest
    // pair is geologically honest, since rock that far down is metamorphosed
    // and homogenised anyway.
    static constexpr int MAX_LAYERS = 8;

    // A parcel of crust. Markers are the material; the grid is only where we
    // look at it.
    //
    // Markers rotate exactly with their plate, so transport introduces no
    // error at all no matter how small the timestep is. That is the whole
    // point: carrying crust as fields on fixed cells means interpolating every
    // step, and interpolation is diffusion. Measured on a single plate
    // rotating once around the planet - a pure coordinate change that should
    // return every column to where it started - field transport destroyed 57%
    // of the crustal contrast. Markers destroy none, because nothing is ever
    // averaged: a parcel of granite stays that parcel of granite.
    struct Marker {
        glm::vec3 position{0.0f, 0.0f, 1.0f}; // unit vector
        uint16_t plateId = 0;

        // The rock record, oldest at [0]. Volume, density and age below are
        // derived from it, cached so the hot loops do not have to walk the
        // stack.
        Layer layers[MAX_LAYERS];
        uint8_t layerCount = 0;

        // Double, because this is the conserved quantity. A parcel holds
        // ~1e11 m^3 and is repeatedly added to and subtracted from; in single
        // precision the rounding accumulates into a visible imbalance in the
        // silicate budget over a few hundred steps.
        double volume = 0.0;                  // m^3 of crust in this parcel
        float density = 2950.0f;              // kg/m^3, volume weighted
        float age = 0.0f;                     // My, volume weighted

        // Recompute the cached totals from the layers.
        void refresh() {
            double total = 0.0;
            double mass = 0.0;
            double ageVolume = 0.0;
            for (int i = 0; i < layerCount; i++) {
                total += layers[i].volume;
                mass += layers[i].volume * rockDensity(layers[i].rock);
                ageVolume += layers[i].volume * layers[i].age;
            }
            volume = total;
            density = total > 0.0 ? static_cast<float>(mass / total) : 2950.0f;
            age = total > 0.0 ? static_cast<float>(ageVolume / total) : 0.0f;
        }

        // Lay new rock down on top. If the record is full the two deepest
        // episodes are merged to make room, which is where a real column loses
        // its detail too.
        void deposit(RockType rock, double addedVolume, float atAge);

        // Strip rock from the top, youngest first - what erosion does.
        // Returns how much was actually removed.
        double erodeFromTop(double wanted);

        // Remove rock from the bottom - what delamination does when an
        // over-thickened root turns to eclogite and founders.
        double removeFromBottom(double wanted);

        // Take a fraction out of every layer at once - what subduction does,
        // since the whole slab descends rather than being peeled.
        double consumeProportionally(double wanted);
    };

    // subdivisions: icosphere level. 6 gives 40,962 cells, ~17 km apart on a
    // 1000 km planet. plateCount: number of rigid plates.
    // parcelsPerCell: how many crust parcels to seed in each cell, or zero for
    // the default. Exposed because it is the one knob that changes how noisy
    // the projection from parcels to cells is without changing any physics,
    // which makes it the only way to ask whether a result depends on the
    // sampling rather than on the rock.
    CrustGrid(float planetRadius, uint32_t seed, int subdivisions = 6, int plateCount = 12,
              int parcelsPerCell = 0);

    // Advance the simulation. A request larger than the stable step is split
    // internally, so callers can ask for any interval.
    void step(float millionYears);

    // Largest timestep that keeps plate motion resolved against the grid, in
    // My. Transport itself is exact at any step, but the boundary processes
    // are not: a plate that crosses several cells at once has its trenches and
    // ridges sampled only where it happens to land. Callers that want to
    // budget their own time should step in slices of this.
    float maxStableTimestep() const;

    // An immutable picture of the surface at one instant.
    //
    // The simulation runs on its own thread, so the renderer cannot read the
    // live grid - it would be sampling a field that is being rewritten
    // underneath it. Instead each step publishes one of these and the renderer
    // holds whichever is newest for as long as it needs. Nothing here is ever
    // modified after publication, so no locking is needed to read it.
    //
    // Only the fields change; cell positions and adjacency are fixed at
    // construction, so a snapshot carries just the elevations.
    struct Snapshot {
        std::vector<float> elevation;   // per cell, metres relative to sea level

        // How elevation is changing at each cell, in metres per metre, as a
        // vector lying in the sphere's tangent plane there.
        //
        // Blending three cell values across a triangle reproduces the values
        // but not the shape between them: the result is flat inside every
        // triangle and creased along every edge, so the surface reads as a
        // faceted shell rather than as ground. Knowing the slope at each
        // corner as well as the height is what lets the surface curve.
        //
        // Computed once when the snapshot is taken, because it is the same
        // for every one of the millions of samples the renderer will draw
        // from it.
        std::vector<glm::vec3> elevationGradient;

        // Carried so the renderer can show what the simulation is thinking,
        // not just what the surface looks like. Being able to colour the
        // planet by plate is the difference between "that island looks odd"
        // and "that terrane is docking".
        std::vector<uint16_t> plateId;
        std::vector<float> crustAge;      // My
        std::vector<float> crustThickness;// m
        std::vector<uint8_t> surfaceRock; // RockType at the top of the column
        std::vector<float> cloudCover;    // 0 to 1, from the climate model

        // Surface temperature in degrees C, from the climate model.
        //
        // Published because the snow line is a temperature, and without this
        // the renderer had no way to ask what the temperature was - so it used
        // latitude and a fraction of the tallest mountain on the planet
        // instead.
        std::vector<float> temperature;

        // The drainage network erosion routes water down. Accumulated
        // discharge at each cell, and which neighbour it flows into.
        //
        // Erosion computed all of this every step and threw it away, having
        // used it to decide incision rates. It is a river network - depressions
        // filled, flow routed downhill, discharge accumulated from the top of
        // the landscape down - and keeping it costs two arrays.
        std::vector<float> discharge;     // m^3-ish per step, relative units
        std::vector<int32_t> flowsInto;   // cell index, or -1 at a sink

        // How deep standing water lies here, in metres.
        //
        // Erosion fills depressions before routing flow, because a pit with no
        // outlet stops the routing dead - and a filled depression is a lake.
        // The depth is what the fill added, so this is not an extra model, it
        // is a number the routing already computed and discarded.
        //
        // Also the reason a cell can legitimately drain to a neighbour that is
        // higher than it: across a lake the water surface is level, so flow
        // follows the filled surface rather than the rock.
        std::vector<float> lakeDepth;

        // How deep the river has cut below the mean surface of its cell, in
        // metres. Sub-grid: the cell is seventeen kilometres across and the
        // channel is a few hundred metres of it, so this is the part of the
        // relief the grid cannot hold but the simulation still knows.
        std::vector<float> channelDepth;

        // Where the channel crosses each cell, as a unit direction, rather
        // than the cell centre.
        //
        // The routing can only say which neighbour a cell drains into, so the
        // drawn river ran centre to centre and its geometry was pinned to a
        // seventeen kilometre lattice - every bend a sixty degree corner in the
        // same six directions, which reads as a diagram however much meander
        // noise is laid over it. The crossing point is below the grid, but it
        // is not unknown: a channel entering from one neighbour and leaving
        // towards another cuts the inside of that bend, and where it cuts is
        // decided by the two directions the simulation already knows.
        //
        // Derived rather than stored, so there is no second piece of state to
        // drift out of agreement with the network it describes.
        std::vector<glm::vec3> channelPoint;

        // How the crust under each cell is turning, in radians per million
        // years, as the Euler vector of the plate that owns it.
        //
        // The grid is Eulerian and the rock is not, so a fixed latitude and
        // longitude is not a fixed piece of ground. Watching one place for six
        // million years put the continent hundreds of kilometres outside an
        // eighty kilometre view and showed open ocean, which reads as the river
        // having vanished. Anything meaning to follow a feature has to move
        // with the plate carrying it, and this is what says how.
        std::vector<glm::vec3> crustOmega;

        // The surface the network was routed on, in metres relative to sea
        // level: elevation with depressions filled, at the moment the routing
        // ran.
        //
        // Published because the network is not rebuilt every step - it outlives
        // a couple of million years, and rebuilding it more often costs an
        // erosion pass for no gain. But plates move and isostasy adjusts in the
        // meantime, so by the time anything reads the network the elevations it
        // was routed on are no longer the current ones. Comparing the two gives
        // cells that appear to drain uphill by several hundred metres, with
        // nothing wrong in the routing at all.
        //
        // With this, flow direction can be checked against the surface it was
        // actually derived from, which is an invariant that holds by
        // construction rather than one that holds when the timing happens to
        // line up.
        std::vector<float> routedSurface;

        float minElevation = 0.0f;
        float maxElevation = 0.0f;
        float seaLevel = 0.0f;
        float simulationTime = 0.0f;
        uint64_t version = 0;
    };

    // Take a picture of the current surface. Called on the simulation thread.
    std::shared_ptr<const Snapshot> publishSnapshot() const;

    // Large-scale elevation at a direction on the sphere, in metres relative
    // to sea level. Resolves down to cell spacing; finer detail is the
    // renderer's business.
    float sampleElevation(const glm::vec3& sphereNormal) const;

    // The same, read from a published snapshot rather than the live grid.
    // Safe to call from any thread: it touches only the snapshot and the
    // grid's fixed topology.
    float sampleElevation(const Snapshot& snapshot, const glm::vec3& sphereNormal) const;

    // Everything about the surface at one point that the renderer needs in
    // order to decide what the ground between cells should look like.
    //
    // Relief below the simulation's own resolution has to come from somewhere,
    // and generic noise everywhere is the wrong somewhere - it gives a
    // mountainside and an abyssal plain the same texture. What the simulation
    // already knows is enough to condition it: how steep the ground is, and
    // what it is made of.
    struct SurfaceSample {
        float elevation = 0.0f;   // metres relative to sea level
        float slope = 0.0f;       // metres per metre, so dimensionless
        uint8_t rock = 0;         // RockType exposed at the top of the column

        // How broken the ground is here, blended across cells rather than
        // taken from whichever one dominates.
        //
        // The rock type cannot be blended - the average of basalt and granite
        // is not a rock - but what the renderer wants from it is a number, and
        // numbers blend fine. Choosing per sample instead put a hard step in
        // the surface texture at every cell boundary, and made the texture of
        // a whole neighbourhood jump the moment a cell's dominant parcel
        // changed, which reads as detail flickering on and off.
        float roughness = 1.0f;
    };

    // How much relief a rock type holds at scales below the grid. Sediment is
    // laid down by water and lies flat; lavas and granites break, joint and
    // keep an edge.
    static float rockRoughness(RockType rock) {
        switch (rock) {
            case RockType::Sediment: return 0.35f;
            case RockType::Basalt:   return 0.85f;
            case RockType::Granite:  return 1.00f;
            case RockType::Andesite: return 1.15f;
            default:                 return 1.00f;
        }
    }
    SurfaceSample sampleSurface(const Snapshot& snapshot, const glm::vec3& sphereNormal) const;

    // Sky cover at a direction, blended between cells. Read from a published
    // snapshot so the renderer's worker threads can ask while the simulation
    // is mid-step.
    // Surface temperature at a direction, blended across the triangle a point
    // falls in rather than taken from the nearest cell.
    //
    // Nearest-cell would be simpler and is visibly wrong: it makes the field
    // piecewise constant, so anything thresholded against it - the snow line,
    // for one - draws the grid. The planet grew hexagons the first time this
    // was tried the easy way.
    float sampleTemperature(const Snapshot& snapshot,
                            const glm::vec3& sphereNormal) const;

    float sampleCloudCover(const Snapshot& snapshot, const glm::vec3& sphereNormal) const;

    // Where the nearest river channel is, and how big it is.
    //
    // A cell carrying a major river is seventeen kilometres across and the
    // river is not, so colouring whole cells would draw rivers as wide as
    // Belgium. The flow direction is known, though - each cell routes into a
    // specific neighbour - so the channel is a path, and this reports how far
    // a point lies from it.
    //
    // Distance and discharge rather than one blended strength, because a river
    // shapes the ground at two scales at once: a channel a kilometre across
    // and a valley ten times that. A single number cannot express both, and
    // the valley is most of what makes a river visible.
    // How steeply the side of a river valley can stand: the angle loose rock
    // comes to rest at, about thirty-one degrees. It sets how wide a valley of
    // a given depth must be, which decides both how it is drawn and how long it
    // takes to fill in once the river has gone. Shared so those two cannot
    // disagree about the same valley.
    static constexpr float VALLEY_WALL_SLOPE = 0.6f;

    // What the routing did, and how much of it was warranted.
    //
    // The network is rebuilt from scratch every step by taking, for each cell,
    // whichever neighbour is lowest. That is an argmax over near-equal numbers
    // drawn from a field that moves every step, so it will change its mind
    // whether or not anything has happened. This counts how often it changes
    // its mind for a reason: a new receiver is warranted only if it is lower
    // than the one being abandoned by more than the depth of the channel the
    // water is currently sitting in, which is the condition for the flow
    // actually being able to climb out and go elsewhere.
    struct DrainageAudit {
        long long routed = 0;        // cells given a receiver
        long long changed = 0;       // receivers different from last step
        long long warranted = 0;     // changes that clear the channel depth
        double abandonedDepth = 0.0; // total channel depth walked away from, m
    };
    // How much rock erosion has moved, cumulatively, in cubic metres.
    //
    // Kept because a denudation rate is the one number in this whole model that
    // can be checked against the world rather than against my judgement:
    // continents wear down at thirty to a hundred metres per million years, and
    // that is measured, not modelled. Everything downstream of erosion is built
    // on the assumption that it is roughly right, and until this existed the
    // assumption had never been tested.
    struct ErosionBudget {
        double eroded = 0.0;     // m^3 cut out of hillsides and channels
        double deposited = 0.0;  // m^3 laid down again
        float simulatedTime = 0.0f;  // My covered by the above
    };
    // Where crust comes from and where it goes, cumulatively, in cubic metres.
    //
    // Continental crust was measured draining away at forty per cent per
    // hundred million years and two guesses at the cause were both wrong, which
    // is what this is for. Every path that creates or destroys crust reports
    // through it, so the question stops being which mechanism sounds most
    // likely and becomes which number is largest.
    struct CrustBudget {
        double meltFromMantle = 0.0;      // new basalt at rifts and ridges
        double arcFromMantle = 0.0;       // andesite returned above slabs
        double subducted = 0.0;           // dense crust pulled under
        double delaminated = 0.0;         // buoyant roots foundering
        double riftedAway = 0.0;          // continental crust stretched to ocean
        float simulatedTime = 0.0f;

        // How the shedding is distributed, which is what tells noise from
        // physics. A column genuinely thickened by collision sits far above its
        // capacity; a column that only reads too thick because its parcels
        // happened to land generously sits barely above it. The two look
        // identical in a total and completely different in a histogram.
        long long shedEvents = 0;         // cells shedding at all
        long long marginalEvents = 0;     // shedding less than a twentieth of capacity
        double marginalVolume = 0.0;      // m^3 shed by those
        double excessThickness = 0.0;     // m of excess, summed over events
    };
    const CrustBudget& getCrustBudget() const { return crustBudget; }
    void resetCrustBudget() { crustBudget = CrustBudget{}; }

    const ErosionBudget& getErosionBudget() const { return erosionBudget; }
    void resetErosionBudget() { erosionBudget = ErosionBudget{}; }

    const DrainageAudit& getDrainageAudit() const { return drainageAudit; }
    void resetDrainageAudit() { drainageAudit = DrainageAudit{}; }

    struct RiverSample {
        float distance = 1e30f;    // metres to the channel centreline
        float width = 0.0f;        // channel width in metres
        float catchments = 0.0f;   // upstream cells draining through it
        float depth = 0.0f;        // metres cut below the surrounding ground
    };
    RiverSample sampleRiverGeometry(const Snapshot& snapshot,
                                    const glm::vec3& sphereNormal) const;

    // How wide a channel carrying this many upstream cells is. One rule, so
    // that what is drawn and what is measured cannot drift apart.
    float channelWidthFor(float catchments) const;

    // How strongly a river runs at a direction, 0 to 1. A readout of the
    // above, for colouring.
    float sampleRiver(const Snapshot& snapshot, const glm::vec3& sphereNormal) const;
    SurfaceSample sampleSurface(const glm::vec3& sphereNormal) const;

    // Nearest cell to a direction, via the spatial accelerator.
    int findNearestCell(const glm::vec3& sphereNormal) const;

    // The three cells surrounding a direction, and how much each contributes.
    //
    // Cell centres are the vertices of a triangulation of the sphere, so any
    // direction falls inside one triangle and the value there is the linear
    // blend of its corners. Weights sum to one and each goes to one exactly at
    // its own cell, which is what makes the reconstruction continuous across
    // the whole sphere with no flat spots anywhere.
    //
    // This replaced weighting the nearest cell and its ring by inverse square
    // angular distance. That weight is singular at a cell centre, so every
    // cell got a plateau of its own value with a step at the edge, and the
    // grid showed through the rendered surface as hexagons.
    //
    // Returns false only if the direction is degenerate or the grid is empty.
    bool barycentricCells(const glm::vec3& sphereNormal, int outCells[3],
                          float outWeights[3]) const;

    // Elevation as a flat array, with its slope at every cell, refreshed
    // whenever the surface moves. Kept because both the renderer's snapshot
    // and the live sampler want the same gradients, and fitting them is
    // O(cells) once against millions of samples read from them.
    // The last drainage network erosion routed, kept for the renderer.
    std::vector<float> lastDischarge;

    // How deep the channel is at each cell. Simulation state, not decoration:
    // it is cut by incision, filled by deposition and by the hillslopes either
    // side of it, and it is what a river has to climb out of to change course.
    std::vector<float> channelDepth;

    // Move that depth by one step, given how much fluvial erosion or deposition
    // happened at each cell in metres of mean column.
    void evolveChannels(const std::vector<double>& fluvial, float dt);

    // Where the channel crosses each cell, worked out from the flow through it.
    std::vector<glm::vec3> lastChannelPoint;

    DrainageAudit drainageAudit;
    ErosionBudget erosionBudget;
    CrustBudget crustBudget;

    // Geological time banked towards the next routed erosion pass.
    float erosionDebt = 0.0f;

    std::vector<int32_t> lastFlowsInto;
    std::vector<float> lastLakeDepth;
    std::vector<float> lastRoutedSurface;

    // Scratch for the parallel half of the projection: where each parcel lands
    // and how its volume divides between that cell and its neighbours. Held as
    // a member so it is not reallocated every step.
    struct Projection {
        static constexpr int MAX = 7;   // a cell and up to six neighbours
        int landing = -1;
        int count = 0;
        int cells[MAX] = {};
        double shares[MAX] = {};
    };
    std::vector<Projection> projection;

    std::vector<float> elevationField;
    std::vector<glm::vec3> elevationGradient;
    void refreshElevationField();

    // Slope of a per-cell field at one cell, fitted from its neighbour ring.
    glm::vec3 estimateGradient(int cell, const std::vector<float>& values) const;

    // A per-cell field sampled anywhere on the sphere, curving between cells
    // rather than blending flat across them. Falls back to the flat blend if
    // no gradients are supplied.
    float reconstruct(const glm::vec3& sphereNormal, const std::vector<float>& values,
                      const std::vector<glm::vec3>& gradients) const;

    const std::vector<Cell>& getCells() const { return cells; }
    const std::vector<Plate>& getPlates() const { return plates; }
    const std::vector<Marker>& getMarkers() const { return markers; }

    // Where a step's time actually goes, in milliseconds, from the last one.
    //
    // Needed to design rather than guess. Erosion is the obvious suspect for
    // being the expensive phase, and "obvious suspect" has been wrong often
    // enough in this project to be worth measuring instead.
    struct Timings {
        float plateMotion = 0.0f;
        float advection = 0.0f;
        float reconcile = 0.0f;
        float isostasy = 0.0f;
        float climate = 0.0f;
        float erosion = 0.0f;

        // Erosion's own phases, because it is now the largest one and
        // "parallelise erosion" is not a plan until it is known which part of
        // it costs anything. Two of these cannot be threaded at all - a
        // priority queue and a walk in downstream order are both sequential by
        // construction - so it matters a great deal whether they are the
        // expensive ones.
        float erosionFill = 0.0f;
        float erosionRoute = 0.0f;
        float erosionAccumulate = 0.0f;
        float erosionIncise = 0.0f;
        float erosionCreep = 0.0f;
        float erosionApply = 0.0f;
        float rebalance = 0.0f;
        float gradients = 0.0f;
        float total = 0.0f;
    };
    const Timings& getTimings() const { return timings; }

    // Adjacency, as an allocation-free range. Geodesic cells have 5 or 6
    // neighbours - the twelve pentagons are the icosahedron's original corners.
    int neighbourCount(int cell) const {
        return neighbourStart[cell + 1] - neighbourStart[cell];
    }
    int neighbourAt(int cell, int k) const {
        return neighbourIndices[neighbourStart[cell] + k];
    }

    float getPlanetRadius() const { return planetRadius; }
    float getSeaLevel() const { return seaLevel; }

    // The climate the current arrangement of continents produces. Read by
    // erosion, and worth showing: where the rain falls is visible in the shape
    // of the mountains it wears down.
    const Climate& getClimate() const { return climate; }
    Climate& getClimate() { return climate; }
    float getSimulationTime() const { return simulationTime; }

    // Typical distance between neighbouring cells, in metres. This is the
    // finest scale the simulation resolves; anything below it is the
    // renderer's roughness, not simulated relief.
    float cellSpacing() const;

    // Current elevation extremes relative to sea level, cached each step so
    // per-vertex sampling does not have to scan the whole grid.
    float getMinElevation() const { return minElevation; }
    float getMaxElevation() const { return maxElevation; }

    // Bumped on every step, so renderers can tell when to rebuild.
    uint64_t getVersion() const { return version; }

    // How many times plates have broken up or welded together. A planet whose
    // tectonics genuinely evolves accumulates these; one with frozen
    // kinematics never does.
    uint32_t getSplitCount() const { return splitCount; }
    uint32_t getWeldCount() const { return weldCount; }

    Constants& getConstants() { return constants; }
    const Constants& getConstants() const { return constants; }

    // Surface velocity of the plate owning this point, in metres per My.
    glm::vec3 plateVelocityAt(const glm::vec3& sphereNormal, uint16_t plateId) const;

    // Mean cell area in m^2, used to turn per-cell sums into volumes.
    float getCellArea() const;

    // Silicate volume returned to the mantle, minus what has been drawn out of
    // it as melt, since t = 0. Crust volume plus this must equal the starting
    // crust volume, once advection's numerical leak is accounted for.
    double getMantleReservoir() const { return mantleReservoir; }
    double getInitialCrustVolume() const { return initialCrustVolume; }

    // Volume unaccounted for by any process. Transport is a forward scatter
    // with weights that sum to one, so this should stay at rounding error; if
    // it ever grows, a process has started leaking.
    double getAdvectionDrift() const { return advectionDrift; }

    // Where continental crust goes when it stops being continental. Split by
    // channel so a runaway can be attributed to a process rather than guessed
    // at: rifting thins it until it founders and floods with basalt, and
    // over-thickened orogenic roots turn to eclogite and delaminate.
    double getContinentalLostToRifting() const { return continentalLostToRifting; }
    double getContinentalLostToDelamination() const { return continentalLostToDelamination; }
    double getContinentalCreatedByArcs() const { return continentalCreatedByArcs; }

    // Rock removed by erosion and sediment laid back down, in m^3. These must
    // agree: erosion moves material, it does not destroy it.
    double getErodedVolume() const { return erodedVolume; }
    double getDepositedVolume() const { return depositedVolume; }

    // Diagnostics
    struct Stats {
        float landFraction = 0.0f;
        float meanElevation = 0.0f;
        float minElevation = 0.0f;
        float maxElevation = 0.0f;
        float meanOceanicAge = 0.0f;
        float crustVolume = 0.0f;         // m^3
        float continentalVolume = 0.0f;   // m^3 of buoyant crust
        float oceanicVolume = 0.0f;       // m^3 of dense crust
    };
    Stats computeStats() const;

    // Things that should not be true of a sane planet.
    //
    // Plate tectonics has few closed-form answers to check against, so the way
    // to know it is behaving is to name the things that must never happen and
    // watch for them. Every field here is a violation count or magnitude:
    // small or zero is healthy.
    struct Diagnostics {
        // Crust sitting in a cell whose majority belongs to a different plate.
        // Real plates cannot overlap - one of them subducts - so a persistent
        // non-zero value means two plates are passing through each other.
        float overlapFraction = 0.0f;
        int maxPlatesInOneCell = 0;

        // Largest single-step change in surface height.
        //
        // Not an error bound, which is what this comment used to imply, and the
        // implication cost an afternoon. Kilometres in one step is normal at a
        // moving plate boundary: as a margin sweeps across a cell, seven
        // kilometres of ocean floor is replaced by forty of continent, and Airy
        // isostasy plus the loss of thermal subsidence puts the surface eight
        // kilometres higher. Every cell that moves is accounted for exactly by
        // its own column - thickness, density and age - and there is a test
        // that says so.
        //
        // It also grows as the grid gets finer, which looks like refinement
        // making things worse and is the opposite. A coarse cell straddles the
        // margin and averages both sides, so its readout creeps; a fine cell
        // resolves the margin and flips, so its readout approaches the real
        // contrast. Convergence, not degradation.
        //
        // Still worth watching, because a jump with no column change behind it
        // would be a genuine fault.
        float maxElevationJump = 0.0f;
        int cellOfLargestJump = -1;

        // Cells that no parcel landed in. A few is normal at a spreading
        // ridge; many means transport is losing track of the material.
        int emptyCells = 0;

        // Plates too small to be meaningful, and the largest speed on the
        // planet in cm/yr.
        int microPlates = 0;
        float fastestPlateCmPerYear = 0.0f;
    };
    Diagnostics computeDiagnostics() const;

    // Total crustal volume in m^3.
    double computeCrustVolume() const;

    // Volume of buoyant (continental) crust in m^3.
    double computeContinentalVolume() const;

    // Change in continental volume attributable to transport. Reclassification
    // has no explicit channel - a column can drift across the compositional
    // threshold without any process reporting a loss - so the only reliable
    // way to attribute it is to measure the phase directly.
    double getContinentalDeltaFromTransport() const { return continentalDeltaTransport; }

private:
    float planetRadius;
    uint32_t seed;
    Constants constants;

    // Recomputed when the surface has moved enough to change where the rain
    // falls. The atmosphere settles in weeks against steps of millions of
    // years, so what matters is the equilibrium, not the path to it.
    Climate climate;
    float climateAge = 0.0f;
    Timings timings;

    // Simulated time owed to the phases that do not run every step.
    float tectonicDebt = 0.0f;
    float networkAge = 0.0f;

    std::vector<Cell> cells;
    std::vector<Plate> plates;
    std::vector<Marker> markers;

    // Cell positions again, held apart from the mutable cell fields. Geometry
    // is fixed at construction, so keeping a copy that is never written lets
    // the renderer locate cells while the simulation thread is rewriting
    // everything else about them.
    std::vector<glm::vec3> cellPositions;

    // Which markers currently land in each cell, rebuilt every projection.
    std::vector<std::vector<int>> cellMarkers;

    // Flattened adjacency: neighbours of cell i are
    // neighbourIndices[neighbourStart[i] .. neighbourStart[i+1])
    std::vector<int> neighbourIndices;

    // The triangulation the cell centres are the vertices of, kept so the
    // surface can be reconstructed between them rather than only at them,
    // along with the triangles meeting at each cell. A point's nearest cell is
    // always a corner of the triangle containing it, so those few are the only
    // ones that ever have to be tested.
    std::vector<glm::ivec3> triangles;
    std::vector<int> cellTriangleStart;
    std::vector<int> cellTriangleIndices;
    std::vector<int> neighbourStart;

    // Latitude/longitude bin accelerator over the fixed cell positions.
    static constexpr int BIN_LAT = 64;
    static constexpr int BIN_LON = 128;
    std::vector<std::vector<int>> bins;

    float seaLevel = 0.0f;
    float simulationTime = 0.0f;
    float minElevation = 0.0f;
    float maxElevation = 0.0f;
    uint64_t version = 0;

    double mantleReservoir = 0.0;
    double initialCrustVolume = 0.0;
    double advectionDrift = 0.0;
    double continentalLostToRifting = 0.0;
    double continentalLostToDelamination = 0.0;
    double continentalCreatedByArcs = 0.0;
    double continentalDeltaTransport = 0.0;
    uint32_t splitCount = 0;
    uint32_t weldCount = 0;
    double erodedVolume = 0.0;
    double depositedVolume = 0.0;

    // Previous step's surface, so a jump can be spotted.
    std::vector<float> previousElevation;
    float largestElevationJump = 0.0f;
    int largestJumpCell = -1;

    void buildGeodesicGrid(int subdivisions);
    void buildAccelerator();
    void assignPlates(int plateCount);
    void seedInitialCrust();

    // Solve each plate's rotation from the forces acting on it. Implemented in
    // plate_dynamics.cpp.
    void updatePlateMotion(float dt);

    // Break up plates whose driving forces disagree, and weld together plates
    // that have locked in continental collision. Also in plate_dynamics.cpp.
    // Wear the surface down and move what comes off. In erosion.cpp.
    // Erosion at two fidelities, chosen by how long the step is.
    //
    // A process is only meaningful inside a range of timesteps. A delta forms
    // over thousands of years; asking a two-million-year step to resolve one
    // does not give a coarse delta, it gives a meaningless number. So the
    // choice here is not a performance trade, it is about whether the answer
    // means anything - and running the detailed model outside its range would
    // be a category error that happens to also be slow.
    //
    // Routed: depressions filled into lakes, flow directed downhill, discharge
    // accumulated, stream power incision, sediment carried to capacity and
    // dropped. This is what makes rivers, valleys and deltas.
    // networkOnly builds the drainage network and stops before moving any
    // rock. The network is a property of the shape of the land, not of how
    // long a step is, so rivers have to keep existing at speeds where the
    // routed model has nothing to say - otherwise they vanish the moment
    // anyone speeds the clock up, which is the least explicable thing a
    // simulation can do.
    void erodeSurface(float dt, bool networkOnly = false);

    // Bulk: highlands wear down in proportion to how far they stand above
    // their surroundings and how much rain falls on them, and the sediment
    // goes to the basins. No routing, because over a million years which
    // channel carried a grain is not a question anyone can answer, and the
    // answer would not survive to be seen.
    void erodeBulk(float dt);

    // Move the rock the grid decided to move. Shared, so conservation cannot
    // depend on which erosion model ran.
    void applyErosionChange(const std::vector<double>& change,
                            double eroded, double deposited);

    void reorganisePlates();
    bool trySplitPlate(uint16_t plateId);
    bool riftSupercontinent();
    void weldLockedPlates();
    void absorbTinyPlates();

    // The smallest plate worth having, in cells, derived from the share of the
    // surface it must cover. Everything that asks "is this too small to be a
    // plate" goes through here, so the answer cannot depend on the resolution
    // in one place and not another.
    int minPlateCellCount() const {
        return std::max(4, static_cast<int>(constants.minPlateFraction *
                                            static_cast<float>(cells.size())));
    }

    float sinceReorganisation = 0.0f;   // My

    void stepOnce(float millionYears);
    void seedMarkers();
    void advectMarkers(float dt);
    void projectMarkersToGrid();
    void reconcileCrust(float dt);
    void resolvePlateOverlap(float dt);
    void rebalanceMarkers();
    void updateIsostasy();
    void solveSeaLevel();

    // Isostatic elevation of one column above the compensation datum.
    float isostaticHeight(const Cell& cell) const;

    int binIndex(const glm::vec3& n) const;
};

} // namespace simulation
