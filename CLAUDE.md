# Working notes for this repository

## Rebuild cleanly after changing a header that defines simulation state

Changing the members of `CrustGrid`, `CrustGrid::Snapshot` or anything else
whose size other translation units depend on requires

    cmake --build build_windows --config Release --clean-first

An incremental build does not reliably recompile every dependent. When it does
not, different object files disagree about `sizeof` the same class, which is an
ODR violation, and it shows up as `0xc0000374` - heap corruption - in whichever
tests happen to allocate one first. It looks exactly like a memory bug in
whatever was changed last, and it is not.

This cost most of an afternoon. A perfectly good change to erosion scheduling
was measured, found to work, blamed for crashing four unrelated tests, and
reverted - before a clean rebuild of the identical source passed everything.
If tests start crashing rather than failing an assertion, rebuild clean before
believing anything else.

## Measure before designing, and distrust the measurement first

Nearly every wrong turn in the drainage work has been a wrong measurement
rather than wrong reasoning:

- feeding accumulated drainage volume into a rule expecting a cell count, and
  reading the resulting saturation as the bug
- walking a drainage network against elevations it had never been routed on
- testing valley persistence with the water still running, so abandonment -
  the case that was broken - was never exercised
- sampling a noisy signal every 0.4 My and reporting the aliasing as a cliff
- measuring level 5 when the application runs level 6

When a result is surprising, check what was measured before concluding
anything about the simulation.

The worst of these was an experiment that varied `CrustGrid`'s fourth
constructor argument believing it was the parcel count. It is `plateCount`, so
what got measured was three times the plate boundaries producing three and a
half times the subduction - a sensible result about something nobody had asked
about. Read the signature before varying a positional argument, and be more
suspicious of a clean dramatic result than of a messy one: a ratio of 3.6 is
what prompted the check, and a ratio near 1.0 would have been believed.

## Constants standing in for quantities the code could derive

A recurring defect, now six times over: skirt depth, detail frequency, the bulk
erosion coefficient, minimum plate size as a count rather than a fraction,
channel depth inferred from discharge, and valley width as a fixed multiple of
channel width. Each looked like a tuning parameter and was really a derivable
quantity being guessed at. When adding a constant, check whether something
already in the simulation determines it.

The opposite defect exists too, and is harder to see: a constant that
contradicts its own comment. `arcProductionRatio` was 0.25 while the comment
beside it cited 1-2 km^3/yr returned against ~3 subducted, which is a third to
two thirds. The planet lost forty per cent of its continental crust over a
hundred million years because of it. Read what the comment claims and check the
value is inside it.

## Calibrating against an outcome, when it is allowed

Two constants here are set by calibration rather than measurement: the
asthenosphere viscosity, chosen so Earth-sized plates move at the few cm/yr we
observe, and the arc production ratio, chosen so continents persist. Both are
legitimate because the quantity is genuinely unconstrained by observation and
the target is not.

The test is whether the outcome responds to the parameter and then saturates.
Land came out at 9%, 14.5% and 15.7% for arc ratios of a quarter, a half and
three quarters - so it is what sets the equilibrium rather than merely
correlating with it. Sweep before setting, and record the sweep. Adjusting a
constant until one run looks right is worth nothing.

## Open: the crustal budget depends on resolution

Continental crust changes by -18% over a hundred million years at level 6 and
+2% at level 5, and sea level moves ~300 m in opposite directions. Denudation,
by contrast, is 94 m/My at level 6 and 97 at level 5 - properly independent.

A global mass balance should not care how finely the sphere is divided. The
likeliest culprit is the crustal capacity rule in `reconcileCrust`, which sheds
the excess above a per-cell capacity, so its aggregate effect scales with the
number of cells. Delamination is already the largest single term in the crust
ledger at 51% of all destruction, and it returns nothing.

This matters because `arcProductionRatio` was calibrated at level 6. If the
budget's resolution dependence is fixed, that calibration has to be redone.

What is established, from four measurements on the same planet at both grids:

- shedding runs 1.50x at level 6, rifting 1.03x, so it is the capacity rule
- doubling parcels per cell gives 0.85x and marginal events are 14% of shed
  volume, so projection noise is real but is about a seventh of the effect
- mean excess per event is 1265 m at level 5 and 1245 m at level 6, so it is
  not convergence concentrating into narrower cells - the excesses are the
  same size, there are simply 1.8x more of them per cell
- thickness and density are projected through the same stencil with the same
  weights, so they cannot disagree with each other

Four repairs have been tried and all failed. Rate-limiting the shedding over a
foundering timescale: ratio 1.50 -> 1.78, denudation 97 -> 175 m/My. Smoothing
density over the flexural wavelength: ratio -> 1.43, continental crust -18% ->
-48%. Smoothing capacity instead: ratio -> 1.44, continental crust -> -47%.

The last two failed for a reason worth keeping. Shedding is a one-sided
function of capacity, so any symmetric smoothing increases it: dropping a
continental cell's capacity creates large new shedding because that column
carries 35 km, while raising an oceanic cell's saves almost nothing because
that column carries seven. Smoothing is the wrong tool here in either
direction. And since neither variant moved the ratio, the story that the finer
grid resolves purer cells with extreme capacities is not the dominant cause
either.

So the mechanism is still unexplained. What is not explained is specifically
this: why 1.8x as many columns per cell exceed capacity at level 6, when the
typical excess is unchanged and it is not noise.

## Open: continents concentrate instead of spreading, and it shows as snow

The mottled white blotches over the continents are not weather and not a
colour-band artefact. They are snow, and the land really is at the snow line.

    t= 7 My  land 12238  mean elevation 2616 m  snow line 3345 m  36% above
    t=37 My  land  8719  mean elevation 2835 m  snow line 3182 m  42% above
             mean land crust thickness 33.9 km -> 37.5 km

Continental volume now holds, since the arc production ratio was corrected. The
land *area* does not: it falls by nearly a third over thirty million years while
the volume stays, so the same rock piles into less ground, thickens by three and
a half kilometres, and isostasy lifts it across the snow line. The mean sits
only a few hundred metres below the line, so ordinary relief puts neighbouring
cells on opposite sides of it - which is exactly what produces blotches rather
than an ice cap. It compounds: the ground rises while the snow line falls.

Nothing spreads continental crust sideways. Real continents extend, rift and
collapse under their own weight; here they only thicken. That is probably the
same defect as the capacity rule above, seen from the other end, and this is
the better handle on it - it is visible from orbit, so a repair can be judged
by looking rather than by a resolution ratio.

## Following the crust rather than a coordinate

The grid is Eulerian and the rock is not. Watching a fixed latitude and
longitude for six million years put the continent hundreds of kilometres
outside an eighty kilometre view and showed open ocean, which reads exactly
like the river having vanished.

    python tools/planetctl.py camera track <lat> <lon> <km>

latches onto the ground under that point and advects it by the local plate
rotation every frame, so it stays on the same rock. Use it for anything watched
over time; `camera goto` is for a fixed place on the sphere.

## Driving the simulator

    PlanetSimulator.exe -control 8765
    python tools/planetctl.py find deepest-valley
    python tools/planetctl.py camera goto <lat> <lon> <km>
    python tools/planetctl.py --watch stats --advance 0.5 --steps 20 --out run.csv

`sim advance` specifies geological time and blocks until it has happened, which
is what makes a time series evenly spaced in the units that matter. Most
problems here are only visible over time.
