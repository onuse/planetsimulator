#include "simulation/climate.hpp"
#include "simulation/crust_grid.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "utils/parallel.hpp"

namespace simulation {

namespace {

constexpr float PI = 3.14159265358979323846f;

// The second Legendre polynomial, which is how the annual mean of the sun's
// energy is distributed over a rotating, tilted planet. Not a curve fitted to
// anything - it falls out of averaging the geometry over an orbit.
float legendreP2(float x) {
    return 0.5f * (3.0f * x * x - 1.0f);
}

} // namespace

// Deliberately reads nothing from the grid.
//
// This is a member of CrustGrid and is declared before the cells are, so when
// it runs the grid's own vectors have not been constructed yet. Asking one for
// its size here reads an object that does not exist, gets a garbage length,
// and tries to allocate it. Everything this needs is sized on first use
// instead, by which time the planet exists.
Climate::Climate(const CrustGrid& grid) : grid(grid) {}

float Climate::cloudFromCondensation(float condensing, float mean) const {
    if (mean <= 0.0f) {
        return 0.0f;
    }
    // Saturating rather than linear: a sky cannot be more than covered, so
    // twice the condensation of an already cloudy place adds very little.
    // Most of the planet sits well below the mean, which is what leaves the
    // subtropical deserts and the continental interiors clear.
    // The coefficient decides how cloudy an average place is. At 0.5, ground
    // condensing at the planetary mean is about a third covered and it takes
    // several times the mean to approach overcast - which leaves most of the
    // planet with visible sky. Higher values look plausible cell by cell and
    // produce a world under permanent total cloud, because the mean is itself
    // dragged up by the oceans, where condensation never stops.
    return 1.0f - std::exp(-0.5f * condensing / mean);
}

float Climate::saturationCapacity(float temperatureC) const {
    // Clausius-Clapeyron, as a doubling every ten degrees. The exact
    // exponential matters less than the fact that it is exponential: it is why
    // the tropics can rain torrentially and the poles, however saturated, are
    // deserts.
    const float doublings = (temperatureC - constants.freezingPoint) /
                            constants.saturationDoubling;
    return constants.saturationAtFreezing * std::pow(2.0f, doublings);
}

void Climate::solveTemperature() {
    const auto& cells = grid.getCells();
    const size_t n = cells.size();
    if (n == 0) {
        return;
    }

    // Energy balance, iterated because two of its terms depend on the answer.
    //
    // Absorbed sunlight must equal what the planet radiates away plus what the
    // winds and currents carry off:
    //
    //     S(lat) * (1 - albedo) = A + B*T + C*(T - Tmean)
    //
    // Albedo depends on T through ice, and Tmean is an average of every T, so
    // this is solved by going round a few times rather than in one pass. It
    // converges quickly because both feedbacks are weak compared to the
    // radiative term - except near a snowball transition, which is exactly
    // where a planet's climate really is bistable.
    const float quarterSolar = constants.solarConstant * 0.25f;

    for (int iteration = 0; iteration < 12; iteration++) {
        double meanSum = 0.0;
        for (size_t i = 0; i < n; i++) {
            meanSum += fields.temperature[i];
        }
        const float mean = static_cast<float>(meanSum / n);

        std::atomic<int> iceCellsAtomic{0};

        util::parallelFor(n, [&](size_t begin, size_t end) {
        int localIce = 0;
        for (size_t i = begin; i < end; i++) {
            const float sinLatitude = cells[i].position.y;

            // Annual mean insolation by latitude. The 0.482 is the standard
            // second-moment coefficient for a planet at Earth's obliquity.
            const float insolation =
                quarterSolar * (1.0f - 0.482f * legendreP2(sinLatitude));

            const float elevation = cells[i].elevation - grid.getSeaLevel();
            const bool ocean = elevation < 0.0f;

            // Ice where it is cold enough, which is what closes the feedback.
            const bool frozen = fields.temperature[i] < constants.freezingPoint;
            float albedo = ocean ? constants.oceanAlbedo : constants.landAlbedo;
            if (frozen) {
                albedo = constants.iceAlbedo;
                localIce++;
            }

            // Solve the balance for T at sea level.
            const float absorbed = insolation * (1.0f - albedo);
            const float seaLevelTemperature =
                (absorbed - constants.longwaveOffset +
                 constants.meridionalTransport * mean) /
                (constants.longwaveSlope + constants.meridionalTransport);

            // Then carry it up the mountain. Height is why the tropics have
            // glaciers, and it is what puts a snow line on a volcano at the
            // equator.
            const float height = std::max(elevation, 0.0f);
            fields.temperature[i] = seaLevelTemperature - constants.lapseRate * height;
        }
        iceCellsAtomic.fetch_add(localIce, std::memory_order_relaxed);
        });

        fields.iceFraction =
            static_cast<float>(iceCellsAtomic.load()) / static_cast<float>(n);
    }

    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += fields.temperature[i];
    }
    fields.meanTemperature = static_cast<float>(sum / n);
}

