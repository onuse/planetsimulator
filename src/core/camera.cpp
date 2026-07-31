#define GLM_ENABLE_EXPERIMENTAL
#include "core/camera.hpp"
#include "utils/log.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace core {

Camera::Camera(uint32_t width, uint32_t height)
    : viewportWidth(width)
    , viewportHeight(height)
    , aspectRatio(static_cast<float>(width) / static_cast<float>(height))
    , position(0.0f, 0.0f, 10000.0f)  // Start at reasonable distance, will be set properly
    , target(0.0f, 0.0f, 0.0f)
    , up(0.0f, 1.0f, 0.0f)
    , orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
    
    // Auto-adjust clip planes based on initial altitude (temporary until properly set)
    float altitude = glm::length(position) - 1000.0f; // Temporary default
    autoAdjustClipPlanes(altitude, 1000.0f);
    
    util::vlog() << "Camera constructor: altitude=" << altitude << ", near=" << nearPlane << ", far=" << farPlane << std::endl;
    
    updateVectors();
    updateViewMatrix();
    updateProjection();
}

void Camera::update(float deltaTime) {
    // Update transitions if active
    if (isTransitioning()) {
        updateTransition(deltaTime);
    }
    
    // Apply inertia to smooth movement
    if (smoothingEnabled) {
        applyInertia(deltaTime);
    }
    
    // Update based on camera mode
    switch (mode) {
        case CameraMode::Orbital:
            updateOrbitalPosition();
            break;
        case CameraMode::FreeFly:
            updateFreeFlyPosition(deltaTime);
            break;
        case CameraMode::FirstPerson:
            // First person mode would lock to surface
            // Implementation depends on planet surface query
            break;
    }
    
    // Enforce minimum altitude to prevent going inside planet
    // Note: This should be set based on actual planet radius from outside
    // For now, just ensure we don't go too close to origin
    const float MIN_DISTANCE = 10.0f; // Minimum 10 meters from origin
    float distanceFromCenter = glm::length(position);
    if (distanceFromCenter < MIN_DISTANCE) {
        // Push camera back to minimum distance
        position = glm::normalize(position) * MIN_DISTANCE;
        if (mode == CameraMode::Orbital) {
            orbitDistance = MIN_DISTANCE;
        }
    }
    
    // Update matrices
    updateVectors();
    updateViewMatrix();
}

// ============================================================================
// Orbital Mode Controls
// ============================================================================

void Camera::orbit(float deltaAzimuth, float deltaElevation) {
    if (mode != CameraMode::Orbital) return;
    
    // Both rotations are about the camera's own axes rather than the world's,
    // so a drag moves the surface in the direction of the drag wherever the
    // camera is - which the world-vertical version stopped doing near the
    // poles. Kept for keyboard and scripted use; dragSurface() is what the
    // mouse goes through.
    const glm::vec3 localUp = orbitRotation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 localRight = orbitRotation * glm::vec3(1.0f, 0.0f, 0.0f);

    orbitRotation = glm::angleAxis(deltaAzimuth * rotationSpeed, localUp) * orbitRotation;
    orbitRotation = glm::angleAxis(deltaElevation * rotationSpeed, localRight) * orbitRotation;
    orbitRotation = glm::normalize(orbitRotation);
}

void Camera::zoom(float delta) {
    if (mode == CameraMode::Orbital) {
        // Get actual planet radius from orbit center distance (approximation)
        const float PLANET_RADIUS = glm::length(orbitCenter) > 1.0f ? glm::length(orbitCenter) : 100.0f;
        const float MIN_ALTITUDE = PLANET_RADIUS * 0.01f; // 1% of radius minimum altitude
        
        float oldDistance = orbitDistance;
        
        // Scale zoom speed based on altitude for better control
        float altitude = orbitDistance - PLANET_RADIUS;
        float speedScale = 1.0f;
        
        // Scale thresholds based on planet size
        if (altitude < PLANET_RADIUS * 1.0f) {  // Below 1x radius altitude
            speedScale = 0.3f;  // Very slow zoom
        } else if (altitude < PLANET_RADIUS * 5.0f) {  // Below 5x radius
            speedScale = 0.5f;  // Slow zoom
        } else if (altitude < PLANET_RADIUS * 20.0f) {  // Below 20x radius
            speedScale = 0.7f;  // Moderate zoom
        }
        // Above 20x radius: normal zoom speed
        
        // Apply scaled exponential zoom
        float adjustedDelta = delta * speedScale;
        orbitDistance *= std::pow(zoomSpeed, -adjustedDelta);
        
        // Prevent camera from going inside planet
        orbitDistance = clamp(orbitDistance, PLANET_RADIUS + MIN_ALTITUDE, 100000000.0f);
        
        // Log significant zoom changes
        float newAltitude = orbitDistance - PLANET_RADIUS;
        if (std::abs(oldDistance - orbitDistance) > 1000.0f) {  // More than 1km change
            util::vlog() << "[CAMERA ZOOM] Alt: " << altitude/1000.0f << "km -> " 
                      << newAltitude/1000.0f << "km (delta: " << delta 
                      << ", scale: " << speedScale << ")" << std::endl;
        }
    } else {
        // Adjust movement speed for other modes
        movementSpeed *= std::pow(zoomSpeed, delta);
        movementSpeed = clamp(movementSpeed, 1.0f, 10000000.0f);
    }
}

