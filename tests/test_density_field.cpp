// Tests for core::DensityField - the single source of truth for planet shape.
//
// These cover the properties that were actually broken before: noise that was
// not really simplex noise, and lat/long terrain that pinched at the poles and
// seamed at longitude +/-pi.

#include "core/density_field.hpp"

#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>

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

// Evenly distributed directions on the sphere (Fibonacci lattice), so tests
// sample the poles as densely as the equator.
std::vector<glm::vec3> sphereSamples(int count) {
    std::vector<glm::vec3> pts;
    pts.reserve(count);
    const float golden = 3.14159265358979f * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < count; i++) {
        const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float theta = golden * static_cast<float>(i);
        pts.emplace_back(std::cos(theta) * r, y, std::sin(theta) * r);
    }
    return pts;
}

void testHeightIsFiniteEverywhere() {
    std::printf("Height field is finite over the whole sphere\n");
    core::DensityField field(1000000.0f, 42);

    bool allFinite = true;
    for (const glm::vec3& n : sphereSamples(20000)) {
        const float h = field.getTerrainHeight(n);
        if (!std::isfinite(h)) {
            allFinite = false;
            break;
        }
    }
    check(allFinite, "no NaN or infinity in terrain height");

    // Poles are ordinary points for 3D noise; a lat/long field degenerates here.
    check(std::isfinite(field.getTerrainHeight(glm::vec3(0, 1, 0))), "north pole is finite");
    check(std::isfinite(field.getTerrainHeight(glm::vec3(0, -1, 0))), "south pole is finite");
}

void testHeightWithinStatedRelief() {
    std::printf("Height stays within the advertised relief bound\n");
    core::DensityField field(1000000.0f, 7);
    const float relief = field.getMaxRelief();

    float lo = 1e30f, hi = -1e30f;
    for (const glm::vec3& n : sphereSamples(20000)) {
        const float h = field.getTerrainHeight(n);
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    std::printf("  observed range [%.1f, %.1f] m, bound +/-%.1f m\n", lo, hi, relief);
    check(lo >= -relief && hi <= relief, "height is inside getMaxRelief()");
    check(hi > lo, "height actually varies");
}

// A lat/long parameterisation crossing longitude +/-pi produces a visible
// seam. Walking a ring of longitudes should show no step larger than the
// typical neighbouring-sample difference.
void testNoSeamAtDateline() {
    std::printf("No discontinuity where longitude wraps\n");
    core::DensityField field(1000000.0f, 42);

    const int steps = 4000;
    const float lat = 0.35f;  // arbitrary off-equator ring
    std::vector<float> heights(steps);
    for (int i = 0; i < steps; i++) {
        const float lon = -3.14159265f + 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(steps);
        const glm::vec3 n(std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon));
        heights[i] = field.getTerrainHeight(n);
    }

    float maxStep = 0.0f;
    double totalStep = 0.0;
    for (int i = 0; i < steps; i++) {
        const float d = std::fabs(heights[(i + 1) % steps] - heights[i]);
        maxStep = std::max(maxStep, d);
        totalStep += d;
    }
    const float meanStep = static_cast<float>(totalStep / steps);

    // A seam shows up as one step orders of magnitude above the mean.
    std::printf("  mean step %.3f m, max step %.3f m\n", meanStep, maxStep);
    check(maxStep < meanStep * 40.0f + 1.0f, "no seam-sized jump around the ring");
}

void testDeterministicForSeed() {
    std::printf("Same seed gives the same planet\n");
    core::DensityField a(1000000.0f, 1234);
    core::DensityField b(1000000.0f, 1234);
    core::DensityField c(1000000.0f, 5678);

    bool identical = true;
    bool differs = false;
    for (const glm::vec3& n : sphereSamples(2000)) {
        if (a.getTerrainHeight(n) != b.getTerrainHeight(n)) identical = false;
        if (a.getTerrainHeight(n) != c.getTerrainHeight(n)) differs = true;
    }
    check(identical, "identical seeds match exactly");
    check(differs, "different seeds produce different terrain");
}

void testContinuity() {
    std::printf("Height field is continuous\n");
    core::DensityField field(1000000.0f, 42);

    // Nearby directions must give nearby heights - noise with a broken corner
    // selection shows up as cliffs between adjacent samples.
    const float relief = field.getMaxRelief();
    float worst = 0.0f;
    for (const glm::vec3& n : sphereSamples(3000)) {
        const glm::vec3 nearby = glm::normalize(n + glm::vec3(1e-4f, 0.0f, 0.0f));
        worst = std::max(worst, std::fabs(field.getTerrainHeight(nearby) - field.getTerrainHeight(n)));
    }
    std::printf("  worst neighbour delta %.4f m over relief %.1f m\n", worst, relief);
    check(worst < relief * 0.02f, "no cliffs between adjacent samples");
}

