#include <Renderer/Renderer.h>
#include <Scene/Transparency.h>
#include <quill/LogMacros.h>

Transparency::Transparency(Renderer* renderer) : mRenderer(renderer), mPipelineLayout(nullptr), mDescriptorSet(nullptr), mDescriptorSetLayout(nullptr) {}

void Transparency::init() {
    initImages();
    initDescriptors();
    writeDescriptors();
    initPipeline();
}

void Transparency::initImages() {
    mAccumImage = mRenderer->mResources.createImage(
        mRenderer->mInfrastructure.mDrawImage.imageExtent,
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        false,
        true
    );
    mRenderer->mCore.labelResourceDebug(mAccumImage.image, "AccumImage");
    mRenderer->mCore.labelResourceDebug(mAccumImage.imageView, "AccumImageView");
    LOG_INFO(mRenderer->mLogger, "Accumulation Image and Image View Created");

    mRevealageImage = mRenderer->mResources.createImage(
        mRenderer->mInfrastructure.mDrawImage.imageExtent,
        vk::Format::eR16Unorm,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        false,
        true
    );
    mRenderer->mCore.labelResourceDebug(mRevealageImage.image, "RevealageImage");
    mRenderer->mCore.labelResourceDebug(mRevealageImage.imageView, "RevealageImageView");
    LOG_INFO(mRenderer->mLogger, "Revealage Image and Image View Created");

    mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd) {
        vkhelper::transitionImage(
            cmd,
            *mAccumImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite
        );
        vkhelper::transitionImage(
            cmd,
            *mRevealageImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite
        );
    });
}

void Transparency::initDescriptors() {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eSampledImage);
    builder.addBinding(1, vk::DescriptorType::eSampledImage);
    mDescriptorSetLayout = builder.build(mRenderer->mCore.mDevice, vk::ShaderStageFlagBits::eFragment);
    mRenderer->mCore.labelResourceDebug(mDescriptorSetLayout, "CompositeDescriptorSetLayout");
    mDescriptorSet = mRenderer->mInfrastructure.mMainDescriptorAllocator.allocate(*mDescriptorSetLayout);
    mRenderer->mCore.labelResourceDebug(mDescriptorSet, "CompositeDescriptorSet");
    LOG_INFO(mRenderer->mLogger, "Composite Descriptor Set and Layout Created");
}

void Transparency::writeDescriptors() {
    DescriptorSetWriter writer;

    writer.writeImage(0, *mAccumImage.imageView, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage);
    writer.writeImage(1, *mRevealageImage.imageView, nullptr, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eSampledImage);

    writer.updateSetBindings(mRenderer->mCore.mDevice, *mDescriptorSet);
}

void Transparency::initPipeline() {
    vk::DescriptorSetLayout transparencyDescriptorLayouts = mDescriptorSetLayout;
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
    pipelineLayoutCreateInfo.pSetLayouts = &transparencyDescriptorLayouts;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

    mPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);
    mRenderer->mCore.labelResourceDebug(mPipelineLayout, "CompositePipelineLayout");
    LOG_INFO(mRenderer->mLogger, "Composite Pipeline Layout Created");

    vk::ShaderModule fragShader = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "Composite.frag.spv");
    vk::ShaderModule vertexShader = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "Composite.vert.spv");

    vk::PipelineColorBlendAttachmentState compositeBlendAttachment{};
    compositeBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    compositeBlendAttachment.blendEnable = VK_TRUE;
    compositeBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    compositeBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    compositeBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    compositeBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    compositeBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    compositeBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    GraphicsPipelineBuilder transparencyPipelineBuilder;
    transparencyPipelineBuilder.setShaders(vertexShader, fragShader);
    transparencyPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    transparencyPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
    transparencyPipelineBuilder.setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise);
    MSAA_ENABLE ? transparencyPipelineBuilder.enableMultisampling() : transparencyPipelineBuilder.disableMultisampling();
    MSAA_ENABLE ? transparencyPipelineBuilder.enableSampleShading() : transparencyPipelineBuilder.disableSampleShading();
    transparencyPipelineBuilder.addColorAttachment(mRenderer->mInfrastructure.mDrawImage.imageFormat, compositeBlendAttachment);
    transparencyPipelineBuilder.setDepthFormat(mRenderer->mInfrastructure.mDepthImage.imageFormat);
    transparencyPipelineBuilder.disableDepthTest();
    transparencyPipelineBuilder.mPipelineLayout = *mPipelineLayout;

    mPipelineBundle =
        PipelineBundle(mRenderer->mInfrastructure.mLatestPipelineId, transparencyPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice), *mPipelineLayout);
    mRenderer->mCore.labelResourceDebug(mPipelineBundle.pipeline, "TransparencyPipeline");
    LOG_INFO(mRenderer->mLogger, "Transparency Pipeline Created");

    mRenderer->mInfrastructure.mLatestPipelineId++;
}

void Transparency::resizeImages() {
    mAccumImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Accumulation Image and Image View Destroyed");
    mRevealageImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Revealage Image and Image View Destroyed");

    initImages();
    writeDescriptors();

    LOG_INFO(mRenderer->mLogger, "Transparency Images Resized");
}

void Transparency::cleanup() {
    mAccumImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Accumulation Image and Image View Destroyed");
    mRevealageImage.cleanup();
    LOG_INFO(mRenderer->mLogger, "Revealage Image and Image View Destroyed");
    mPipelineBundle.cleanup();
    LOG_INFO(mRenderer->mLogger, "Transparency Pipeline Destroyed");
    mPipelineLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Transparency Pipeline Layout Destroyed");
    mDescriptorSetLayout.clear();
    LOG_INFO(mRenderer->mLogger, "Transparency Descriptor Set Layout Destroyed");
    mDescriptorSet.clear();
    LOG_INFO(mRenderer->mLogger, "Transparency Descriptor Set Destroyed");
}