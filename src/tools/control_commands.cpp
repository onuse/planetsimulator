#include "tools/control_commands.hpp"

#include "core/camera.hpp"
#include "core/octree.hpp"
#include "simulation/crust_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <vector>

namespace tools {

namespace {

constexpr double DEG = 3.14159265358979323846 / 180.0;

std::vector<std::string> tokenise(const std::string& line) {
    std::vector<std::string> parts;
    std::istringstream stream(line);
    std::string word;
    while (stream >> word) {
        parts.push_back(word);
    }
    return parts;
}

double number(const std::string& text, bool& ok) {
    try {
        size_t used = 0;
        const double value = std::stod(text, &used);
        ok = used == text.size();
        return value;
    } catch (...) {
        ok = false;
        return 0.0;
    }
}

std::string fail(const std::string& why) {
    return "{\"ok\":false,\"error\":\"" + why + "\"}";
}

std::string format(const char* pattern, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, pattern);
    std::vsnprintf(buffer, sizeof(buffer), pattern, args);
    va_end(args);
    return std::string(buffer);
}

} // namespace

glm::dvec3 directionFromLatLon(double latitudeDegrees, double longitudeDegrees) {
    const double lat = latitudeDegrees * DEG;
    const double lon = longitudeDegrees * DEG;
    return glm::dvec3(std::cos(lat) * std::cos(lon), std::sin(lat),
                      std::cos(lat) * std::sin(lon));
}

void latLonFromDirection(const glm::dvec3& direction, double& latitudeDegrees,
                         double& longitudeDegrees) {
    const glm::dvec3 n = glm::normalize(direction);
    latitudeDegrees = std::asin(glm::clamp(n.y, -1.0, 1.0)) / DEG;
    longitudeDegrees = std::atan2(n.z, n.x) / DEG;
}

std::string ControlCommands::help() const {
    return "{\"ok\":true,\"commands\":["
           "\"help\",\"status\",\"stats\",\"quit\","
           "\"screenshot <path>\","
           "\"panels on|off\",\"clouds on|off\","
           "\"camera goto <lat> <lon> <altitude_km>\",\"camera where\","
           "\"sun world <az> <el>|camera <az> <el>|off\","
           "\"sim pause|resume|rate <kyr_per_s>|advance <My>\","
           "\"probe <lat> <lon>\","
           "\"find deepest-valley|biggest-river|river-mouth|highest|land\""
           "]}";
}

std::string ControlCommands::status() const {
    const auto* planet = ctx.planet;
    if (planet == nullptr) {
        return fail("no planet");
    }
    const auto* crust = planet->getCrustGrid();

    double lat = 0.0, lon = 0.0;
    float altitude = 0.0f;
    if (ctx.camera != nullptr) {
        latLonFromDirection(glm::dvec3(ctx.camera->getPosition()), lat, lon);
        altitude = ctx.camera->getAltitude(glm::vec3(0.0f), planet->getRadius());
    }

    return format(
        "{\"ok\":true,\"simulationTime\":%.4f,\"rate\":%.5f,\"achieved\":%.5f,"
        "\"advancing\":%s,\"outstanding\":%.4f,"
        "\"latitude\":%.3f,\"longitude\":%.3f,\"altitudeKm\":%.2f,"
        "\"panels\":%s,\"sunFollowsCamera\":%s}",
        crust != nullptr ? crust->getSimulationTime() : 0.0f,
        planet->getSimulationRate(), planet->getAchievedSimulationRate(),
        planet->advanceComplete() ? "false" : "true", planet->advanceOutstanding(),
        lat, lon, altitude / 1000.0f,
        (ctx.panelsVisible && ctx.panelsVisible()) ? "true" : "false",
        ctx.sunFollowsCamera ? "true" : "false");
}