void Climate::solveWind() {
    const auto& cells = grid.getCells();
    const size_t n = cells.size();

    // The three-cell circulation, which is not a pattern anyone chose - it is
    // what a rotating sphere does when its equator is heated. Air rises at the
    // equator, comes down near thirty degrees, and the Coriolis force turns
    // each returning branch. That gives easterly trades in the tropics,
    // westerlies in the middle latitudes and easterlies again at the poles,
    // and it is why deserts sit where they do.
    util::parallelFor(n, [&](size_t begin, size_t end) {
    for (size_t i = begin; i < end; i++) {
        const glm::vec3 up = cells[i].position;
        const float sinLatitude = glm::clamp(up.y, -1.0f, 1.0f);
        const float latitude = std::asin(sinLatitude);
        const float degrees = latitude * 180.0f / PI;

        // East at this point: the direction of rotation, perpendicular to both
        // the axis and the local vertical. Degenerate at the poles, where
        // there is no east, so the wind there is left meridional.
        const glm::vec3 axis(0.0f, 1.0f, 0.0f);
        glm::vec3 east = glm::cross(axis, up);
        const float eastLength = glm::length(east);
        if (eastLength < 1e-5f) {
            fields.wind[i] = glm::vec3(0.0f);
            continue;
        }
        east /= eastLength;

        // North at this point, in the tangent plane.
        const glm::vec3 north = glm::cross(up, east);

        // Zonal component by band. Negative is westward.
        float zonal = 0.0f;
        const float absDegrees = std::abs(degrees);
        if (absDegrees < 30.0f) {
            zonal = -std::cos(absDegrees / 30.0f * (PI * 0.5f));      // trades
        } else if (absDegrees < 60.0f) {
            zonal = std::sin((absDegrees - 30.0f) / 30.0f * PI);      // westerlies
        } else {
            zonal = -std::sin((absDegrees - 60.0f) / 30.0f * PI);     // polar easterlies
        }

        // Meridional component: towards the equator in the tropics where the
        // trades converge, polewards in the middle latitudes. This is what
        // gathers moisture into the equatorial belt.
        float meridional = 0.0f;
        if (absDegrees < 30.0f) {
            meridional = -0.35f * (degrees / 30.0f);
        } else if (absDegrees < 60.0f) {
            meridional = 0.25f * (degrees > 0.0f ? 1.0f : -1.0f);
        }

        fields.wind[i] = glm::normalize(east * zonal + north * meridional);
    }
    });
}