void Camera::pan(float deltaX, float deltaY) {
    if (mode != CameraMode::Orbital) return;
    
    // Pan the orbit center
    glm::vec3 panRight = right * deltaX * orbitDistance * 0.001f;
    glm::vec3 panUp = up * deltaY * orbitDistance * 0.001f;
    orbitCenter += panRight + panUp;
}

// ============================================================================
// Free Fly Mode Controls
// ============================================================================

void Camera::moveForward(float distance) {
    if (mode == CameraMode::FreeFly) {
        velocity += forward * distance * movementSpeed;
    }
}

void Camera::moveRight(float distance) {
    if (mode == CameraMode::FreeFly) {
        velocity += right * distance * movementSpeed;
    }
}

void Camera::moveUp(float distance) {
    if (mode == CameraMode::FreeFly) {
        velocity += up * distance * movementSpeed;
    }
}

void Camera::rotate(float deltaYaw, float deltaPitch) {
    if (mode == CameraMode::FreeFly) {
        yaw += deltaYaw * rotationSpeed;
        pitch += deltaPitch * rotationSpeed;
        
        // Clamp pitch to prevent flipping
        pitch = clamp(pitch, -1.5f, 1.5f);
        
        // Update orientation quaternion
        glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat pitchQuat = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat rollQuat = glm::angleAxis(rollAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        
        orientation = yawQuat * pitchQuat * rollQuat;
    }
}

void Camera::roll(float angle) {
    if (mode == CameraMode::FreeFly) {
        rollAngle += angle * rotationSpeed;
    }
}

// ============================================================================
// Direct Control
// ============================================================================

void Camera::setPosition(const glm::vec3& pos) {
    position = pos;
    
    // Update orbital parameters if in orbital mode
    if (mode == CameraMode::Orbital) {
        orbitDistance = glm::length(position - orbitCenter);
        
        if (orbitDistance > 0.001f) {
            orbitRotation = orbitRotationLookingFrom(
                glm::normalize(position - orbitCenter));
        }
        
        util::vlog() << "[CAMERA] setPosition in Orbital mode: orbitDistance=" << orbitDistance 
                  << ", orbitCenter=(" << orbitCenter.x << "," << orbitCenter.y << "," << orbitCenter.z << ")" << std::endl;
    }
}

void Camera::setTarget(const glm::vec3& tgt) {
    target = tgt;
    
    if (mode == CameraMode::Orbital) {
        orbitCenter = target;
    }
}

void Camera::lookAt(const glm::vec3& tgt) {
    target = tgt;
    glm::vec3 dir = glm::normalize(target - position);
    
    if (mode == CameraMode::FreeFly) {
        // Calculate yaw and pitch from direction
        pitch = std::asin(-dir.y);
        yaw = std::atan2(dir.x, dir.z);
        
        glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat pitchQuat = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        orientation = yawQuat * pitchQuat;
    }
    
    // Update vectors and view matrix immediately so changes take effect
    updateVectors();
    updateViewMatrix();
}

void Camera::setUp(const glm::vec3& newUp) {
    up = glm::normalize(newUp);
}

// ============================================================================
// Camera Mode
// ============================================================================

void Camera::setMode(CameraMode newMode) {
    if (mode == newMode) return;
    
    // Convert current state to new mode
    if (newMode == CameraMode::Orbital) {
        // Switch to orbital: set orbit center at current target
        orbitCenter = target;
        orbitDistance = glm::length(position - orbitCenter);
        
        if (orbitDistance > 0.001f) {
            orbitRotation = orbitRotationLookingFrom(
                glm::normalize(position - orbitCenter));
        }
    } else if (newMode == CameraMode::FreeFly) {
        // Switch to free fly: calculate orientation from current view
        glm::vec3 dir = glm::normalize(target - position);
        pitch = std::asin(-dir.y);
        yaw = std::atan2(dir.x, dir.z);
        rollAngle = 0.0f;
        
        glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat pitchQuat = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        orientation = yawQuat * pitchQuat;
    }
    
    mode = newMode;
}

// ============================================================================
// Planet-Aware Functions
// ============================================================================

void Camera::viewFrom(const glm::vec3& direction, float planetRadius, float altitude) {
    const glm::vec3 outward = glm::normalize(direction);

    orbitCenter = glm::vec3(0.0f);
    orbitDistance = planetRadius + std::max(altitude, 1.0f);
    orbitRotation = orbitRotationLookingFrom(outward);

    setMode(CameraMode::Orbital);
    updateOrbitalPosition();
    autoAdjustClipPlanes(std::max(altitude, 1.0f), planetRadius);
    autoAdjustSpeed(std::max(altitude, 1.0f));
    updateViewMatrix();
}

void Camera::alignToPlanetSurface(const glm::vec3& planetCenter, float /*planetRadius*/) {
    glm::vec3 toPlanet = position - planetCenter;
    float distance = glm::length(toPlanet);
    
    if (distance > 0.001f) {
        // Set up vector to point away from planet center
        up = glm::normalize(toPlanet);
        
        // Adjust forward vector to be tangent to planet surface
        if (std::abs(glm::dot(forward, up)) > 0.99f) {
            // Forward is too aligned with up, use a different basis
            forward = glm::normalize(glm::cross(up, glm::vec3(1.0f, 0.0f, 0.0f)));
            if (glm::length(forward) < 0.001f) {
                forward = glm::normalize(glm::cross(up, glm::vec3(0.0f, 0.0f, 1.0f)));
            }
        } else {
            // Project forward onto tangent plane
            forward = glm::normalize(forward - up * glm::dot(forward, up));
        }
        
        right = glm::normalize(glm::cross(forward, up));
    }
}

void Camera::clampToMinimumAltitude(const glm::vec3& planetCenter, float planetRadius, float minAltitude) {
    glm::vec3 toPlanet = position - planetCenter;
    float distance = glm::length(toPlanet);
    float minDistance = planetRadius + minAltitude;
    
    if (distance < minDistance && distance > 0.001f) {
        position = planetCenter + glm::normalize(toPlanet) * minDistance;
        
        if (mode == CameraMode::Orbital) {
            orbitDistance = minDistance;
        }
    }
}

float Camera::getAltitude(const glm::vec3& planetCenter, float planetRadius) const {
    return glm::length(position - planetCenter) - planetRadius;
}

// ============================================================================
// Smooth Transitions
// ============================================================================

void Camera::startTransition(const glm::vec3& targetPosition, const glm::quat& targetRotation, float duration) {
    transitionStartPos = position;
    transitionEndPos = targetPosition;
    transitionStartRot = orientation;
    transitionEndRot = targetRotation;
    transitionTime = 0.0f;
    transitionDuration = duration;
}

void Camera::updateTransition(float deltaTime) {
    transitionTime += deltaTime;
    if (transitionTime >= transitionDuration) {
        position = transitionEndPos;
        orientation = transitionEndRot;
        transitionTime = transitionDuration;
        return;
    }
    
    float t = smoothStep(transitionTime / transitionDuration);
    position = glm::mix(transitionStartPos, transitionEndPos, t);
    orientation = glm::slerp(transitionStartRot, transitionEndRot, t);
}

// ============================================================================
// Projection Settings
// ============================================================================

void Camera::setFieldOfView(float newFov) {
    fov = clamp(newFov, 1.0f, 179.0f);
    updateProjection();
}

void Camera::setAspectRatio(float aspect) {
    aspectRatio = aspect;
    updateProjection();
}

void Camera::setNearFar(float near, float far) {
    nearPlane = std::max(0.001f, near);
    farPlane = std::max(nearPlane + 0.001f, far);
    updateProjection();
}

void Camera::updateProjection() {
    // util::vlog() << "updateProjection called: near=" << nearPlane << ", far=" << farPlane << std::endl;
    
    projectionMatrix = glm::perspective(
        glm::radians(fov),
        aspectRatio,
        nearPlane,
        farPlane
    );
    
    // Vulkan clip space has inverted Y and half Z
    projectionMatrix[1][1] *= -1.0f;
}

void Camera::autoAdjustClipPlanes(float altitude, float planetRadius) {
    // Relief the near plane must not clip. Altitude is measured against the
    // sea-level sphere, so a camera a kilometre up over a mountain range is
    // much closer to the ground than its altitude suggests.
    constexpr float MAX_RELIEF = 15000.0f;

    const float clearance = std::max(altitude - MAX_RELIEF, 1.0f);

    // A tenth of the clearance. Large enough that the ratio to the far plane
    // stays in the hundreds - which is what keeps the depth buffer usable -
    // and still an order of magnitude closer than anything that can be drawn.
    nearPlane = std::max(0.5f, clearance * 0.1f);

    // Distance to the horizon from this altitude, by Pythagoras on the
    // tangent: the far plane only has to reach the furthest ground visible,
    // and beyond the horizon the planet occludes itself. The margin covers
    // terrain standing up past the horizon plane and, later, atmosphere.
    const float distance = planetRadius + std::max(altitude, 0.0f);
    const float horizon =
        std::sqrt(std::max(distance * distance - planetRadius * planetRadius, 0.0f));
    farPlane = horizon + MAX_RELIEF * 4.0f + planetRadius * 0.05f;

    farPlane = std::max(farPlane, nearPlane * 16.0f);

    updateProjection();
}

void Camera::setViewport(uint32_t width, uint32_t height) {
    viewportWidth = width;
    viewportHeight = height;
    aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    updateProjection();
}

// ============================================================================
// Frustum Culling
// ============================================================================

Camera::Frustum Camera::getFrustum() const {
    Frustum frustum;
    glm::mat4 vp = projectionMatrix * viewMatrix;
    
    // Extract frustum planes from view-projection matrix
    // Left plane
    frustum.planes[0] = glm::vec4(
        vp[0][3] + vp[0][0],
        vp[1][3] + vp[1][0],
        vp[2][3] + vp[2][0],
        vp[3][3] + vp[3][0]
    );
    
    // Right plane
    frustum.planes[1] = glm::vec4(
        vp[0][3] - vp[0][0],
        vp[1][3] - vp[1][0],
        vp[2][3] - vp[2][0],
        vp[3][3] - vp[3][0]
    );
    
    // Bottom plane
    frustum.planes[2] = glm::vec4(
        vp[0][3] + vp[0][1],
        vp[1][3] + vp[1][1],
        vp[2][3] + vp[2][1],
        vp[3][3] + vp[3][1]
    );
    
    // Top plane
    frustum.planes[3] = glm::vec4(
        vp[0][3] - vp[0][1],
        vp[1][3] - vp[1][1],
        vp[2][3] - vp[2][1],
        vp[3][3] - vp[3][1]
    );
    
    // Near plane
    frustum.planes[4] = glm::vec4(
        vp[0][3] + vp[0][2],
        vp[1][3] + vp[1][2],
        vp[2][3] + vp[2][2],
        vp[3][3] + vp[3][2]
    );
    
    // Far plane
    frustum.planes[5] = glm::vec4(
        vp[0][3] - vp[0][2],
        vp[1][3] - vp[1][2],
        vp[2][3] - vp[2][2],
        vp[3][3] - vp[3][2]
    );
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(frustum.planes[i]));
        if (length > 0.0f) {
            frustum.planes[i] /= length;
        }
    }
    
    return frustum;
}