std::string ControlCommands::probe(double latitude, double longitude) const {
    const auto* planet = ctx.planet;
    if (planet == nullptr) {
        return fail("no planet");
    }
    const auto* crust = planet->getCrustGrid();
    auto snapshot = planet->getRenderSnapshot();
    if (crust == nullptr || !snapshot) {
        return fail("no simulation");
    }

    const glm::vec3 dir = glm::vec3(directionFromLatLon(latitude, longitude));
    const int cell = crust->findNearestCell(dir);
    if (cell < 0 || cell >= static_cast<int>(snapshot->elevation.size())) {
        return fail("no cell there");
    }

    // Everything comes from the published snapshot rather than from the live
    // grid. The simulation is running on another thread and owns its own state;
    // the snapshot is the copy it has finished with, and reading anything else
    // from here would be a race that shows up as impossible numbers once in a
    // few thousand probes.
    const auto& s = *snapshot;
    const auto river = crust->sampleRiverGeometry(s, dir);
    const int into = cell < static_cast<int>(s.flowsInto.size()) ? s.flowsInto[cell] : -1;

    double downLat = 0.0, downLon = 0.0;
    if (into >= 0 && into < static_cast<int>(crust->getCells().size())) {
        latLonFromDirection(glm::dvec3(crust->getCells()[into].position), downLat, downLon);
    }

    return format(
        "{\"ok\":true,\"cell\":%d,\"latitude\":%.3f,\"longitude\":%.3f,"
        "\"elevation\":%.1f,\"discharge\":%.2f,\"channelDepth\":%.2f,"
        "\"lakeDepth\":%.2f,\"cloudCover\":%.3f,"
        "\"flowsInto\":%d,\"downstreamLat\":%.3f,\"downstreamLon\":%.3f,"
        "\"riverWidth\":%.1f,\"riverDistance\":%.1f,\"riverDepth\":%.1f,"
        "\"simulationTime\":%.4f}",
        cell, latitude, longitude,
        s.elevation[cell],
        cell < static_cast<int>(s.discharge.size()) ? s.discharge[cell] : 0.0f,
        cell < static_cast<int>(s.channelDepth.size()) ? s.channelDepth[cell] : 0.0f,
        cell < static_cast<int>(s.lakeDepth.size()) ? s.lakeDepth[cell] : 0.0f,
        cell < static_cast<int>(s.cloudCover.size()) ? s.cloudCover[cell] : 0.0f,
        into, downLat, downLon,
        river.width, river.distance, river.depth,
        crust->getSimulationTime());
}

std::string ControlCommands::stats() const {
    const auto* planet = ctx.planet;
    if (planet == nullptr) {
        return fail("no planet");
    }
    auto snapshot = planet->getRenderSnapshot();
    if (!snapshot) {
        return fail("no simulation");
    }

    // Aggregates rather than a time series. Anything that needs to be watched
    // over time is this, called repeatedly with an explicit advance between -
    // which keeps the protocol small and puts the sampling schedule in the
    // hands of whoever is asking the question.
    const auto& s = *snapshot;
    const int n = static_cast<int>(s.elevation.size());

    int land = 0, channels = 0, draining = 0, lakes = 0;
    double depthSum = 0.0;
    float deepest = 0.0f, biggestRiver = 0.0f;

    for (int i = 0; i < n; i++) {
        if (s.elevation[i] > 0.0f) {
            land++;
        }
        if (i < static_cast<int>(s.channelDepth.size()) && s.channelDepth[i] > 0.0f) {
            channels++;
            depthSum += s.channelDepth[i];
            deepest = std::max(deepest, s.channelDepth[i]);
        }
        if (i < static_cast<int>(s.flowsInto.size()) && s.flowsInto[i] >= 0) {
            draining++;
        }
        if (i < static_cast<int>(s.lakeDepth.size()) && s.lakeDepth[i] > 0.0f) {
            lakes++;
        }
        if (i < static_cast<int>(s.discharge.size())) {
            biggestRiver = std::max(biggestRiver, s.discharge[i]);
        }
    }

    return format(
        "{\"ok\":true,\"simulationTime\":%.4f,\"version\":%llu,\"cells\":%d,"
        "\"land\":%d,\"channels\":%d,\"draining\":%d,\"lakes\":%d,"
        "\"meanChannelDepth\":%.2f,\"deepestChannel\":%.1f,\"biggestCatchment\":%.1f,"
        "\"seaLevel\":%.1f,\"maxElevation\":%.1f}",
        s.simulationTime, static_cast<unsigned long long>(s.version), n,
        land, channels, draining, lakes,
        channels > 0 ? depthSum / channels : 0.0, deepest, biggestRiver,
        s.seaLevel, s.maxElevation);
}

