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
    
    // Dragging the planet itself.
    //
    // Orbiting by accumulating azimuth and elevation is not what a hand on a
    // globe does. It rotates about the world's vertical axis rather than the
    // screen's, so how far the surface travels for a given drag depends on
    // where the camera happens to be - near the poles it spins in place - and
    // the direction the ground moves stops matching the direction of the drag.
    //
    // Instead: find the point of the planet under the cursor when the drag
    // starts, and every frame afterwards rotate the camera so that same point
    // is under the cursor again. The ground then follows the mouse exactly,
    // at every latitude and any zoom, because it is the same question being
    // asked each frame rather than an angle being accumulated.
    //
    // Both arguments are unit directions from the planet centre, as returned
    // by pickSphere.
    void dragSurface(const glm::vec3& fromDirection, const glm::vec3& toDirection);

    // World-space direction of the ray through a pixel.
    glm::vec3 rayDirection(const glm::vec2& pixel) const;

    // Where the ray through a pixel meets the planet, as a unit direction from
    // its centre. Off the limb there is no intersection, so this returns the
    // nearest point on the silhouette instead - which keeps a drag going
    // smoothly when the cursor runs off the edge of the planet rather than
    // dropping it. Returns false only if the camera is inside the sphere.
    bool pickSphere(const glm::vec2& pixel, float radius, glm::vec3& outDirection) const;

    // Planet-aware functions
    // Put the camera directly above a point on the sphere, looking down at it.
    //
    // Everything else that moves the camera is relative - orbit by this much,
    // zoom by that much - which is right for a hand on a mouse and useless for
    // "show me the mouth of the largest river". This is the absolute form, and
    // it is what makes a view reproducible from one run to the next.
    void viewFrom(const glm::vec3& direction, float planetRadius, float altitude);

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
    // Put the near and far planes where the visible world actually is.
    //
    // Both are derived rather than tabulated. Nothing can be nearer than the
    // ground below the camera, so the near plane tracks altitude; nothing
    // solid is further than the horizon, so the far plane is the horizon
    // distance for this altitude. That keeps the ratio between them small at
    // every scale, which is what depth precision depends on - a fixed near
    // plane either clips the ground on approach or wastes the entire depth
    // buffer from orbit.
    void autoAdjustClipPlanes(float altitude, float planetRadius);
    
    // Viewport
    void setViewport(uint32_t width, uint32_t height);
    
    // Getters for matrices
    const glm::mat4& getViewMatrix() const { return viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return projectionMatrix; }
    // Same convention as the no-argument version, including the Y flip for
    // Vulkan clip space. It did not used to, which made two functions of the
    // same name disagree about which way up the world was.
    glm::mat4 getProjectionMatrix(float aspect) const {
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        projection[1][1] *= -1.0f;
        return projection;
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
    float orbitDistance = 10000.0f;  // Will be set based on planet size
    // Where the camera sits on its orbit, as a rotation rather than a pair of
    // angles: the camera is at orbitRotation * +Z, with orbitRotation * +Y
    // overhead. Angles have a singularity at the poles that a rotation does
    // not, and dragging the surface composes rotations directly - there is no
    // pair of angles that expresses "rotate about this arbitrary axis".
    glm::quat orbitRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // The orbit rotation that places the camera along a given direction from
    // the orbit centre, keeping north as close to overhead as possible.
    static glm::quat orbitRotationLookingFrom(const glm::vec3& direction);
    glm::vec3 orbitCenter = glm::vec3(0.0f); // Usually planet center
    
    // Free fly mode parameters
    glm::quat orientation;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float rollAngle = 0.0f;
    
    // Projection parameters
    float fov = 75.0f;          // Field of view in degrees - wider to see more planet
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 1000.0f;   // 1 km - reasonable default
    float farPlane = 20000000.0f; // 20,000 km - reasonable default
    
    // Viewport
    uint32_t viewportWidth;
    uint32_t viewportHeight;
    
    // Matrices
    glm::mat4 viewMatrix = glm::mat4(1.0f);          // Initialize to identity
    glm::mat4 projectionMatrix = glm::mat4(1.0f);    // Initialize to identity
    
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