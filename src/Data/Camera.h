#pragma once

#include <SDL_events.h>
#include <glm/gtx/quaternion.hpp>

#include <functional>

class Renderer;

enum MovementMode
{
	FREEFLY,
	DRONE,
};

class Camera {
private:
	Renderer* mRenderer;

public:
	glm::vec3 mVelocity;
	glm::vec3 mPosition;
	float mPitch{ 0.f };
	float mYaw{ 0.f };
	float mSpeed{ 10.f };

	SDL_bool mRelativeMode{ SDL_FALSE };
	MovementMode mMovementMode;
	std::unordered_map<MovementMode, std::function<void()>> mMovementFunctions;

	Camera(Renderer* renderer);

	void init();

	glm::mat4 getViewMatrix() const;
	glm::quat getPitchMatrix() const;
	glm::quat getYawMatrix() const;
	glm::mat4 getRotationMatrix() const;
	glm::vec3 getDirectionVector() const;

	void update(float deltaTime, float expectedDeltaTime);
};
