#include <Renderer/Renderer.h>
#include <Scene/Culler.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

Culler::Culler(Renderer* renderer)
    : mRenderer(renderer),
      mResetPipelineLayout(nullptr),
      mCullDescriptorSetLayout(nullptr),
      mCullDescriptorSet(nullptr),
      mCullPipelineLayout(nullptr),
      mCompactPipelineLayout(nullptr),
      mDepthPyramidPipelineLayout(nullptr),
      mDepthPyramidDescriptorSetLayout(nullptr),
      mDepthPyramidDescriptorSet(nullptr) {}

void Culler::init() {
    initDepthPyramidImage();
    initDepthPyramidDescriptor();
    writeDepthPyramidDescriptor();
    initDepthPyramidPipeline();
    initDepthPyramidPushConstants();
    initResetPipeline();
    initCullDescriptor();
    writeCullDescriptor();
    initCullPipeline();
    initCullPushConstants();
    initCompactPipeline();
}

void Culler::initDepthPyramidImage() {
    // Depth Pyramid Image
    u32 depthPyramidWidth = vkhelper::previousPow2(mRenderer->mInfrastructure.mDepthImage.extent.width);
    u32 depthPyramidHeight = vkhelper::previousPow2(mRenderer->mInfrastructure.mDepthImage.extent.height);
    mDepthPyramidExtent = vk::Extent3D{
        depthPyramidWidth,
        depthPyramidHeight,
        1,
    };
    mDepthPyramidLevels = vkhelper::getMipMapLevelsDepthPyramid(mDepthPyramidExtent);
    mDepthPyramidImage = mRenderer->mResources.createImage(
        mDepthPyramidExtent, vk::Format::eR32Sfloat, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage, true
    );
    mRenderer->mCore.labelResourceDebug(mDepthPyramidImage.image, "CullerDepthPyramidImage");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image Created");
    mRenderer->mCore.labelResourceDebug(mDepthPyramidImage.view, "CullerDepthPyramidImageView");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image View Created");

    // Depth Pyramid Image Views
    mDepthPyramidMipViews.clear();
    mDepthPyramidMipViews.reserve(mDepthPyramidLevels);
    for (u32 i = 0; i < mDepthPyramidLevels; i++) {
        vk::ImageViewCreateInfo levelInfo = vkhelper::imageViewCreateInfo(mDepthPyramidImage.format, mDepthPyramidImage.image, vk::ImageAspectFlagBits::eColor);
        levelInfo.subresourceRange.levelCount = 1;
        levelInfo.subresourceRange.baseMipLevel = i;
        mDepthPyramidMipViews.emplace_back(std::move(mRenderer->mCore.mDevice.createImageView(levelInfo)));
        mRenderer->mCore.labelResourceDebug(mDepthPyramidMipViews.back(), fmt::format("CullerDepthPyramidMipView{}", i).c_str());
    }
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Mip Views Created");

    // Transition images
    mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd) {
        mDepthPyramidImage.transition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );
    });
}

void Culler::initDepthPyramidDescriptor() {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eSampledImage);                            // Depth Image
    builder.addBinding(1, vk::DescriptorType::eSampledImage, MAX_DEPTH_PYRAMID_LEVELS);  // Depth Pyramid Read
    builder.addBinding(2, vk::DescriptorType::eStorageImage, MAX_DEPTH_PYRAMID_LEVELS);  // Depth Pyramid Write
    mDepthPyramidDescriptorSetLayout = builder.build(mRenderer->mCore.mDevice, vk::ShaderStageFlagBits::eCompute);
    mRenderer->mCore.labelResourceDebug(mDepthPyramidDescriptorSetLayout, "CullerDepthPyramidDescriptorSetLayout");

    mDepthPyramidDescriptorSet = mRenderer->mInfrastructure.mMainDescriptorAllocator.allocate(*mDepthPyramidDescriptorSetLayout);
    mRenderer->mCore.labelResourceDebug(mDepthPyramidDescriptorSet, "CullerDepthPyramidDescriptorSet");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Descriptor Set and Layout Created");
}

void Culler::writeDepthPyramidDescriptor() {
    DescriptorSetWriter writer;

    writer.writeImage(0, *mRenderer->mInfrastructure.mDepthImage.view, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage);
    for (u32 i = 0; i < mDepthPyramidLevels; i++) {
        writer.writeImageArray(1, i, *mDepthPyramidMipViews[i], nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eSampledImage);
        writer.writeImageArray(2, i, *mDepthPyramidMipViews[i], nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
    }

    writer.updateSetBindings(mRenderer->mCore.mDevice, *mDepthPyramidDescriptorSet);
}

void Culler::initDepthPyramidPipeline() {
    vk::PushConstantRange depthPyramidPushConstantRange{};
    depthPyramidPushConstantRange.offset = 0;
    depthPyramidPushConstantRange.size = sizeof(CullerDepthPyramidPushConstants);
    depthPyramidPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::PipelineLayoutCreateInfo depthPyramidPipelineLayoutInfo{};
    depthPyramidPipelineLayoutInfo.setLayoutCount = 1;
    depthPyramidPipelineLayoutInfo.pSetLayouts = &(*mDepthPyramidDescriptorSetLayout);
    depthPyramidPipelineLayoutInfo.pPushConstantRanges = &depthPyramidPushConstantRange;
    depthPyramidPipelineLayoutInfo.pushConstantRangeCount = 1;

    mDepthPyramidPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(depthPyramidPipelineLayoutInfo);
    mRenderer->mCore.labelResourceDebug(mDepthPyramidPipelineLayout, "CullerDepthPyramidPipelineLayout");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Pipeline Layout Created");

    vk::ShaderModule computeShaderModule = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "CullerDepthPyramid.comp.spv");

    ComputePipelineBuilder depthPyramidPipelineBuilder;
    depthPyramidPipelineBuilder.setShader(computeShaderModule);
    depthPyramidPipelineBuilder.mPipelineLayout = *mDepthPyramidPipelineLayout;

    mDepthPyramidPipelineBundle = PipelineBundle(
        mRenderer->mInfrastructure.mLatestPipelineId++, depthPyramidPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice), *mDepthPyramidPipelineLayout
    );
    mRenderer->mCore.labelResourceDebug(mDepthPyramidPipelineBundle.pipeline, "CullerDepthPyramidPipeline");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Pipeline Created");
}