void Climate::solvePrecipitation() {
    const auto& cells = grid.getCells();
    const size_t n = cells.size();
    if (n == 0) {
        return;
    }

    std::vector<float> moisture(n, 0.0f);
    std::vector<float> rain(n, 0.0f);
    std::vector<float> next(n, 0.0f);

    // Where water enters the air. Evaporation is limited by how much the air
    // can hold, so a warm ocean feeds far more moisture than a cold one - the
    // exponential in the saturation curve doing its work.
    const auto isOcean = [&](size_t i) {
        return cells[i].elevation - grid.getSeaLevel() < 0.0f;
    };

    for (size_t i = 0; i < n; i++) {
        if (isOcean(i)) {
            moisture[i] = saturationCapacity(fields.temperature[i]);
        }
    }

    // Carry the moisture downwind, raining as it goes.
    //
    // Each pass hands a cell's moisture to whichever neighbour lies furthest
    // along its wind, which is upwind differencing - crude, but it is the
    // right crudeness here: it never produces moisture that was not there and
    // it puts the rain shadow on the correct side of the range.
    // Which way the wind sends each cell's moisture does not change between
    // transport steps - the wind field is fixed and so is the terrain - so the
    // downwind neighbour is found once rather than forty-eight times. That
    // alone is most of the cost, and it is a pure per-cell question.
    std::vector<int> downwind(n, -1);
    util::parallelFor(n, [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; i++) {
            const glm::vec3 wind = fields.wind[i];
            if (glm::dot(wind, wind) < 1e-8f) {
                continue;
            }
            int target = -1;
            float best = 0.2f;   // ignore neighbours that are barely downwind
            for (int k = 0; k < grid.neighbourCount(static_cast<int>(i)); k++) {
                const int j = grid.neighbourAt(static_cast<int>(i), k);
                const glm::vec3 toward =
                    glm::normalize(cells[j].position - cells[i].position);
                const float alignment = glm::dot(toward, wind);
                if (alignment > best) {
                    best = alignment;
                    target = j;
                }
            }
            downwind[i] = target;
        }
    });

    // The transport itself stays on one thread. It is a scatter - several cells
    // hand their moisture to the same downwind neighbour - and each step
    // depends on the one before, so there is no range to split that does not
    // race. What was expensive about it has already been lifted out.
    for (int step = 0; step < constants.transportSteps; step++) {
        std::fill(next.begin(), next.end(), 0.0f);

        for (size_t i = 0; i < n; i++) {
            if (moisture[i] <= 0.0f) {
                continue;
            }

            const int target = downwind[i];
            if (target < 0) {
                next[i] += moisture[i];
                continue;
            }

            // How much falls on the way. Climbing forces ascent and rains out
            // hard; descending air warms and holds what it has, so the lee
            // side gets almost nothing. This is the whole point of the model.
            const float climb = std::max(0.0f, cells[target].elevation - cells[i].elevation);
            float fraction = constants.backgroundRainout + climb * constants.orographicRainout;

            // Air cannot hold more than saturation; anything above it falls
            // whether or not the ground rises. This is what makes the warm wet
            // tropics rain and the cold poles not.
            const float capacity = saturationCapacity(fields.temperature[target]);
            if (moisture[i] > capacity) {
                fraction = std::max(fraction, 1.0f - capacity / moisture[i]);
            }

            fraction = glm::clamp(fraction, 0.0f, 1.0f);

            // Credited to the cell being climbed into, not the one the air
            // is leaving. Orographic rain falls on the slope that forces the
            // ascent - that is what makes one flank of a range wet and the
            // other dry, and crediting it to the foot of the slope instead
            // smears the pattern back over ground the air had already crossed.
            const float falls = moisture[i] * fraction;
            rain[target] += falls;
            next[target] += moisture[i] - falls;
        }

        moisture.swap(next);

        // The ocean keeps evaporating; it is not a finite reservoir being
        // drained over the course of the calculation.
        for (size_t i = 0; i < n; i++) {
            if (isOcean(i)) {
                moisture[i] = std::max(moisture[i], saturationCapacity(fields.temperature[i]));
            }
        }
    }

    // Whatever is still airborne has to come down somewhere.
    for (size_t i = 0; i < n; i++) {
        rain[i] += moisture[i];
    }

    double total = 0.0;
    for (size_t i = 0; i < n; i++) {
        fields.precipitation[i] = rain[i];
        total += rain[i];
    }
    meanPrecipitation = static_cast<float>(total / n);

    // Cloud is the water on its way down, seen from above.
    fields.cloudCover.resize(n);
    for (size_t i = 0; i < n; i++) {
        fields.cloudCover[i] = cloudFromCondensation(rain[i], meanPrecipitation);
    }
}

void Climate::update() {
    // Sized here rather than at construction. The grid builds this as a member
    // before it has built its own cells, so at that point there is nothing to
    // size against - and a climate that thinks the planet has no cells writes
    // straight past the end of its own arrays the first time it is asked
    // about one.
    const size_t n = grid.getCells().size();
    if (fields.temperature.size() != n) {
        fields.temperature.assign(n, 15.0f);
        fields.precipitation.assign(n, 1.0f);
        fields.wind.assign(n, glm::vec3(0.0f));
        fields.cloudCover.assign(n, 0.0f);
    }
    if (n == 0) {
        return;
    }

    solveTemperature();
    solveWind();
    solvePrecipitation();
}

float Climate::relativePrecipitation(int cell) const {
    if (cell < 0 || cell >= static_cast<int>(fields.precipitation.size()) ||
        meanPrecipitation <= 0.0f) {
        return 1.0f;
    }

    // Relative to the planet's own mean, so the absolute scale stays with the
    // stream power coefficient where it was calibrated. Clamped because a
    // desert should erode slowly, not never - wind, frost and the occasional
    // flood still work on it.
    return glm::clamp(fields.precipitation[cell] / meanPrecipitation, 0.05f, 6.0f);
}

} // namespace simulation
