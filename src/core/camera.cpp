#define GLM_ENABLE_EXPERIMENTAL
#include "core/camera.hpp"
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
    , position(0.0f, 0.0f, 6400000.0f)  // Start ~30km above planet surface
    , target(0.0f, 0.0f, 0.0f)
    , up(0.0f, 1.0f, 0.0f)
    , orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
    
    // Auto-adjust clip planes based on initial altitude
    float altitude = glm::length(position) - 6371000.0f; // Assume planet radius
    autoAdjustClipPlanes(altitude);
    
    std::cout << "Camera constructor: altitude=" << altitude << ", near=" << nearPlane << ", far=" << farPlane << std::endl;
    
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
    const float PLANET_RADIUS = 6371000.0f;
    const float MIN_ALTITUDE = 10000.0f; // 10km minimum altitude
    float distanceFromCenter = glm::length(position);
    if (distanceFromCenter < PLANET_RADIUS + MIN_ALTITUDE) {
        // Push camera back to minimum altitude
        position = glm::normalize(position) * (PLANET_RADIUS + MIN_ALTITUDE);
        if (mode == CameraMode::Orbital) {
            orbitDistance = PLANET_RADIUS + MIN_ALTITUDE;
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
    
    orbitAzimuth += deltaAzimuth * rotationSpeed;
    orbitElevation += deltaElevation * rotationSpeed;
    
    // Clamp elevation to prevent gimbal lock
    orbitElevation = clamp(orbitElevation, -1.5f, 1.5f); // ~+-85 degrees
    
    // Keep azimuth in reasonable range
    while (orbitAzimuth > static_cast<float>(M_PI * 2.0)) orbitAzimuth -= static_cast<float>(M_PI * 2.0);
    while (orbitAzimuth < 0.0f) orbitAzimuth += static_cast<float>(M_PI * 2.0);
}

void Camera::zoom(float delta) {
    if (mode == CameraMode::Orbital) {
        const float PLANET_RADIUS = 6371000.0f;
        const float MIN_ALTITUDE = 10000.0f; // 10km minimum altitude above surface
        
        // Scale zoom speed based on altitude for better control
        float altitude = orbitDistance - PLANET_RADIUS;
        float speedScale = 1.0f;
        
        if (altitude < 100000.0f) {  // Below 100km
            speedScale = 0.3f;  // Very slow zoom
        } else if (altitude < 500000.0f) {  // Below 500km
            speedScale = 0.5f;  // Slow zoom
        } else if (altitude < 2000000.0f) {  // Below 2000km
            speedScale = 0.7f;  // Moderate zoom
        }
        // Above 2000km: normal zoom speed
        
        // Apply scaled exponential zoom
        float adjustedDelta = delta * speedScale;
        orbitDistance *= std::pow(zoomSpeed, -adjustedDelta);
        
        // Prevent camera from going inside planet
        orbitDistance = clamp(orbitDistance, PLANET_RADIUS + MIN_ALTITUDE, 100000000.0f);
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
            glm::vec3 dir = glm::normalize(position - orbitCenter);
            orbitElevation = std::asin(dir.y);
            orbitAzimuth = std::atan2(dir.x, dir.z);
        }
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
            glm::vec3 dir = glm::normalize(position - orbitCenter);
            orbitElevation = std::asin(dir.y);
            orbitAzimuth = std::atan2(dir.x, dir.z);
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
    // std::cout << "updateProjection called: near=" << nearPlane << ", far=" << farPlane << std::endl;
    
    projectionMatrix = glm::perspective(
        glm::radians(fov),
        aspectRatio,
        nearPlane,
        farPlane
    );
    
    // Vulkan clip space has inverted Y and half Z
    projectionMatrix[1][1] *= -1.0f;
}

void Camera::autoAdjustClipPlanes(float altitude) {
    // Dynamic clipping planes based on altitude for scaled coordinate system
    // altitude is in meters, we scale by 1/1,000,000 in the renderer
    
    const float planetRadius = 6371000.0f;  // meters
    float cameraDistance = altitude + planetRadius;  // Distance from planet center
    
    if (altitude < 1000.0f) {
        // Very close to surface (< 1km) - for walking around
        nearPlane = 0.1f;  // 10cm
        farPlane = 10000.0f;  // 10km view distance
    } else if (altitude < 100000.0f) {
        // Low altitude (1km - 100km) - for flying
        nearPlane = altitude * 0.01f;  // 1% of altitude
        farPlane = altitude * 20.0f + planetRadius;  // See to horizon
    } else if (altitude < 1000000.0f) {
        // Medium altitude (100km - 1000km) - see significant portion of planet
        nearPlane = altitude * 0.1f;  // 10% of altitude
        farPlane = cameraDistance * 3.0f;  // See past planet
    } else {
        // High altitude (> 1000km) - see whole planet
        nearPlane = altitude * 0.2f;  // 20% of altitude to avoid precision issues
        farPlane = cameraDistance * 4.0f;  // See well past planet
    }
    
    // Ensure minimum values for stability
    nearPlane = std::max(nearPlane, 0.1f);
    farPlane = std::max(farPlane, nearPlane * 1000.0f);  // Keep ratio reasonable
    
    // PERFORMANCE: Disabled clipping plane debug logging
    // std::cout << "[DEBUG] Clipping planes OVERRIDDEN for debugging: near=" 
    //           << nearPlane << ", far=" << farPlane << std::endl;
    
    /* ORIGINAL CODE - DISABLED FOR DEBUGGING
    // Adjust near/far planes based on altitude
    if (altitude < 20000.0f) {
        // Very close to surface (10-20km)
        nearPlane = 1.0f;  // 1 meter near plane for close detail
        farPlane = 100000.0f;  // 100km far plane
    } else if (altitude < 100000.0f) {
        // Low altitude (20-100km)
        nearPlane = 10.0f;
        farPlane = altitude * 100.0f;
    } else if (altitude < 1000000.0f) {
        // Medium altitude (100km-1000km)
        nearPlane = 100.0f;
        farPlane = altitude * 50.0f;
    } else {
        // High altitude / space (>1000km)
        // Keep near/far ratio under 10,000:1 to avoid precision issues
        nearPlane = altitude * 0.001f;   // Near at 0.1% of altitude
        farPlane = altitude * 2.0f;      // Far at 2x altitude
        // Clamp to reasonable values
        nearPlane = std::max(1000.0f, nearPlane);
        farPlane = std::min(50000000.0f, farPlane);  // Cap at 50M meters
    }
    */
    
    // std::cout << "autoAdjustClipPlanes result: near=" << nearPlane << ", far=" << farPlane << std::endl;
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

void Camera::updateOrbitalPosition() {
    // Calculate position from spherical coordinates
    float cosElev = std::cos(orbitElevation);
    float sinElev = std::sin(orbitElevation);
    float cosAzim = std::cos(orbitAzimuth);
    float sinAzim = std::sin(orbitAzimuth);
    
    position = orbitCenter + glm::vec3(
        orbitDistance * cosElev * sinAzim,
        orbitDistance * sinElev,
        orbitDistance * cosElev * cosAzim
    );
    
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
    std::cout << "Camera Debug Info:\n";
    std::cout << "  Mode: " << (mode == CameraMode::Orbital ? "Orbital" : 
                               (mode == CameraMode::FreeFly ? "FreeFly" : "FirstPerson")) << "\n";
    std::cout << "  Position: (" << position.x << ", " << position.y << ", " << position.z << ")\n";
    std::cout << "  Target: (" << target.x << ", " << target.y << ", " << target.z << ")\n";
    std::cout << "  Forward: (" << forward.x << ", " << forward.y << ", " << forward.z << ")\n";
    std::cout << "  Up: (" << up.x << ", " << up.y << ", " << up.z << ")\n";
    std::cout << "  FOV: " << fov << " degrees\n";
    std::cout << "  Near/Far: " << nearPlane << " / " << farPlane << "\n";
    std::cout << "  Movement Speed: " << movementSpeed << " m/s\n";
    
    if (mode == CameraMode::Orbital) {
        std::cout << "  Orbit Distance: " << orbitDistance << " m\n";
        std::cout << "  Orbit Azimuth: " << glm::degrees(orbitAzimuth) << " degrees\n";
        std::cout << "  Orbit Elevation: " << glm::degrees(orbitElevation) << " degrees\n";
    }
}

} // namespace core