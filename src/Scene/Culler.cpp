#include <Renderer/Renderer.h>
#include <Scene/Culler.h>
#include <quill/LogMacros.h>

Culler::Culler(Renderer* renderer) : mRenderer(renderer), mResetPipelineLayout(nullptr), mCullPipelineLayout(nullptr), mCompactPipelineLayout(nullptr) {}

void Culler::init() {
    initResetPipeline();
    initCullPipeline();
    initCompactPipeline();
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

void Culler::initCompactPipeline() {
    vk::PushConstantRange compactPushConstantRange{};
    compactPushConstantRange.offset = 0;
    compactPushConstantRange.size = sizeof(CullerCompactPushConstants);
    compactPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::PipelineLayoutCreateInfo compactLayoutInfo{};
    compactLayoutInfo.setLayoutCount = 0;
    compactLayoutInfo.pSetLayouts = nullptr;
    compactLayoutInfo.pPushConstantRanges = &compactPushConstantRange;
    compactLayoutInfo.pushConstantRangeCount = 1;

    mCompactPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(compactLayoutInfo);
    mRenderer->mCore.labelResourceDebug(mCompactPipelineLayout, "CullerCompactPipelineLayout");
    LOG_INFO(mRenderer->mLogger, "Culler Compact Pipeline Layout Created");

    vk::ShaderModule computeShaderModule = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "CullerCompact.comp.spv");

    ComputePipelineBuilder compactPipelineBuilder;
    compactPipelineBuilder.setShader(computeShaderModule);
    compactPipelineBuilder.mPipelineLayout = *mCompactPipelineLayout;

    mCompactPipelineBundle =
        PipelineBundle(mRenderer->mInfrastructure.mLatestPipelineId++, compactPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice), *mCompactPipelineLayout);
    mRenderer->mCore.labelResourceDebug(mCompactPipelineBundle.pipeline, "CullerCompactPipeline");
    LOG_INFO(mRenderer->mLogger, "Culler Compact Pipeline Created");
}

void Culler::cleanup() {
    mCompactPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Compact Pipeline Destroyed");
    mCompactPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Compact Pipeline Layout Destroyed");
    mCullPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Destroyed");
    mCullPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Layout Destroyed");
    mResetPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Destroyed");
    mResetPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Layout Destroyed");
}
