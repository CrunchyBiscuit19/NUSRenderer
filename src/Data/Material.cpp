#include <Data/Material.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>
#include <Utils/Pipeline.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

std::unordered_map<PipelineOptions, PipelineBundle> PbrMaterial::mPipelinesCache = {};
vk::raii::PipelineLayout PbrMaterial::mPipelineLayout = nullptr;

PbrMaterial::PbrMaterial(Renderer* renderer) : mRenderer(renderer), mRelativeMaterialIndex(0), mPipelineBundle(nullptr), mConstantsBufferOffset(0) {}

void PbrMaterial::initMaterialPipelineLayout(Renderer* renderer) {
    vk::PushConstantRange materialPushConstantRange{};
    materialPushConstantRange.offset = 0;
    materialPushConstantRange.size = sizeof(GeometryPushConstants);
    materialPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector materialDescriptorLayouts = {*renderer->mScene.mPerspective.mDescriptorSetLayout, *renderer->mScene.mMainMaterialResourcesDescriptorSetLayout};

    vk::PipelineLayoutCreateInfo materialPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
    materialPipelineLayoutCreateInfo.pSetLayouts = materialDescriptorLayouts.data();
    materialPipelineLayoutCreateInfo.setLayoutCount = materialDescriptorLayouts.size();
    materialPipelineLayoutCreateInfo.pPushConstantRanges = &materialPushConstantRange;
    materialPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    mPipelineLayout = renderer->mCore.mDevice.createPipelineLayout(materialPipelineLayoutCreateInfo);
    renderer->mCore.labelResourceDebug(mPipelineLayout, "MaterialPipelineLayout");
    LOG_INFO(renderer->mLogger, "Material Pipeline Layout Created");
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
    vk::ShaderModule fragShader = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "Geometry.frag.spv");
    vk::ShaderModule vertexShader = mRenderer->mResources.getShader(std::filesystem::path(SHADERS_PATH) / "Geometry.vert.spv");

    vk::CullModeFlags cullMode;
    (materialPipelineOptions.doubleSided) ? (cullMode = vk::CullModeFlagBits::eNone) : (cullMode = vk::CullModeFlagBits::eBack);
    bool transparency = materialPipelineOptions.alphaMode == fastgltf::AlphaMode::Blend;

    vk::PipelineColorBlendAttachmentState noBlendState{};
    noBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    noBlendState.blendEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState opaqueBlendState{};
    opaqueBlendState.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
    opaqueBlendState.blendEnable = VK_TRUE;
    opaqueBlendState.srcColorBlendFactor = vk::BlendFactor::eZero;
    opaqueBlendState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
    opaqueBlendState.colorBlendOp = vk::BlendOp::eAdd;
    opaqueBlendState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    opaqueBlendState.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    opaqueBlendState.alphaBlendOp = vk::BlendOp::eAdd;

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
    rvlBlendState.srcAlphaBlendFactor = vk::BlendFactor::eZero;
    rvlBlendState.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    rvlBlendState.alphaBlendOp = vk::BlendOp::eAdd;

    GraphicsPipelineBuilder materialPipelineBuilder;
    materialPipelineBuilder.setShaders(vertexShader, fragShader);
    materialPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    materialPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
    materialPipelineBuilder.setCullMode(cullMode, vk::FrontFace::eCounterClockwise);
    MSAA_ENABLE ? materialPipelineBuilder.enableMultisampling() : materialPipelineBuilder.disableMultisampling();
    MSAA_ENABLE ? materialPipelineBuilder.enableSampleShading() : materialPipelineBuilder.disableSampleShading();
    materialPipelineBuilder.enableDepthTest(!transparency, vk::CompareOp::eGreaterOrEqual); 
    materialPipelineBuilder.addColorAttachment(mRenderer->mInfrastructure.mDrawImage.imageFormat, noBlendState);
    materialPipelineBuilder.setDepthFormat(mRenderer->mInfrastructure.mDepthImage.imageFormat);
    materialPipelineBuilder.mPipelineLayout = *mPipelineLayout;

    auto [it, _] = mPipelinesCache.try_emplace(
        materialPipelineOptions, mRenderer->mInfrastructure.mLatestPipelineId, materialPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice), *mPipelineLayout
    );
    mRenderer->mCore.labelResourceDebug(it->second.pipeline, fmt::format("MaterialPipeline{}", mRenderer->mInfrastructure.mLatestPipelineId).c_str());
    LOG_INFO(mRenderer->mLogger, "{}", fmt::format("Material Pipeline {} Created", mRenderer->mInfrastructure.mLatestPipelineId).c_str());

    mRenderer->mInfrastructure.mLatestPipelineId++;
}

void PbrMaterial::cleanup(Renderer* renderer) {
    mPipelinesCache.clear();
    LOG_INFO(renderer->mLogger, "All Material Pipelines Destroyed");
    mPipelineLayout.clear();
    LOG_INFO(renderer->mLogger, "Material Pipeline Layout Destroyed");
}
