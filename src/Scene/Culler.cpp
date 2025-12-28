#include <Renderer/Renderer.h>
#include <Scene/Culler.h>
#include <quill/LogMacros.h>

Culler::Culler(Renderer* renderer)
    : mRenderer(renderer),
      mResetPipelineLayout(nullptr),
      mCullPipelineLayout(nullptr)
{}

void Culler::init() {
    initResetPipeline();
    initCullPipeline();
}

void Culler::initResetPipeline() {
    vk::PushConstantRange resetPushConstantRange{};
    resetPushConstantRange.offset = 0;
    resetPushConstantRange.size = sizeof(CullerResetPushConstants);
    resetPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::PipelineLayoutCreateInfo resetLayoutInfo{};
    resetLayoutInfo.setLayoutCount = 0;
    resetLayoutInfo.pSetLayouts = nullptr;
    resetLayoutInfo.pPushConstantRanges = &resetPushConstantRange;
    resetLayoutInfo.pushConstantRangeCount = 1;

    mResetPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(resetLayoutInfo);
    mRenderer->mCore.labelResourceDebug(mResetPipelineLayout, "CullerResetPipelineLayout");
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Layout Created");

    vk::ShaderModule computeShaderModule = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "CullerReset.comp.spv");

    ComputePipelineBuilder resetPipelineBuilder;
    resetPipelineBuilder.setShader(computeShaderModule);
    resetPipelineBuilder.mPipelineLayout = *mResetPipelineLayout;

    mResetPipelineBundle =
        PipelineBundle(mRenderer->mInfrastructure.mLatestPipelineId++, resetPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice), *mResetPipelineLayout);
    mRenderer->mCore.labelResourceDebug(mResetPipelineBundle.pipeline, "CullerResetPipeline");
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Created");
}

void Culler::initCullPipeline() {
    vk::PushConstantRange cullPushConstantRange{};
    cullPushConstantRange.offset = 0;
    cullPushConstantRange.size = sizeof(CullerCullPushConstants);
    cullPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::PipelineLayoutCreateInfo cullLayoutInfo{};
    cullLayoutInfo.setLayoutCount = 0;
    cullLayoutInfo.pSetLayouts = nullptr;
    cullLayoutInfo.pPushConstantRanges = &cullPushConstantRange;
    cullLayoutInfo.pushConstantRangeCount = 1;

    mCullPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(cullLayoutInfo);
    mRenderer->mCore.labelResourceDebug(mCullPipelineLayout, "CullerCullPipelineLayout");
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Layout Created");

    vk::ShaderModule computeShaderModule = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "CullerCull.comp.spv");

    ComputePipelineBuilder cullPipelineBuilder;
    cullPipelineBuilder.setShader(computeShaderModule);
    cullPipelineBuilder.mPipelineLayout = *mCullPipelineLayout;

    mCullPipelineBundle =
        PipelineBundle(mRenderer->mInfrastructure.mLatestPipelineId++, cullPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice), *mCullPipelineLayout);
    mRenderer->mCore.labelResourceDebug(mCullPipelineBundle.pipeline, "CullerCullPipeline");
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Created");
}

void Culler::cleanup() {
    mCullPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Destroyed");
    mCullPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Layout Destroyed");
    mResetPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Destroyed");
    mResetPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Layout Destroyed");
}
