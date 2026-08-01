#pragma once

#include <functional>
#include <string>

#include <glm/glm.hpp>

namespace octree { class OctreePlanet; }
namespace core { class Camera; }

namespace tools {

// What the commands are allowed to touch.
//
// Passed in rather than reached for, so that the command set does not quietly
// become a second copy of the application. Anything a command needs that is not
// here has to be added deliberately, which is the point - it keeps the surface
// visible and keeps this module compilable without the renderer.
struct ControlContext {
    octree::OctreePlanet* planet = nullptr;
    core::Camera* camera = nullptr;

    // Supplied by the application because they belong to the renderer and the
    // UI, which this module has no business knowing about.
    std::function<bool(const std::string& path)> screenshot;
    std::function<void(bool visible)> setPanelsVisible;
    std::function<bool()> panelsVisible;

    // Sun direction override for looking at things, in world space. Purely a
    // rendering matter: insolation in the climate model comes from latitude,
    // so moving the light to see the night side cannot disturb the simulation
    // being examined. If that ever stops being true, this is where it breaks.
    std::function<void(bool enabled, const glm::vec3& direction)> setSunOverride;

    // Clouds hide the ground, which is fine when watching and useless when
    // inspecting. The panel has had a checkbox for this all along; the point of
    // putting it here is that hiding the panels should not take the controls
    // with it.
    std::function<void(bool visible)> setCloudsVisible;
    std::function<bool()> cloudsVisible;

    // True while the light is being held at a fixed angle to the camera rather
    // than to the planet.
    bool sunFollowsCamera = false;
    float sunAzimuth = -35.0f;
    float sunElevation = 25.0f;
};

// Turns one line of text into one line of reply.
//
// Every command answers, and answers on a single line, because the client is
// as likely to be a shell loop as a program. Failures answer too - a command
// that silently does nothing is indistinguishable from a lost connection.
class ControlCommands {
public:
    explicit ControlCommands(ControlContext& context) : ctx(context) {}

    // Returns the reply. If the command started something that finishes later,
    // sets deferred and the caller answers when it completes.
    std::string dispatch(const std::string& line, bool& deferred);

    // The reply owed once a deferred command finishes.
    std::string completionReply() const;

private:
    ControlContext& ctx;

    std::string help() const;
    std::string status() const;
    std::string probe(double latitude, double longitude) const;
    std::string stats() const;
    std::string find(const std::string& what);
    std::string applySun();
};

// Where a point on the planet is, in the terms a person uses. Kept here because
// every command that names a place uses them and they should agree.
glm::dvec3 directionFromLatLon(double latitudeDegrees, double longitudeDegrees);
void latLonFromDirection(const glm::dvec3& direction, double& latitudeDegrees,
                         double& longitudeDegrees);

} // namespace tools
