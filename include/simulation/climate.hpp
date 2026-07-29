#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace simulation {

class CrustGrid;

// Climate on the same geodesic grid the crust uses.
//
// This exists to drive erosion, not to colour the planet. Erosion already
// carries a precipitation term and it was a single number multiplying the
// whole surface, which makes every mountain range erode at the same rate on
// both sides. Real ranges do not: air rises on the windward flank, cools,
// drops its water, and comes down the other side dry, so one face is stripped
// and the other is preserved. That asymmetry shapes where sediment goes, and
// therefore where crust thickens - so climate is a mechanism with consequences
// in the rock record, not decoration on top of it.
//
// Everything here is an equilibrium field, recomputed when the surface has
// moved enough to matter. The atmosphere reaches steady state in weeks and the
// simulation steps in millions of years, so there is nothing to gain from
// integrating it forward in time - only from asking what it settles to given
// the continents as they now stand.
class Climate {
public:
    struct Constants {
        // Solar constant at the planet's orbit. Earth's is 1361 W/m^2.
        float solarConstant = 1361.0f;      // W/m^2

        // Outgoing longwave radiation, linearised about present conditions as
        // A + B*T with T in Celsius. Budyko's coefficients, fitted to
        // satellite measurements of Earth - the linearisation is what makes an
        // energy balance model solvable in closed form instead of needing
        // radiative transfer through a column of air.
        float longwaveOffset = 210.0f;      // W/m^2
        float longwaveSlope = 2.1f;         // W/m^2 per degree C

        // Heat carried from the tropics to the poles by winds and currents.
        // Without it the equator runs far too hot and the poles far too cold -
        // this single term is what keeps a planet's temperature range close to
        // the observed one.
        float meridionalTransport = 3.8f;   // W/m^2 per degree C from the mean

        // Albedo of each surface. Open ocean is dark, ice and snow are bright,
        // and the difference between them is a feedback: cooling grows ice,
        // ice reflects more, which cools further.
        float oceanAlbedo = 0.08f;
        float landAlbedo = 0.25f;
        float iceAlbedo = 0.60f;

        // Air cools as it rises. 6.5 K/km is the observed average through the
        // troposphere - the moist adiabat, not the dry one, because rising air
        // on a wet planet condenses and releases latent heat on the way up.
        float lapseRate = 6.5e-3f;          // K per metre

        // Below this the ground holds snow year round.
        float freezingPoint = 0.0f;         // degrees C

        // How much water the air can hold, doubling roughly every ten degrees.
        // This is Clausius-Clapeyron, and it is why the tropics are wet and
        // the poles are dry deserts however much they are rained on.
        float saturationAtFreezing = 4.8f;  // g/m^3 at 0 C
        float saturationDoubling = 10.0f;   // degrees C per doubling

        // Fraction of the moisture in a parcel that falls per unit of forced
        // ascent. Air pushed up a mountainside cools, passes saturation, and
        // rains; this sets how sharply.
        float orographicRainout = 1.4e-3f;  // per metre of climb

        // Baseline rainout with no relief at all, so flat ground downwind of
        // an ocean is not bone dry.
        float backgroundRainout = 0.04f;    // per transport step

        // How far moisture travels before it is exhausted, as steps across the
        // grid. Air crosses a continent in days, so this is bounded by how
        // much water it started with, not by time.
        int transportSteps = 48;
    };

    // Per cell. Indexed the same as the crust grid's cells.
    struct Fields {
        std::vector<float> temperature;    // degrees C at the surface
        std::vector<float> precipitation;  // metres of water per year
        std::vector<glm::vec3> wind;       // unit tangent vector, prevailing

        // How much of the sky is covered, 0 to 1.
        //
        // Cloud is not a separate thing to model - it is the moisture already
        // being carried, seen against how much the air at that temperature can
        // hold. Air at eighty per cent of saturation is hazy; air at a hundred
        // is raining. The transport already computes both numbers and threw
        // the first away.
        std::vector<float> cloudCover;
        float meanTemperature = 0.0f;      // degrees C, area-weighted
        float iceFraction = 0.0f;          // of the whole surface
    };

    explicit Climate(const CrustGrid& grid);

    // Recompute the equilibrium for the surface as it currently stands.
    void update();

    const Fields& getFields() const { return fields; }
    Constants& getConstants() { return constants; }
    const Constants& getConstants() const { return constants; }

    // Rainfall at a cell, relative to the planet's own mean. This is what
    // multiplies erosion, so it is dimensionless on purpose: the absolute
    // rate is already carried by the stream power coefficient, and what
    // erosion needs from climate is where the water is, not how much.
    float relativePrecipitation(int cell) const;

private:
    const CrustGrid& grid;
    Constants constants;
    Fields fields;

    float meanPrecipitation = 0.0f;

    void solveTemperature();
    void solveWind();
    void solvePrecipitation();

    // Cloud from how much water is condensing, relative to the planet's mean.
    //
    // Not from surface humidity, which is the obvious choice and is useless
    // here: the ocean is held at saturation by definition, so humidity is one
    // over seventy per cent of the planet and the sky comes out uniformly
    // overcast. Cloud is condensed water - the same water that goes on to
    // fall - so where it is condensing is where the cloud is. That puts cloud
    // in the convergence zones and on windward slopes and leaves the
    // subtropics and the lee sides clear, which is the pattern a photograph of
    // a planet actually shows.
    float cloudFromCondensation(float condensing, float mean) const;

    // Water the air can hold at a given temperature, in g/m^3.
    float saturationCapacity(float temperatureC) const;
};

} // namespace simulation
