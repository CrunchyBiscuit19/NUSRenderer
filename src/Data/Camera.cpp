#include <Renderer/Renderer.h>
#include <Data/Camera.h>

Camera::Camera(Renderer* renderer) :
	mRenderer(renderer)
{
	mVelocity = glm::vec3(0.f);
	mPosition = glm::vec3(0, 0, 5);
	mPitch = 0;
	mYaw = 0;
	mMovementMode = FREEFLY;

	mMovementFunctions[FREEFLY] = [this]() -> void {
		const SDL_Keymod modState = SDL_GetModState();
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);
		if (keyState[SDL_SCANCODE_W]) {
			if (modState & KMOD_LSHIFT)
				mVelocity.y = 1;
			else
				mVelocity.z = -1;
		}
		if (keyState[SDL_SCANCODE_S]) {
			if (modState & KMOD_LSHIFT)
				mVelocity.y = -1;
			else
				mVelocity.z = 1;
		}
		if (keyState[SDL_SCANCODE_A])
			mVelocity.x = -1;
		if (keyState[SDL_SCANCODE_D])
			mVelocity.x = 1;
		mVelocity *= 0.1f;
	};

	mMovementFunctions[DRONE] = [this]() -> void {
		const SDL_Keymod modState = SDL_GetModState();
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);
		if (keyState[SDL_SCANCODE_W])
			mVelocity.z = -1;
		if (keyState[SDL_SCANCODE_S])
			mVelocity.z = 1;
		if (keyState[SDL_SCANCODE_A])
			mVelocity.x = -1;
		if (keyState[SDL_SCANCODE_D])
			mVelocity.x = 1;
		mVelocity *= 0.1f;
	};
}

void Camera::init()
{
	mRenderer->mRendererEvent.addEventCallback([this](SDL_Event& e) -> void {
		const SDL_Keymod modState = SDL_GetModState();
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		mVelocity = glm::vec3(0.f);

		mMovementFunctions[mMovementMode]();

		// F12 is a reserved, kernel-based key for the debugger, so it will crash while using VS debugger. 
		// https://stackoverflow.com/questions/18997754/how-to-disable-f12-to-debug-application-in-visual-studio-2012
		// If we have to test it, build and launch from output folder (ie. click on the actual exe).
		if (keyState[SDL_SCANCODE_F11] && e.type == SDL_KEYDOWN && !e.key.repeat) {
			switch (mMovementMode) {
			case FREEFLY:
				mMovementMode = DRONE;
				break;
			case DRONE:
				mMovementMode = FREEFLY;
				break;
			}
		}

		if (e.button.button == SDL_BUTTON_RIGHT && e.type == SDL_MOUSEBUTTONDOWN)
			mRelativeMode = static_cast<SDL_bool>(!mRelativeMode);

		if (e.type == SDL_MOUSEMOTION && mRelativeMode) {
			mYaw += static_cast<float>(e.motion.xrel) / 200.f;
			mPitch -= static_cast<float>(e.motion.yrel) / 200.f;
		}
	});
}

glm::mat4 Camera::getViewMatrix() const
{
	const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), mPosition);
	const glm::mat4 cameraRotation = getRotationMatrix();
	return glm::inverse(cameraTranslation * cameraRotation);
}

glm::quat Camera::getPitchMatrix() const
{
	return glm::angleAxis(mPitch, glm::vec3{ 1.f, 0.f, 0.f });
}

glm::quat Camera::getYawMatrix() const
{
	return glm::angleAxis(mYaw, glm::vec3{ 0.f, -1.f, 0.f }); // Negative Y to flip OpenGL rotation?;
}

glm::mat4 Camera::getRotationMatrix() const
{
	const glm::quat pitchRotation = getPitchMatrix();
	const glm::quat yawRotation = getYawMatrix();
	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::vec3 Camera::getDirectionVector() const
{
	glm::vec3 direction;
	direction.x = std::cos(mPitch) * std::sin(mYaw);
	direction.y = std::sin(mPitch);
	direction.z = -std::cos(mPitch) * std::cos(mYaw);
	return -glm::normalize(direction);
}

void Camera::update(float deltaTime, float expectedDeltaTime)
{
	switch (mMovementMode) {
	case FREEFLY:
		mPosition += glm::vec3(getYawMatrix() * glm::vec4(mVelocity * mSpeed * (deltaTime / expectedDeltaTime), 0.f));
		break;
	case DRONE:
		mPosition += glm::vec3(getRotationMatrix() * glm::vec4(mVelocity * mSpeed * (deltaTime / expectedDeltaTime), 0.f));
		break;
	}
}