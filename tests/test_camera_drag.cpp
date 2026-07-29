// Dragging the planet.
//
// The claim the drag makes is exact and therefore testable: whichever point of
// the surface was under the cursor when the button went down is under the
// cursor still, however far it has been dragged, at any zoom and any latitude.
// That is one assertion - pick a point, drag, project it back, see whether it
// lands on the pixel the cursor is on - and it catches everything that used to
// be wrong here. An inverted axis fails it. A sensitivity in radians per pixel
// fails it as soon as the zoom changes. Rotating about the world's vertical
// instead of the screen's fails it away from the equator.

#include "core/camera.hpp"

#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::printf("  FAIL: %s\n", what.c_str());
        failures++;
    } else {
        std::printf("  ok: %s\n", what.c_str());
    }
}

constexpr float PLANET_RADIUS = 1000000.0f;
constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;

// Radius of the planet's disc on screen, in pixels. Drags have to stay inside
// it to be testable: off the disc there is no intersection to anchor to, and
// the silhouette fallback keeps the gesture alive rather than tracking a point
// that is not there. Looking straight down from low altitude the disc is only
// tens of pixels across, so this cannot be a fixed number.
float screenDiscRadius(const core::Camera& camera) {
    const float distance = glm::length(camera.getPosition());
    if (distance <= PLANET_RADIUS) {
        return static_cast<float>(HEIGHT);
    }
    const float angular = std::asin(PLANET_RADIUS / distance);
    const float tanHalfFov = std::tan(glm::radians(camera.getFieldOfView()) * 0.5f);
    return (HEIGHT * 0.5f) * std::tan(angular) / tanHalfFov;
}

core::Camera makeCamera(float altitude) {
    core::Camera camera(WIDTH, HEIGHT);
    camera.setViewport(WIDTH, HEIGHT);
    camera.setMode(core::CameraMode::Orbital);
    camera.setPosition(glm::vec3(0.0f, 0.0f, PLANET_RADIUS + altitude));
    camera.lookAt(glm::vec3(0.0f));
    camera.update(0.0f);
    return camera;
}

// Where a direction from the planet centre lands on screen, in pixels.
bool projectToPixel(const core::Camera& camera, const glm::vec3& direction,
                    glm::vec2& outPixel) {
    const glm::vec3 worldPos = direction * PLANET_RADIUS;
    const glm::vec4 clip =
        camera.getProjectionMatrix(static_cast<float>(WIDTH) / HEIGHT) *
        camera.getViewMatrix() * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.0f) {
        return false;
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    // The projection carries Vulkan's Y flip, so NDC +Y is already down the
    // screen and maps straight to pixel rows.
    outPixel.x = (ndc.x * 0.5f + 0.5f) * WIDTH;
    outPixel.y = (ndc.y * 0.5f + 0.5f) * HEIGHT;
    return true;
}

// One drag, from press to release, reporting how far the grabbed point ended
// up from the cursor in pixels.
float dragError(core::Camera& camera, const glm::vec2& from, const glm::vec2& to,
                int steps = 12) {
    glm::vec3 anchor;
    if (!camera.pickSphere(from, PLANET_RADIUS, anchor)) {
        return -1.0f;
    }

    for (int i = 1; i <= steps; i++) {
        const float t = static_cast<float>(i) / steps;
        const glm::vec2 cursor = from + (to - from) * t;

        glm::vec3 under;
        if (!camera.pickSphere(cursor, PLANET_RADIUS, under)) {
            return -1.0f;
        }
        camera.dragSurface(under, anchor);
        camera.update(0.0f);
    }

    glm::vec2 landed;
    if (!projectToPixel(camera, anchor, landed)) {
        return -1.0f;
    }
    return glm::length(landed - to);
}

