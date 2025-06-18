#include <Renderer/Renderer.h>
#include <Utils/Helper.h>
#include <Utils/Pipeline.h>
#include <Data/Material.h>

#include <fmt/core.h>

std::unordered_map<PipelineOptions, PipelineBundle> PbrMaterial::mPipelinesCache = {};
vk::raii::PipelineLayout PbrMaterial::mPipelineLayout = nullptr;

PbrMaterial::PbrMaterial(Renderer* renderer) :
	mRenderer(renderer),
	mRelativeMaterialIndex(0),
	mPipelineBundle(nullptr),
	mConstantsBufferOffset(0)
{
}

void PbrMaterial::initMaterialPipelineLayout(Renderer* renderer)
{
	vk::PushConstantRange materialPushConstantRange{};
	materialPushConstantRange.offset = 0;
	materialPushConstantRange.size = sizeof(ScenePushConstants);
	materialPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> materialDescriptorLayouts = {
		*renderer->mRendererScene.mPerspective.mDescriptorSetLayout,
		*renderer->mRendererScene.mMainMaterialResourcesDescriptorSetLayout
	};

	vk::PipelineLayoutCreateInfo materialPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	materialPipelineLayoutCreateInfo.pSetLayouts = materialDescriptorLayouts.data();
	materialPipelineLayoutCreateInfo.setLayoutCount = materialDescriptorLayouts.size();
	materialPipelineLayoutCreateInfo.pPushConstantRanges = &materialPushConstantRange;
	materialPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mPipelineLayout = renderer->mRendererCore.mDevice.createPipelineLayout(materialPipelineLayoutCreateInfo);
	renderer->mRendererCore.labelResourceDebug(mPipelineLayout, "MaterialPipelineLayout");
	LOG_INFO(renderer->mLogger, "Material Pipeline Layout Created");
}

void PbrMaterial::getMaterialPipeline()
{
	PipelineOptions materialPipelineOptions{mPbrData.doubleSided, mPbrData.alphaMode};
	if (auto it = mPipelinesCache.find(materialPipelineOptions); it != mPipelinesCache.end())
	{
		mPipelineBundle = &it->second;
		return;
	}
	createMaterialPipeline(materialPipelineOptions);
	mPipelineBundle = &mPipelinesCache.at(materialPipelineOptions);
}

void PbrMaterial::createMaterialPipeline(PipelineOptions materialPipelineOptions)
{
	vk::ShaderModule fragShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "mesh/mesh.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "mesh/mesh.vert.spv");

	vk::CullModeFlags cullMode;
	(materialPipelineOptions.doubleSided)
		? (cullMode = vk::CullModeFlagBits::eNone)
		: (cullMode = vk::CullModeFlagBits::eBack);
	bool transparency;
	(materialPipelineOptions.alphaMode == fastgltf::AlphaMode::Blend) ? (transparency = true) : (transparency = false);

	GraphicsPipelineBuilder materialPipelineBuilder;
	materialPipelineBuilder.setShaders(vertexShader, fragShader);
	materialPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
	materialPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
	materialPipelineBuilder.setCullMode(cullMode, vk::FrontFace::eCounterClockwise);
	materialPipelineBuilder.enableMultisampling();
	materialPipelineBuilder.enableSampleShading();
	transparency ? materialPipelineBuilder.enableBlendingAlpha() : materialPipelineBuilder.disableBlending();
	//materialPipelineBuilder.enableDepthtest(!transparency, vk::CompareOp::eGreaterOrEqual); // TODO transparency
	materialPipelineBuilder.enableDepthTest(true, vk::CompareOp::eGreaterOrEqual);
	materialPipelineBuilder.setColorAttachmentFormat(mRenderer->mRendererInfrastructure.mDrawImage.imageFormat);
	materialPipelineBuilder.setDepthFormat(mRenderer->mRendererInfrastructure.mDepthImage.imageFormat);
	materialPipelineBuilder.mPipelineLayout = *mPipelineLayout;

	auto [it, _] = mPipelinesCache.try_emplace(materialPipelineOptions,
	                                           mRenderer->mRendererInfrastructure.mLatestPipelineId,
	                                           materialPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
	                                           *mPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(it->second.pipeline,
	                                            fmt::format("MaterialPipeline{}",
	                                                        mRenderer->mRendererInfrastructure.mLatestPipelineId).
	                                            c_str());
	LOG_INFO(mRenderer->mLogger, "{}",
	         fmt::format("Material Pipeline {} Created", mRenderer->mRendererInfrastructure.mLatestPipelineId).c_str());

	mRenderer->mRendererInfrastructure.mLatestPipelineId++;
}

void PbrMaterial::cleanup(Renderer* renderer)
{
	mPipelinesCache.clear();
	LOG_INFO(renderer->mLogger, "All Material Pipelines Destroyed");
	mPipelineLayout.clear();
	LOG_INFO(renderer->mLogger, "Material Pipeline Layout Destroyed");
}
