#include <Scene/Culler.h>
#include <Renderer/Renderer.h>

#include <quill/LogMacros.h>

Culler::Culler(Renderer* renderer) :
	mRenderer(renderer),
	mPipelineLayout(nullptr)

{
}

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

	mPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(cullLayoutInfo);
	mRenderer->mCore.labelResourceDebug(mPipelineLayout, "CullPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Layout Created");
}

void Culler::initPipeline()
{
	vk::ShaderModule computeShaderModule = mRenderer->mResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "Cull.comp.spv");

	ComputePipelineBuilder cullPipelineBuilder;
	cullPipelineBuilder.setShader(computeShaderModule);
	cullPipelineBuilder.mPipelineLayout = *mPipelineLayout;

	mPipelineBundle = PipelineBundle(
		mRenderer->mInfrastructure.mLatestPipelineId++,
		cullPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice),
		*mPipelineLayout
	);
	mRenderer->mCore.labelResourceDebug(mPipelineBundle.pipeline, "CullPipeline");
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Created");
}

void Culler::cleanup()
{
	mPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Destroyed");
	mPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Layout Destroyed");
}