void testGroundFollowsCursor() {
    std::printf("Ground stays under the cursor\n");

    // Tolerance is generous in pixels but tiny next to the drag itself. A
    // reversed axis or a mis-scaled sensitivity misses by hundreds.
    constexpr float TOLERANCE = 2.0f;

    const float altitudes[] = {2000000.0f, 500000.0f, 50000.0f, 10000.0f};
    for (float altitude : altitudes) {
        core::Camera camera = makeCamera(altitude);

        // Half the disc, so the drag is a big gesture at every altitude
        // without running off the edge.
        const float reach = screenDiscRadius(camera) * 0.5f;
        const glm::vec2 centre(640.0f, 360.0f);
        const float error = dragError(camera, centre,
                                      centre + glm::vec2(reach * 0.8f, -reach * 0.4f));
        check(error >= 0.0f && error < TOLERANCE,
              "altitude " + std::to_string(static_cast<int>(altitude / 1000.0f)) +
                  " km: grabbed point lands " + std::to_string(error) + " px from cursor");
    }
}

void testDragDirection() {
    std::printf("Surface moves the way the mouse moves\n");

    // The specific failure that prompted this: dragging down moved the
    // surface up. Checked as a direction rather than an angle, because that
    // is what a hand on a globe expects.
    struct Case {
        const char* name;
        glm::vec2 to;
    };
    const Case cases[] = {
        {"drag right", glm::vec2(840.0f, 360.0f)},
        {"drag left", glm::vec2(440.0f, 360.0f)},
        {"drag down", glm::vec2(640.0f, 560.0f)},
        {"drag up", glm::vec2(640.0f, 160.0f)},
    };

    for (const Case& c : cases) {
        core::Camera camera = makeCamera(1500000.0f);
        const glm::vec2 from(640.0f, 360.0f);

        glm::vec3 anchor;
        camera.pickSphere(from, PLANET_RADIUS, anchor);

        glm::vec2 before;
        projectToPixel(camera, anchor, before);

        dragError(camera, from, c.to);

        glm::vec2 after;
        projectToPixel(camera, anchor, after);

        const glm::vec2 surfaceMoved = after - before;
        const glm::vec2 cursorMoved = c.to - from;
        const float alignment = glm::dot(glm::normalize(surfaceMoved),
                                         glm::normalize(cursorMoved));

        check(alignment > 0.99f,
              std::string(c.name) + ": surface follows cursor (alignment " +
                  std::to_string(alignment) + ")");
    }
}

void testAwayFromEquator() {
    std::printf("Behaviour does not depend on latitude\n");

    // Angle-based orbiting degrades towards the poles - the same drag turns
    // the planet further and further until it spins in place. Anchoring to
    // the surface should not care.
    core::Camera camera = makeCamera(1500000.0f);

    // Move most of the way to the pole first.
    camera.setPosition(glm::vec3(0.0f, 2300000.0f, 600000.0f));
    camera.lookAt(glm::vec3(0.0f));
    camera.update(0.0f);

    const float reach = screenDiscRadius(camera) * 0.5f;
    const glm::vec2 centre(640.0f, 360.0f);
    const float error =
        dragError(camera, centre, centre + glm::vec2(reach * 0.7f, reach * 0.3f));
    check(error >= 0.0f && error < 2.0f,
          "near the pole: grabbed point lands " + std::to_string(error) + " px from cursor");
}

void testOffTheLimb() {
    std::printf("Dragging past the edge of the planet\n");

    // The cursor leaving the disc must not stall or snap the drag; the pick
    // falls back to the silhouette, so the rotation stays continuous.
    core::Camera camera = makeCamera(1500000.0f);

    glm::vec3 direction;
    const bool picked = camera.pickSphere(glm::vec2(20.0f, 20.0f), PLANET_RADIUS, direction);
    check(picked, "a pick well off the planet still returns a direction");
    check(std::abs(glm::length(direction) - 1.0f) < 1e-4f,
          "and it is a unit direction");

    const float error = dragError(camera, glm::vec2(640.0f, 360.0f),
                                  glm::vec2(1240.0f, 80.0f));
    check(error >= 0.0f, "a drag that runs off the disc completes");
}

} // namespace

int main() {
    std::printf("=== Camera drag ===\n\n");

    testGroundFollowsCursor();
    std::printf("\n");
    testDragDirection();
    std::printf("\n");
    testAwayFromEquator();
    std::printf("\n");
    testOffTheLimb();

    std::printf("\n%s\n", failures == 0 ? "All passed" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
