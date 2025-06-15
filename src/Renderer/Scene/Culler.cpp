#include <Renderer/Scene/Culler.h>
#include <Renderer/Renderer.h>

Culler::Culler() :
	mPipelineLayout(nullptr)
{}

void Culler::init()
{
	initPipelineLayout();
	initPipeline();
}

void Culler::initPipelineLayout()
{
	vk::PushConstantRange cullPushConstantRange{};
	cullPushConstantRange.offset = 0;
	cullPushConstantRange.size = sizeof(CullPushConstants);
	cullPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

	vk::PipelineLayoutCreateInfo cullLayoutInfo{};
	cullLayoutInfo.setLayoutCount = 0;
	cullLayoutInfo.pSetLayouts = nullptr;
	cullLayoutInfo.pPushConstantRanges = &cullPushConstantRange;
	cullLayoutInfo.pushConstantRangeCount = 1;

	mPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(cullLayoutInfo);
	mRenderer->mRendererCore.labelResourceDebug(mPipelineLayout, "CullPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Layout Created");
}

void Culler::initPipeline()
{
	vk::ShaderModule computeShaderModule = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "cull/cull.comp.spv");

	ComputePipelineBuilder cullPipelineBuilder;
	cullPipelineBuilder.setShader(computeShaderModule);
	cullPipelineBuilder.mPipelineLayout = *mPipelineLayout;

	mPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId,
		cullPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
		*mPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mPipelineBundle.pipeline, "CullPipeline");
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Created");

	mRenderer->mRendererInfrastructure.mLatestPipelineId++;
}

void Culler::cleanup()
{
	mPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Destroyed");
	mPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Layout Destroyed");
}