std::string ControlCommands::find(const std::string& what) {
    const auto* planet = ctx.planet;
    if (planet == nullptr) {
        return fail("no planet");
    }
    const auto* crust = planet->getCrustGrid();
    auto snapshot = planet->getRenderSnapshot();
    if (crust == nullptr || !snapshot) {
        return fail("no simulation");
    }

    // Point me at the interesting thing.
    //
    // This is here because the alternative is flying a camera around hoping to
    // stumble across the feature in question, which wasted more time than any
    // actual bug did - an automatic zoom that oscillates and parks over open
    // ocean is not a way to inspect a river. The simulation already knows where
    // its deepest valley is; asking it is both faster and repeatable.
    const auto& s = *snapshot;
    const auto& cells = crust->getCells();
    const int n = std::min(static_cast<int>(s.elevation.size()),
                           static_cast<int>(cells.size()));

    int best = -1;
    float bestScore = -1e30f;

    for (int i = 0; i < n; i++) {
        const bool isLand = s.elevation[i] > 0.0f;
        float score = -1e30f;

        if (what == "deepest-valley") {
            if (i < static_cast<int>(s.channelDepth.size())) {
                score = s.channelDepth[i];
            }
        } else if (what == "biggest-river") {
            if (isLand && i < static_cast<int>(s.discharge.size())) {
                score = s.discharge[i];
            }
        } else if (what == "river-mouth") {
            // A land cell that drains straight into the sea, carrying the most.
            if (isLand && i < static_cast<int>(s.flowsInto.size())) {
                const int into = s.flowsInto[i];
                if (into >= 0 && into < static_cast<int>(s.elevation.size()) &&
                    s.elevation[into] <= 0.0f) {
                    score = s.discharge[i];
                }
            }
        } else if (what == "highest") {
            score = s.elevation[i];
        } else if (what == "land") {
            score = isLand ? s.elevation[i] : -1e30f;
        } else {
            return fail("unknown target: " + what);
        }

        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }

    if (best < 0 || bestScore <= -1e29f) {
        return fail("nothing matching " + what);
    }

    double lat = 0.0, lon = 0.0;
    latLonFromDirection(glm::dvec3(cells[best].position), lat, lon);
    return format(
        "{\"ok\":true,\"target\":\"%s\",\"cell\":%d,\"latitude\":%.3f,"
        "\"longitude\":%.3f,\"value\":%.2f,\"elevation\":%.1f}",
        what.c_str(), best, lat, lon, bestScore, s.elevation[best]);
}

std::string ControlCommands::applySun() {
    if (!ctx.setSunOverride) {
        return fail("no sun control");
    }

    // Azimuth and elevation are given in the frame the light is locked to -
    // relative to the camera when it follows, relative to the planet otherwise.
    // The camera-relative case is what makes it possible to look at the night
    // side at all: the light keeps a fixed angle to the view, so rotating the
    // planet moves the terminator across it instead of moving the lit half out
    // of sight.
    const float az = ctx.sunAzimuth * static_cast<float>(DEG);
    const float el = ctx.sunElevation * static_cast<float>(DEG);
    const glm::vec3 local(std::sin(az) * std::cos(el), std::sin(el),
                          -std::cos(az) * std::cos(el));

    glm::vec3 direction = local;
    if (ctx.sunFollowsCamera && ctx.camera != nullptr) {
        const glm::vec3 f = ctx.camera->getForward();
        const glm::vec3 r = ctx.camera->getRight();
        const glm::vec3 u = ctx.camera->getUp();
        direction = glm::normalize(r * local.x + u * local.y + f * (-local.z));
    }

    ctx.setSunOverride(true, glm::normalize(-direction));
    return format("{\"ok\":true,\"sun\":\"%s\",\"azimuth\":%.1f,\"elevation\":%.1f}",
                  ctx.sunFollowsCamera ? "camera" : "world", ctx.sunAzimuth,
                  ctx.sunElevation);
}

std::string ControlCommands::completionReply() const {
    return status();
}