void testDensitySignAgreesWithHeight() {
    std::printf("Signed distance agrees with the height field\n");
    core::DensityField field(1000000.0f, 99);
    const float R = field.getPlanetRadius();

    bool consistent = true;
    for (const glm::vec3& n : sphereSamples(2000)) {
        const float h = field.getTerrainHeight(n);
        // Just inside the surface must be negative, just outside positive.
        if (field.getDensity(n * (R + h - 50.0f)) >= 0.0f) consistent = false;
        if (field.getDensity(n * (R + h + 50.0f)) <= 0.0f) consistent = false;
    }
    check(consistent, "density is negative inside and positive outside");

    // Far outside the terrain shell the cheap early-out still has to be signed
    // correctly.
    check(field.getDensity(glm::vec3(0, R * 4.0f, 0)) > 0.0f, "deep space is outside");
    check(field.getDensity(glm::vec3(0, R * 0.1f, 0)) < 0.0f, "planet interior is inside");
}

void testScalesWithRadius() {
    std::printf("Terrain keeps its proportions at any planet radius\n");
    // Frequencies are cycles per radius, so a small and a large planet with the
    // same seed should have the same shape, just scaled.
    core::DensityField small(1000.0f, 3);
    core::DensityField large(1000000.0f, 3);

    bool proportional = true;
    for (const glm::vec3& n : sphereSamples(500)) {
        const float hs = small.getTerrainHeight(n) / small.getPlanetRadius();
        const float hl = large.getTerrainHeight(n) / large.getPlanetRadius();
        if (std::fabs(hs - hl) > 1e-5f) {
            proportional = false;
            break;
        }
    }
    check(proportional, "relative relief is radius-independent");
}

// Hypsometry: how the surface divides between ocean, lowland, highland and
// snow. This is the check that catches a planet which is technically correct
// but looks wrong - an all-mountain world, or one with no coastline.
void testHypsometry() {
    std::printf("Surface divides into plausible proportions\n");
    core::DensityField field(1000000.0f, 42);

    const float seaLevel = field.getSeaLevelHeight();
    const float maxElevation = field.getMaxElevation();

    int ocean = 0, beach = 0, lowland = 0, highland = 0, snow = 0;
    const auto samples = sphereSamples(40000);
    for (const glm::vec3& n : samples) {
        const float elevation = field.getTerrainHeight(n) - seaLevel;
        if (elevation < 0.0f) {
            ++ocean;
            continue;
        }
        if (elevation > field.getSnowLineElevation(n)) {
            ++snow;
        } else {
            const float e = elevation / maxElevation;
            if (e < 0.012f)     ++beach;
            else if (e < 0.10f) ++lowland;
            else                ++highland;
        }
    }

    const float total = static_cast<float>(samples.size());
    const float oceanPct = 100.0f * ocean / total;
    const float landPct = 100.0f - oceanPct;
    const int landCount = static_cast<int>(samples.size()) - ocean;
    const float snowOfLand = landCount > 0 ? 100.0f * snow / landCount : 0.0f;

    std::printf("  ocean %.1f%%  beach %.1f%%  lowland %.1f%%  highland %.1f%%  snow %.1f%%\n",
                oceanPct,
                100.0f * beach / total,
                100.0f * lowland / total,
                100.0f * highland / total,
                100.0f * snow / total);
    std::printf("  snow is %.1f%% of land\n", snowOfLand);

    // Earth is ~71%% ocean. Anything from a third to nearly all water is a
    // plausible planet; no ocean at all, or no land at all, is not.
    check(oceanPct > 30.0f && oceanPct < 90.0f, "ocean covers a plausible fraction");
    check(landPct > 10.0f, "there is a meaningful amount of land");

    // Mountains should be the exception. An all-white planet means the ridge
    // signal is saturating, which is easy to do with ridged noise.
    check(snowOfLand < 30.0f, "snow and ice are a minority of land");

    // And there should be some, otherwise the mountain term is doing nothing.
    check(snowOfLand > 0.5f, "some land does reach the snow line");
}

} // namespace

int main() {
    std::printf("=== DensityField tests ===\n\n");

    testHeightIsFiniteEverywhere();
    testHeightWithinStatedRelief();
    testNoSeamAtDateline();
    testDeterministicForSeed();
    testContinuity();
    testDensitySignAgreesWithHeight();
    testScalesWithRadius();
    testHypsometry();

    std::printf("\n");
    if (failures == 0) {
        std::printf("All DensityField tests passed.\n");
        return 0;
    }
    std::printf("%d DensityField check(s) failed.\n", failures);
    return 1;
}
