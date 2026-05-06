#include <Data/Material.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>
#include <Utils/Pipeline.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

std::unordered_map<PipelineOptions, PipelineBundle> PbrMaterial::mPipelinesCache = {};
vk::raii::PipelineLayout PbrMaterial::mOpaquePipelineLayout = nullptr;
vk::raii::PipelineLayout PbrMaterial::mTransparentPipelineLayout = nullptr;
vk::raii::DescriptorSetLayout PbrMaterial::mDrawImageDescriptorSetLayout = nullptr;
vk::raii::DescriptorSet PbrMaterial::mDrawImageDescriptorSet = nullptr;

PbrMaterial::PbrMaterial(Renderer* renderer) : mRenderer(renderer), mRelativeMaterialIndex(0), mPipelineBundle(nullptr), mConstantsBufferOffset(0) {}

void PbrMaterial::init(Renderer* renderer) {
    initMaterialPipelineLayout(renderer);
}

void PbrMaterial::initMaterialPipelineLayout(Renderer* renderer) {
    vk::PushConstantRange materialPushConstantRange{};
    materialPushConstantRange.offset = 0;
    materialPushConstantRange.size = sizeof(GeometryPushConstants);
    materialPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::array<vk::DescriptorSetLayout, 2> materialDescriptorLayouts = {
        *renderer->mScene.mPerspective.mDescriptorSetLayout, 
        *renderer->mScene.mMainMaterialResourcesDescriptorSetLayout
    };
    vk::PipelineLayoutCreateInfo materialPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
    materialPipelineLayoutCreateInfo.pSetLayouts = materialDescriptorLayouts.data();
    materialPipelineLayoutCreateInfo.setLayoutCount = materialDescriptorLayouts.size();
    materialPipelineLayoutCreateInfo.pPushConstantRanges = &materialPushConstantRange;
    materialPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    mOpaquePipelineLayout = renderer->mCore.mDevice.createPipelineLayout(materialPipelineLayoutCreateInfo);
    renderer->mCore.labelResourceDebug(mOpaquePipelineLayout, "OpaqueMaterialPipelineLayout");
    LOG_INFO(renderer->mLogger, "Opaque Material Pipeline Layout Created");

    mTransparentPipelineLayout = renderer->mCore.mDevice.createPipelineLayout(materialPipelineLayoutCreateInfo);
    renderer->mCore.labelResourceDebug(mTransparentPipelineLayout, "TransparentMaterialPipelineLayout");
    LOG_INFO(renderer->mLogger, "Transparent Material Pipeline Layout Created");
}

void PbrMaterial::getMaterialPipeline() {
    PipelineOptions materialPipelineOptions{mPbrData.doubleSided, mPbrData.alphaMode};
    if (auto it = mPipelinesCache.find(materialPipelineOptions); it != mPipelinesCache.end()) {
        mPipelineBundle = &it->second;
        return;
    }
    createMaterialPipeline(materialPipelineOptions);
    mPipelineBundle = &mPipelinesCache.at(materialPipelineOptions);
}

void PbrMaterial::createMaterialPipeline(PipelineOptions materialPipelineOptions) const {
    vk::CullModeFlags cullMode;
    (materialPipelineOptions.doubleSided) ? (cullMode = vk::CullModeFlagBits::eNone) : (cullMode = vk::CullModeFlagBits::eBack);
    bool opaque = materialPipelineOptions.alphaMode != fastgltf::AlphaMode::Blend;

    vk::ShaderModule vertexShader = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "Geometry.vert.spv");
    vk::ShaderModule fragShader =
        mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / (opaque ? "GeometryOpaque.frag.spv" : "GeometryTransparent.frag.spv"));

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState accumBlendState{};
    accumBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    accumBlendState.blendEnable = VK_TRUE;
    accumBlendState.srcColorBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.dstColorBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.colorBlendOp = vk::BlendOp::eAdd;
    accumBlendState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    accumBlendState.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendAttachmentState rvlBlendState{};
    rvlBlendState.colorWriteMask = vk::ColorComponentFlagBits::eR;
    rvlBlendState.blendEnable = VK_TRUE;
    rvlBlendState.srcColorBlendFactor = vk::BlendFactor::eZero;
    rvlBlendState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
    rvlBlendState.colorBlendOp = vk::BlendOp::eAdd;
    rvlBlendState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    rvlBlendState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    rvlBlendState.alphaBlendOp = vk::BlendOp::eAdd;

    GraphicsPipelineBuilder materialPipelineBuilder;
    materialPipelineBuilder.setShaders(vertexShader, fragShader);
    materialPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    materialPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
    materialPipelineBuilder.setCullMode(cullMode, vk::FrontFace::eCounterClockwise);
    MSAA_ENABLE ? materialPipelineBuilder.enableMultisampling() : materialPipelineBuilder.disableMultisampling();
    MSAA_ENABLE ? materialPipelineBuilder.enableSampleShading() : materialPipelineBuilder.disableSampleShading();
    materialPipelineBuilder.enableDepthTest(opaque, vk::CompareOp::eGreaterOrEqual);
    if (opaque) {
        materialPipelineBuilder.addColorAttachment(mRenderer->mInfrastructure.mDrawImage.format, noBlendState);
        materialPipelineBuilder.mPipelineLayout = *mOpaquePipelineLayout;
    } else {
        materialPipelineBuilder.addColorAttachment(mRenderer->mScene.mTransparency.mAccumImage.format, accumBlendState);
        materialPipelineBuilder.addColorAttachment(mRenderer->mScene.mTransparency.mRevealageImage.format, rvlBlendState);
        materialPipelineBuilder.mPipelineLayout = *mTransparentPipelineLayout;
    }
    materialPipelineBuilder.setDepthFormat(mRenderer->mInfrastructure.mDepthImage.format);

    auto [it, _] = mPipelinesCache.try_emplace(
        materialPipelineOptions,
        mRenderer->mInfrastructure.mLatestPipelineId,
        materialPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice),
        (opaque ? *mOpaquePipelineLayout : *mTransparentPipelineLayout)
    );
    mRenderer->mCore.labelResourceDebug(it->second.pipeline, fmt::format("MaterialPipeline{}", mRenderer->mInfrastructure.mLatestPipelineId).c_str());
    LOG_INFO(mRenderer->mLogger, "{}", fmt::format("Material Pipeline {} Created", mRenderer->mInfrastructure.mLatestPipelineId).c_str());

    mRenderer->mInfrastructure.mLatestPipelineId++;
}

void PbrMaterial::cleanup(Renderer* renderer) {
    mPipelinesCache.clear();
    LOG_INFO(renderer->mLogger, "All Material Pipelines Destroyed");
    mOpaquePipelineLayout.clear();
    mTransparentPipelineLayout.clear();
    LOG_INFO(renderer->mLogger, "Material Pipeline Layouts Destroyed");
    mDrawImageDescriptorSetLayout.clear();
    LOG_INFO(renderer->mLogger, "Draw Image Descriptor Set Layout Destroyed");
    mDrawImageDescriptorSet.clear();
    LOG_INFO(renderer->mLogger, "Draw Image Descriptor Set Destroyed");
}