std::string ControlCommands::dispatch(const std::string& line, bool& deferred) {
    deferred = false;
    const auto parts = tokenise(line);
    if (parts.empty()) {
        return fail("empty command");
    }
    const std::string& verb = parts[0];

    if (verb == "help") {
        return help();
    }
    if (verb == "status") {
        return status();
    }
    if (verb == "stats") {
        return stats();
    }

    if (verb == "screenshot") {
        if (parts.size() < 2) {
            return fail("screenshot needs a path");
        }
        if (!ctx.screenshot) {
            return fail("no screenshot support");
        }
        // Rebuilt from the tail of the line so that a path with spaces works.
        const size_t at = line.find(parts[1]);
        const std::string path = line.substr(at);
        return ctx.screenshot(path) ? format("{\"ok\":true,\"path\":\"%s\"}", path.c_str())
                                    : fail("screenshot failed");
    }

    if (verb == "panels") {
        if (parts.size() < 2 || !ctx.setPanelsVisible) {
            return fail("panels needs on or off");
        }
        const bool on = parts[1] == "on";
        ctx.setPanelsVisible(on);
        return format("{\"ok\":true,\"panels\":%s}", on ? "true" : "false");
    }

    if (verb == "clouds") {
        if (parts.size() < 2 || !ctx.setCloudsVisible) {
            return fail("clouds needs on or off");
        }
        const bool on = parts[1] == "on";
        ctx.setCloudsVisible(on);
        return format("{\"ok\":true,\"clouds\":%s}", on ? "true" : "false");
    }

    if (verb == "camera") {
        if (ctx.camera == nullptr || ctx.planet == nullptr) {
            return fail("no camera");
        }
        if (parts.size() >= 2 && parts[1] == "where") {
            return status();
        }
        if (parts.size() >= 5 && parts[1] == "goto") {
            bool a = false, b = false, c = false;
            const double lat = number(parts[2], a);
            const double lon = number(parts[3], b);
            const double km = number(parts[4], c);
            if (!a || !b || !c) {
                return fail("camera goto wants three numbers");
            }
            const glm::vec3 dir = glm::vec3(directionFromLatLon(lat, lon));
            ctx.camera->viewFrom(dir, ctx.planet->getRadius(),
                                 static_cast<float>(km * 1000.0));
            if (ctx.sunFollowsCamera) {
                applySun();
            }
            return status();
        }
        return fail("camera goto <lat> <lon> <km>, or camera where");
    }

    if (verb == "sun") {
        if (parts.size() < 2) {
            return fail("sun world|camera|off");
        }
        if (parts[1] == "off") {
            if (ctx.setSunOverride) {
                ctx.setSunOverride(false, glm::vec3(0.0f));
            }
            return "{\"ok\":true,\"sun\":\"default\"}";
        }
        if (parts[1] != "world" && parts[1] != "camera") {
            return fail("sun world|camera|off");
        }
        ctx.sunFollowsCamera = parts[1] == "camera";
        if (parts.size() >= 4) {
            bool a = false, b = false;
            const double az = number(parts[2], a);
            const double el = number(parts[3], b);
            if (!a || !b) {
                return fail("sun wants azimuth and elevation in degrees");
            }
            ctx.sunAzimuth = static_cast<float>(az);
            ctx.sunElevation = static_cast<float>(el);
        }
        return applySun();
    }

    if (verb == "sim") {
        if (ctx.planet == nullptr) {
            return fail("no planet");
        }
        if (parts.size() < 2) {
            return fail("sim pause|resume|rate <kyr>|advance <My>");
        }
        if (parts[1] == "pause") {
            ctx.planet->setSimulationRate(0.0f);
            return status();
        }
        if (parts[1] == "resume") {
            ctx.planet->setSimulationRate(1.0f);
            return status();
        }
        if (parts[1] == "rate" && parts.size() >= 3) {
            bool ok = false;
            const double kyr = number(parts[2], ok);
            if (!ok || kyr < 0.0) {
                return fail("rate wants thousand years per second");
            }
            ctx.planet->setSimulationRate(static_cast<float>(kyr * 0.001));
            return status();
        }
        if (parts[1] == "advance" && parts.size() >= 3) {
            bool ok = false;
            const double my = number(parts[2], ok);
            if (!ok || my <= 0.0) {
                return fail("advance wants million years");
            }
            ctx.planet->requestAdvance(static_cast<float>(my));
            // Answered when it has actually happened, so that a caller which
            // waits for the reply is guaranteed the geology is done. Rendering
            // continues meanwhile, so the window stays alive and can be
            // photographed part way through a long run.
            deferred = true;
            return std::string();
        }
        return fail("sim pause|resume|rate <kyr>|advance <My>");
    }

    if (verb == "probe") {
        if (parts.size() < 3) {
            return fail("probe <lat> <lon>");
        }
        bool a = false, b = false;
        const double lat = number(parts[1], a);
        const double lon = number(parts[2], b);
        if (!a || !b) {
            return fail("probe wants two numbers");
        }
        return probe(lat, lon);
    }

    if (verb == "find") {
        if (parts.size() < 2) {
            return fail("find <target>");
        }
        return find(parts[1]);
    }

    return fail("unknown command: " + verb);
}

} // namespace tools
