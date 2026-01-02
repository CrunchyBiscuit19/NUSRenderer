#include <Renderer/Renderer.h>
#include <Scene/Culler.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

Culler::Culler(Renderer* renderer)
    : mRenderer(renderer),
      mResetPipelineLayout(nullptr),
      mCullPipelineLayout(nullptr),
      mCompactPipelineLayout(nullptr),
      mDepthPyramidPipelineLayout(nullptr),
      mDepthPyramidDescriptorSetLayout(nullptr),
      mDepthPyramidDescriptorSet(nullptr) {}

void Culler::init() {
    initDepthPyramidImage();
    initDepthPyramidSampler();
    initDepthPyramidDescriptor();
    writeDepthPyramidDescriptor();
    initDepthPyramidPipeline();
    initResetPipeline();
    initCullPipeline();
    initCompactPipeline();
}

void Culler::initDepthPyramidImage() {
    // Depth Pyramid Image
    u32 depthPyramidWidth = vkhelper::previousPow2(mRenderer->mInfrastructure.mDrawImage.imageExtent.width);
    u32 depthPyramidHeight = vkhelper::previousPow2(mRenderer->mInfrastructure.mDrawImage.imageExtent.height);
    mDepthPyramidExtent = vk::Extent3D{
        depthPyramidWidth,
        depthPyramidHeight,
        1,
    };
    mDepthPyramidLevels = vkhelper::getMipMapLevelsDepthPyramid(mDepthPyramidExtent);
    mDepthPyramidImage = mRenderer->mResources.createImage(
        mDepthPyramidExtent,
        vk::Format::eR32Sfloat,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
        true
    );
    mRenderer->mCore.labelResourceDebug(mDepthPyramidImage.image, "CullerDepthPyramidImage");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image Created");
    mRenderer->mCore.labelResourceDebug(mDepthPyramidImage.imageView, "CullerDepthPyramidImageView");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image View Created");
    mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd) {
        vkhelper::transitionImage(
            cmd,
            *mDepthPyramidImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );
    });

    // Depth Pyramid Image Views
    mDepthPyramidMipViews.clear();
    mDepthPyramidMipViews.reserve(mDepthPyramidLevels);
    for (u32 i = 0; i < mDepthPyramidLevels; i++) {
        vk::ImageViewCreateInfo levelInfo =
            vkhelper::imageViewCreateInfo(mDepthPyramidImage.imageFormat, mDepthPyramidImage.image, vk::ImageAspectFlagBits::eColor);
        levelInfo.subresourceRange.levelCount = 1;
        levelInfo.subresourceRange.baseMipLevel = i;
        mDepthPyramidMipViews.emplace_back(std::move(mRenderer->mCore.mDevice.createImageView(levelInfo)));
        mRenderer->mCore.labelResourceDebug(mDepthPyramidMipViews.back(), fmt::format("CullerDepthPyramidMipView{}", i).c_str());
    }
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Mip Views Created");
}

void Culler::initDepthPyramidSampler() {
    // Depth Pyramid Sampler
    vk::SamplerReductionModeCreateInfo reductionModeInfo{};
    reductionModeInfo.reductionMode = vk::SamplerReductionMode::eMin;
    vk::SamplerCreateInfo depthSamplerCreateInfo{};
    depthSamplerCreateInfo.magFilter = vk::Filter::eLinear;
    depthSamplerCreateInfo.minFilter = vk::Filter::eLinear;
    depthSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    depthSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    depthSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    depthSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    depthSamplerCreateInfo.pNext = &reductionModeInfo;
    mDepthPyramidSampler = mRenderer->mResources.getSampler(depthSamplerCreateInfo);
    mRenderer->mCore.labelResourceDebug(mDepthPyramidSampler, "CullerDepthPyramidSampler");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Sampler Created");
}

void Culler::initDepthPyramidDescriptor() {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eSampledImage); // Depth Image
    builder.addBinding(1, vk::DescriptorType::eSampledImage, MAX_DEPTH_PYRAMID_LEVELS); // Depth Pyramid Read
    builder.addBinding(2, vk::DescriptorType::eStorageImage, MAX_DEPTH_PYRAMID_LEVELS); // Depth Pyramid Write
    builder.addBinding(3, vk::DescriptorType::eSampler); // Depth Pyramid Sampler
    mDepthPyramidDescriptorSetLayout = builder.build(mRenderer->mCore.mDevice, vk::ShaderStageFlagBits::eCompute);
    mRenderer->mCore.labelResourceDebug(mDepthPyramidDescriptorSetLayout, "CullerDepthPyramidDescriptorSetLayout");
    mDepthPyramidDescriptorSet = mRenderer->mInfrastructure.mMainDescriptorAllocator.allocate(*mDepthPyramidDescriptorSetLayout);
    mRenderer->mCore.labelResourceDebug(mDepthPyramidDescriptorSet, "CullerDepthPyramidDescriptorSet");
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Descriptor Set and Layout Created");
}

void Culler::writeDepthPyramidDescriptor() {
    DescriptorSetBinder writer;

    writer.bindImage(0, *mRenderer->mInfrastructure.mDepthImage.imageView, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage);

    for (u32 i = 0; i < mDepthPyramidLevels; i++) {
        writer.bindImageArray(1, i, *mDepthPyramidMipViews[i], nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eSampledImage);
        writer.bindImageArray(2, i, *mDepthPyramidMipViews[i], nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
    }
    
    writer.bindSampler(3, mDepthPyramidSampler, vk::DescriptorType::eSampler);
    
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

void Culler::reconstructDepthPyramid() {
    mDepthPyramidMipViews.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Mip Views Destroyed");
    mDepthPyramidImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Depth Pyramid Image Destroyed");

    initDepthPyramidImage();
    writeDepthPyramidDescriptor();

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
    mCullPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Destroyed");
    mCullPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Cull Pipeline Layout Destroyed");
    mResetPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Destroyed");
    mResetPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Culler Reset Pipeline Layout Destroyed");
}
