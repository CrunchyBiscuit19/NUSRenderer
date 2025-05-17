#pragma once

#include <SDL_events.h>
#include <glm/gtx/quaternion.hpp>

#include <functional>

enum MovementMode
{
    FREEFLY,
    DRONE,
};

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    float pitch{ 0.f };
    float yaw{ 0.f };
    float speed{ 10.f };

    const Uint8* keyState;
    SDL_bool relativeMode{ SDL_FALSE };
    MovementMode movementMode;
    std::unordered_map<MovementMode, std::function<void()>> movementFuns;

    void init();

    glm::mat4 getViewMatrix() const;
    glm::quat getPitchMatrix() const;
    glm::quat getYawMatrix() const;
    glm::mat4 getRotationMatrix() const;
    glm::vec3 getDirectionVector() const;

    void processSDLEvent(const SDL_Event& e);

    void update(float deltaTime, float expectedDeltaTime);
};