bool Camera::Frustum::containsSphere(const glm::vec3& center, float radius) const {
    for (int i = 0; i < 6; i++) {
        float distance = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool Camera::Frustum::containsBox(const glm::vec3& min, const glm::vec3& max) const {
    glm::vec3 corners[8] = {
        glm::vec3(min.x, min.y, min.z),
        glm::vec3(max.x, min.y, min.z),
        glm::vec3(min.x, max.y, min.z),
        glm::vec3(max.x, max.y, min.z),
        glm::vec3(min.x, min.y, max.z),
        glm::vec3(max.x, min.y, max.z),
        glm::vec3(min.x, max.y, max.z),
        glm::vec3(max.x, max.y, max.z)
    };
    
    for (int i = 0; i < 6; i++) {
        int out = 0;
        for (int j = 0; j < 8; j++) {
            float distance = glm::dot(glm::vec3(planes[i]), corners[j]) + planes[i].w;
            if (distance < 0) {
                out++;
            }
        }
        if (out == 8) {
            return false; // All corners outside this plane
        }
    }
    return true;
}

// ============================================================================
// Speed Controls
// ============================================================================

void Camera::autoAdjustSpeed(float altitude) {
    // Exponentially scale movement speed with altitude
    if (altitude < 1000.0f) {
        movementSpeed = 10.0f; // 10 m/s when near surface
    } else if (altitude < 10000.0f) {
        movementSpeed = altitude * 0.1f; // Scale up to 1 km/s
    } else if (altitude < 100000.0f) {
        movementSpeed = altitude * 0.01f; // Scale up to 10 km/s
    } else {
        movementSpeed = altitude * 0.001f; // Scale up to 100+ km/s in space
    }
    
    // Also adjust zoom speed
    zoomSpeed = 1.0f + (altitude / 1000000.0f) * 0.5f; // Faster zoom at high altitude
}

// ============================================================================
// Private Update Functions
// ============================================================================

void Camera::updateVectors() {
    if (mode == CameraMode::FreeFly) {
        // Extract vectors from orientation quaternion
        glm::mat3 rotMatrix = glm::mat3_cast(orientation);
        forward = -rotMatrix[2]; // -Z is forward in OpenGL/Vulkan
        right = rotMatrix[0];     // X is right
        up = rotMatrix[1];        // Y is up
    } else if (mode == CameraMode::Orbital) {
        // Overhead comes from the orbit rotation, which is the only thing that
        // knows how the camera has been turned.
        //
        // This used to rebuild the basis from world north regardless, which
        // discarded whatever roll the orbit carried - so dragging the surface
        // could not track the cursor, because part of the rotation it applied
        // was thrown away before the next frame's ray was cast. It also had to
        // special-case looking straight down the axis, where world north is
        // parallel to the view and the cross product collapses; that switched
        // reference axis abruptly and snapped the view round as the camera
        // crossed a pole. Neither problem exists if the orbit's own frame is
        // used, and there is no pole in it to special-case.
        forward = glm::normalize(target - position);
        const glm::vec3 orbitUp = orbitRotation * glm::vec3(0.0f, 1.0f, 0.0f);
        right = glm::normalize(glm::cross(forward, orbitUp));
        up = glm::normalize(glm::cross(right, forward));
    } else {
        // Calculate vectors from position and target
        forward = glm::normalize(target - position);
        if (std::abs(forward.y) > 0.999f) {
            // Looking straight up or down, use a different up vector
            right = glm::normalize(glm::cross(forward, glm::vec3(1.0f, 0.0f, 0.0f)));
        } else {
            right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        }
        up = glm::normalize(glm::cross(right, forward));
    }
}

void Camera::updateViewMatrix() {
    if (mode == CameraMode::FreeFly) {
        // Use orientation quaternion for free fly
        glm::mat4 rotation = glm::mat4_cast(glm::conjugate(orientation));
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position);
        viewMatrix = rotation * translation;
    } else {
        // Use look-at for orbital and first-person modes
        viewMatrix = glm::lookAt(position, target, up);
    }
}

glm::quat Camera::orbitRotationLookingFrom(const glm::vec3& direction) {
    // The rotation taking +Z to this direction, with the camera's overhead as
    // close to world north as it can be. Degenerate directly above a pole, so
    // there the reference is swung to +Z instead; any choice is arbitrary
    // there and this one is at least continuous with the approach to it.
    const glm::vec3 f = glm::normalize(direction);
    const glm::vec3 reference =
        std::abs(f.y) > 0.9999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

    const glm::vec3 r = glm::normalize(glm::cross(reference, f));
    const glm::vec3 u = glm::cross(f, r);
    return glm::normalize(glm::quat_cast(glm::mat3(r, u, f)));
}

glm::vec3 Camera::rayDirection(const glm::vec2& pixel) const {
    // Pixel to normalised device coordinates. Y is flipped because window
    // coordinates count down from the top and clip space counts up.
    const float ndcX = (2.0f * pixel.x / static_cast<float>(viewportWidth)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * pixel.y / static_cast<float>(viewportHeight));

    const float tanHalfFov = std::tan(glm::radians(fov) * 0.5f);
    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

    // Built from the camera's own axes rather than by inverting the view
    // matrix, so this cannot drift out of step with how the view is built.
    return glm::normalize(forward + right * (ndcX * tanHalfFov * aspect) +
                          up * (ndcY * tanHalfFov));
}

bool Camera::pickSphere(const glm::vec2& pixel, float radius, glm::vec3& outDirection) const {
    const glm::vec3 origin = position;
    const glm::vec3 dir = rayDirection(pixel);

    const float distanceSquared = glm::dot(origin, origin);
    if (distanceSquared <= radius * radius) {
        return false;   // inside the planet; there is no sphere to grab
    }

    // Closest approach of the ray to the centre.
    const float along = -glm::dot(origin, dir);
    const glm::vec3 closest = origin + dir * along;
    const float missDistanceSquared = glm::dot(closest, closest);

    if (missDistanceSquared <= radius * radius) {
        const float halfChord = std::sqrt(radius * radius - missDistanceSquared);
        const float hit = along - halfChord;   // near intersection
        outDirection = glm::normalize(origin + dir * hit);
        return true;
    }

    // The ray missed. The closest point of the sphere to it is on the
    // silhouette, and using that keeps a drag continuous when the cursor
    // leaves the planet instead of stalling it at the edge.
    outDirection = glm::normalize(closest);
    return true;
}

void Camera::dragSurface(const glm::vec3& fromDirection, const glm::vec3& toDirection) {
    if (mode != CameraMode::Orbital) {
        return;
    }

    const glm::vec3 from = glm::normalize(fromDirection);
    const glm::vec3 to = glm::normalize(toDirection);

    const float cosAngle = glm::clamp(glm::dot(from, to), -1.0f, 1.0f);
    if (cosAngle > 0.999999f) {
        return;   // the cursor has not moved far enough to matter
    }

    glm::vec3 axis = glm::cross(from, to);
    const float axisLength = glm::length(axis);
    if (axisLength < 1e-8f) {
        return;   // antipodal; the axis is undefined
    }
    axis /= axisLength;

    // Rotating the camera by the rotation that takes the point now under the
    // cursor to the point that was under it when the drag began puts the
    // grabbed ground back under the cursor. Recomputed from the live camera
    // every frame rather than integrated, so it cannot drift.
    const glm::quat delta = glm::angleAxis(std::acos(cosAngle), axis);
    orbitRotation = glm::normalize(delta * orbitRotation);
}

void Camera::updateOrbitalPosition() {
    position = orbitCenter + (orbitRotation * glm::vec3(0.0f, 0.0f, 1.0f)) * orbitDistance;
    up = orbitRotation * glm::vec3(0.0f, 1.0f, 0.0f);
    target = orbitCenter;
}

void Camera::updateFreeFlyPosition(float deltaTime) {
    // Apply velocity with delta time
    position += velocity * deltaTime;
    
    // Update target based on forward direction
    target = position + forward * 1000.0f;
}

void Camera::applyInertia(float deltaTime) {
    // Dampen velocity over time
    velocity *= std::pow(1.0f - inertia, deltaTime);
    angularVelocity *= std::pow(1.0f - inertia, deltaTime);
    
    // Stop if velocity is negligible
    if (glm::length(velocity) < 0.001f) {
        velocity = glm::vec3(0.0f);
    }
    if (glm::length(angularVelocity) < 0.001f) {
        angularVelocity = glm::vec3(0.0f);
    }
}

// ============================================================================
// Debug
// ============================================================================

void Camera::printDebugInfo() const {
    util::vlog() << "Camera Debug Info:\n";
    util::vlog() << "  Mode: " << (mode == CameraMode::Orbital ? "Orbital" : 
                               (mode == CameraMode::FreeFly ? "FreeFly" : "FirstPerson")) << "\n";
    util::vlog() << "  Position: (" << position.x << ", " << position.y << ", " << position.z << ")\n";
    util::vlog() << "  Target: (" << target.x << ", " << target.y << ", " << target.z << ")\n";
    util::vlog() << "  Forward: (" << forward.x << ", " << forward.y << ", " << forward.z << ")\n";
    util::vlog() << "  Up: (" << up.x << ", " << up.y << ", " << up.z << ")\n";
    util::vlog() << "  FOV: " << fov << " degrees\n";
    util::vlog() << "  Near/Far: " << nearPlane << " / " << farPlane << "\n";
    util::vlog() << "  Movement Speed: " << movementSpeed << " m/s\n";
    
    if (mode == CameraMode::Orbital) {
        util::vlog() << "  Orbit Distance: " << orbitDistance << " m\n";
        const glm::vec3 dir = orbitRotation * glm::vec3(0.0f, 0.0f, 1.0f);
        util::vlog() << "  Orbit Latitude: " << glm::degrees(std::asin(dir.y)) << " degrees\n";
        util::vlog() << "  Orbit Longitude: " << glm::degrees(std::atan2(dir.x, dir.z)) << " degrees\n";
    }
}

} // namespace core