#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace core {

// Camera modes for different control schemes
enum class CameraMode {
    Orbital,    // Orbits around planet (default)
    FreeFly,    // Free flying camera
    FirstPerson // Surface-locked first person
};

class Camera {
public:
    Camera(uint32_t width, uint32_t height);
    ~Camera() = default;
    
    // Update camera (call once per frame)
    void update(float deltaTime);
    
    // View controls for orbital mode
    void orbit(float deltaAzimuth, float deltaElevation);
    void zoom(float delta);
    void pan(float deltaX, float deltaY);
    
    // Movement controls for free fly mode
    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    void rotate(float yaw, float pitch);
    void roll(float angle);
    
    // Direct control
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void lookAt(const glm::vec3& target);
    void setUp(const glm::vec3& up);
    
    // Camera mode
    void setMode(CameraMode mode);
    CameraMode getMode() const { return mode; }
    
    // Planet-aware functions
    void alignToPlanetSurface(const glm::vec3& planetCenter, float planetRadius);
    void clampToMinimumAltitude(const glm::vec3& planetCenter, float planetRadius, float minAltitude);
    float getAltitude(const glm::vec3& planetCenter, float planetRadius) const;
    
    // Smooth transitions
    void startTransition(const glm::vec3& targetPosition, const glm::quat& targetRotation, float duration);
    bool isTransitioning() const { return transitionTime < transitionDuration; }
    
    // Projection settings
    void setFieldOfView(float fov);
    void setAspectRatio(float aspect);
    void setNearFar(float near, float far);
    void updateProjection();
    
    // Auto-adjust near/far based on altitude
    void autoAdjustClipPlanes(float altitude);
    
    // Viewport
    void setViewport(uint32_t width, uint32_t height);
    
    // Getters for matrices
    const glm::mat4& getViewMatrix() const { return viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return projectionMatrix; }
    glm::mat4 getProjectionMatrix(float aspect) const {
        return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    }
    glm::mat4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }
    
    // Getters for properties
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getTarget() const { return target; }
    const glm::vec3& getForward() const { return forward; }
    const glm::vec3& getRight() const { return right; }
    const glm::vec3& getUp() const { return up; }
    float getFieldOfView() const { return fov; }
    float getNearPlane() const { return nearPlane; }
    float getFarPlane() const { return farPlane; }
    
    // Frustum for culling
    struct Frustum {
        glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far
        
        bool containsSphere(const glm::vec3& center, float radius) const;
        bool containsBox(const glm::vec3& min, const glm::vec3& max) const;
    };
    
    Frustum getFrustum() const;
    
    // Speed controls
    void setMovementSpeed(float speed) { movementSpeed = speed; }
    void setRotationSpeed(float speed) { rotationSpeed = speed; }
    float getMovementSpeed() const { return movementSpeed; }
    float getRotationSpeed() const { return rotationSpeed; }
    
    // Auto-adjust speed based on altitude
    void autoAdjustSpeed(float altitude);
    
    // Inertia and smoothing
    void setInertia(float newInertia) { inertia = clamp(newInertia, 0.0f, 0.99f); }
    void setSmoothingEnabled(bool enabled) { smoothingEnabled = enabled; }
    
    // Debug info
    void printDebugInfo() const;
    
private:
    // Camera mode
    CameraMode mode = CameraMode::Orbital;
    
    // Position and orientation
    glm::vec3 position;
    glm::vec3 target;      // Look-at target
    glm::vec3 up;
    
    // Cached direction vectors
    glm::vec3 forward;
    glm::vec3 right;
    
    // Orbital mode parameters
    float orbitDistance = 10000000.0f;  // 10,000 km default
    float orbitAzimuth = 0.0f;          // Horizontal angle (radians)
    float orbitElevation = 0.0f;        // Vertical angle (radians)
    glm::vec3 orbitCenter = glm::vec3(0.0f); // Usually planet center
    
    // Free fly mode parameters
    glm::quat orientation;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float rollAngle = 0.0f;
    
    // Projection parameters
    float fov = 60.0f;          // Field of view in degrees
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 100.0f;   // 100 meters
    float farPlane = 100000000.0f; // 100,000 km
    
    // Viewport
    uint32_t viewportWidth;
    uint32_t viewportHeight;
    
    // Matrices
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    
    // Movement parameters
    float movementSpeed = 1000.0f;  // meters per second
    float rotationSpeed = 1.0f;     // radians per second
    float zoomSpeed = 1.1f;          // Multiplicative zoom factor
    
    // Smooth movement
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 angularVelocity = glm::vec3(0.0f);
    float inertia = 0.9f;           // 0 = no inertia, 1 = infinite inertia
    bool smoothingEnabled = true;
    
    // Transition animation
    glm::vec3 transitionStartPos;
    glm::vec3 transitionEndPos;
    glm::quat transitionStartRot;
    glm::quat transitionEndRot;
    float transitionTime = 0.0f;
    float transitionDuration = 0.0f;
    
    // Helper functions
    void updateVectors();
    void updateViewMatrix();
    void updateOrbitalPosition();
    void updateFreeFlyPosition(float deltaTime);
    void applyInertia(float deltaTime);
    void updateTransition(float deltaTime);
    
    // Utility
    static float clamp(float value, float min, float max) {
        return value < min ? min : (value > max ? max : value);
    }
    
    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    
    static float smoothStep(float t) {
        t = clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
};

} // namespace core