void Culler::initDepthPyramidPushConstants() {
    vk::Extent3D depthPyramidExtent = mDepthPyramidImage.extent;
    vk::Extent3D depthFullExtent = mRenderer->mInfrastructure.mDepthImage.extent;
    mDepthPyramidPushConstants.depthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
    mDepthPyramidPushConstants.depthFullExtent = glm::uvec2(depthFullExtent.width, depthFullExtent.height);
    mDepthPyramidPushConstants.depthFullRatio =
        glm::vec2(depthPyramidExtent.width / static_cast<float>(depthFullExtent.width), depthPyramidExtent.height / static_cast<float>(depthFullExtent.height));
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

void Culler::initCullDescriptor() {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eSampledImage);  // Depth Image
    mCullDescriptorSetLayout = builder.build(mRenderer->mCore.mDevice, vk::ShaderStageFlagBits::eCompute);
    mRenderer->mCore.labelResourceDebug(mCullDescriptorSetLayout, "CullerCullDescriptorSetLayout");
    mCullDescriptorSet = mRenderer->mInfrastructure.mMainDescriptorAllocator.allocate(*mCullDescriptorSetLayout);
    mRenderer->mCore.labelResourceDebug(mCullDescriptorSet, "CullerCullDescriptorSet");
    LOG_INFO(mRenderer->mLogger, "Culler Cull Descriptor Set and Layout Created");
}

void Culler::writeCullDescriptor() {
    DescriptorSetWriter writer;

    writer.writeImage(0, *mDepthPyramidImage.view, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage);

    writer.updateSetBindings(mRenderer->mCore.mDevice, *mCullDescriptorSet);
}

void Culler::initCullPipeline() {
    vk::PushConstantRange cullPushConstantRange{};
    cullPushConstantRange.offset = 0;
    cullPushConstantRange.size = sizeof(CullerCullPushConstants);
    cullPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::PipelineLayoutCreateInfo cullLayoutInfo{};
    cullLayoutInfo.setLayoutCount = 1;
    cullLayoutInfo.pSetLayouts = &(*mCullDescriptorSetLayout);
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

void Culler::initCullPushConstants() {
    vk::Extent3D depthPyramidExtent = mDepthPyramidImage.extent;
    vk::Extent3D drawExtent = mRenderer->mInfrastructure.mDrawImage.extent;
    mCullPushConstants.renderInstancesCountBuffer = mRenderer->mStats.mRenderInstancesCountBuffer.address.value();
    mCullPushConstants.mainBoundsBuffer = mRenderer->mScene.mMainBoundsBuffer.address.value();
    mCullPushConstants.frustumBuffer = mRenderer->mCamera.mFrustumBuffer.address.value();
    mCullPushConstants.mainNodeTransformsBuffer = mRenderer->mScene.mMainNodeTransformsBuffer.address.value();
    mCullPushConstants.mainInstancesBuffer = mRenderer->mScene.mMainInstancesBuffer.address.value();
    mCullPushConstants.mainVisibleRenderInstancesInstanceIndexBuffer = mRenderer->mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.address.value();
    mCullPushConstants.drawExtents = glm::vec2(drawExtent.width, drawExtent.height);
    mCullPushConstants.depthPyramidExtents = glm::vec2(depthPyramidExtent.width, depthPyramidExtent.height);
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

void Culler::resizeCuller() {
    mDepthPyramidMipViews.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Mip Views Destroyed");
    mDepthPyramidImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image Destroyed");

    initDepthPyramidImage();
    writeDepthPyramidDescriptor();
    initDepthPyramidPushConstants();
    writeCullDescriptor();
    initCullPushConstants();

    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Reconstructed After Resize");
}

void Culler::cleanup() {
    mDepthPyramidMipViews.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Mip Views Destroyed");
    mDepthPyramidImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image Destroyed");
    mDepthPyramidDescriptorSet.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Descriptor Set Destroyed");
    mDepthPyramidDescriptorSetLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Descriptor Set Layout Destroyed");
    mDepthPyramidPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Pipeline Destroyed");
    mDepthPyramidPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Pipeline Layout Destroyed");
    mCompactPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Compact Pipeline Destroyed");
    mCompactPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Compact Pipeline Layout Destroyed");
    mCullDescriptorSet.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Descriptor Set Destroyed");
    mCullDescriptorSetLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Descriptor Set Layout Destroyed");
    mCullPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Destroyed");
    mCullPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Layout Destroyed");
    mResetPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Destroyed");
    mResetPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Layout Destroyed");
}
