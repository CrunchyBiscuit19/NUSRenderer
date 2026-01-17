#include <Data/Camera.h>
#include <Renderer/Renderer.h>
#include <quill/LogMacros.h>

Camera::Camera(Renderer* renderer) : mRenderer(renderer) {
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
        if (keyState[SDL_SCANCODE_A]) mVelocity.x = -1;
        if (keyState[SDL_SCANCODE_D]) mVelocity.x = 1;
        mVelocity *= 0.1f;
    };

    mMovementFunctions[DRONE] = [this]() -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);
        if (keyState[SDL_SCANCODE_W]) mVelocity.z = -1;
        if (keyState[SDL_SCANCODE_S]) mVelocity.z = 1;
        if (keyState[SDL_SCANCODE_A]) mVelocity.x = -1;
        if (keyState[SDL_SCANCODE_D]) mVelocity.x = 1;
        mVelocity *= 0.1f;
    };
}

void Camera::initControls() {
    mRenderer->mEventHandler.addEventCallback([this](SDL_Event& e) -> void {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);

        mVelocity = glm::vec3(0.f);

        mMovementFunctions[mMovementMode]();

        if (keyState[SDL_SCANCODE_C] && e.type == SDL_KEYDOWN && !e.key.repeat) {
            switch (mMovementMode) {
                case FREEFLY:
                    mMovementMode = DRONE;
                    break;
                case DRONE:
                    mMovementMode = FREEFLY;
                    break;
            }
        }

        if (e.button.button == SDL_BUTTON_RIGHT && e.type == SDL_MOUSEBUTTONDOWN) mRelativeMode = static_cast<SDL_bool>(!mRelativeMode);

        if (e.type == SDL_MOUSEMOTION && mRelativeMode) {
            mYaw += static_cast<float>(e.motion.xrel) / 200.f;
            mPitch -= static_cast<float>(e.motion.yrel) / 200.f;
        }

        if (e.type == SDL_MOUSEWHEEL) {
            mSpeed += static_cast<float>(e.wheel.y) * 0.2f;
            mSpeed = std::clamp(mSpeed, 0.f, MAX_CAMERA_SPEED);
        }
    });
}

void Camera::initBuffers() {
    const vk::DeviceSize frustumBufferSize = sizeof(Plane) * 6;
    mFrustumBuffer = mRenderer->mResources.createBuffer(
        frustumBufferSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );
    mRenderer->mCore.labelResourceDebug(mFrustumBuffer.buffer, "FrustumBuffer");
    LOG_INFO(mRenderer->mLogger, "Frustum Buffer Created");
}

glm::mat4 Camera::getViewMatrix() const {
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), mPosition);
    const glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::quat Camera::getPitchMatrix() const { return glm::angleAxis(mPitch, glm::vec3{1.f, 0.f, 0.f}); }

glm::quat Camera::getYawMatrix() const {
    return glm::angleAxis(mYaw, glm::vec3{0.f, -1.f, 0.f});  // Negative Y to flip OpenGL rotation?;
}

glm::mat4 Camera::getRotationMatrix() const {
    const glm::quat pitchRotation = getPitchMatrix();
    const glm::quat yawRotation = getYawMatrix();
    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

glm::vec3 Camera::getDirectionVector() const {
    glm::mat4 rot = getRotationMatrix();
    glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0, 0, -1, 0)));
    return forward;
}

void Camera::uploadFrameFrustum() const {
    glm::mat4 rot = getRotationMatrix();
    glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0, 0, -1, 0)));
    glm::vec3 right = glm::normalize(glm::vec3(rot * glm::vec4(1, 0, 0, 0)));
    glm::vec3 up = glm::normalize(glm::vec3(rot * glm::vec4(0, 1, 0, 0)));

    const float halfVSide = std::tanf(glm::radians(FOVY) * .5f);
    const float halfHSide = halfVSide * mRenderer->mCore.mAspectRatio;

    std::array<Plane, 6> planes;
    planes[FRUSTUM_NEAR_FACE] = Plane(forward, mPosition + forward * NEAR_PLANE);
    planes[FRUSTUM_FAR_FACE] = Plane(-forward, mPosition + forward * FAR_PLANE);
    planes[FRUSTUM_LEFT_FACE] = Plane(glm::cross(up, forward + right * halfHSide), mPosition);
    planes[FRUSTUM_RIGHT_FACE] = Plane(glm::cross(forward - right * halfHSide, up), mPosition);
    planes[FRUSTUM_TOP_FACE] = Plane(glm::cross(right, forward - up * halfVSide), mPosition);
    planes[FRUSTUM_BOTTOM_FACE] = Plane(glm::cross(forward + up * halfVSide, right), mPosition);
    // Cross product between slanted vectors and up / right vectors gives plane normals pointing inward.
    // Planes stretch indefinitely. Left, right, top, bottom planes all pass through camera position. Near and far calculate with normal * distance.

    std::memcpy(mFrustumBuffer.info.pMappedData, planes.data(), FRUSTUM_NUM_PLANES * sizeof(Plane));
}

void Camera::update(float deltaTime, float expectedDeltaTime) {
    switch (mMovementMode) {
        case FREEFLY:
            mPosition += glm::vec3(getYawMatrix() * glm::vec4(mVelocity * mSpeed * (deltaTime / expectedDeltaTime), 0.f));
            break;
        case DRONE:
            mPosition += glm::vec3(getRotationMatrix() * glm::vec4(mVelocity * mSpeed * (deltaTime / expectedDeltaTime), 0.f));
            break;
    }
}

void Camera::cleanup() { mFrustumBuffer.cleanup(); }