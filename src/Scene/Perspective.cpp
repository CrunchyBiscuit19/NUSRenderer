#include <Renderer/Renderer.h>
#include <Scene/Perspective.h>
#include <quill/LogMacros.h>

Perspective::Perspective(Renderer* renderer) : mRenderer(renderer), mDescriptorSetLayout(nullptr) {}

void Perspective::init() {
    initData();
    initDescriptor();
}

void Perspective::initData() {
    mData.ambientColor = glm::vec4(.1f);
    mData.sunlightColor = glm::vec4(1.f);
    mData.sunlightDirection = glm::vec4(0.f, 1.f, 0.5f, 1.f);
}

void Perspective::initDescriptor() {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
    mDescriptorSetLayout = builder.build(mRenderer->mCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    mRenderer->mCore.labelResourceDebug(mDescriptorSetLayout, "PerspectiveDescriptorSetLayout");
    LOG_INFO(mRenderer->mLogger, "Perspective Descriptor Layout Created");
}

void Perspective::update() {
    mRenderer->mCamera.update(mRenderer->mStats.mFrameTime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
    mData.view = mRenderer->mCamera.getViewMatrix();
    mData.proj = glm::perspective(
        glm::radians(FOVY),
        mRenderer->mCore.mAspectRatio,
        FAR_PLANE,
        NEAR_PLANE
    );
    mData.proj[1][1] *= -1;

    auto* sceneBufferPtr = static_cast<PerspectiveData*>(mRenderer->mInfrastructure.getCurrentFrame().mPerspectiveBuffer.info.pMappedData);
    std::memcpy(sceneBufferPtr, &mData, 1 * sizeof(PerspectiveData));
}

void Perspective::cleanup() {
    mDescriptorSetLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Perspective Descriptor Layout Destroyed");
}
