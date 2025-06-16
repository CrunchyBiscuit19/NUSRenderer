#include <Renderer/Scene/Picker.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

Picker::Picker(Renderer* renderer) :
	mRenderer(renderer),
	mPipelineLayout(nullptr)
{}

void Picker::init()
{
	initImage();
	initPipelineLayout();
	initPipeline();
}

void Picker::initImage()
{
	mObjectImage = mRenderer->mRendererResources.createImage(
		mRenderer->mRendererInfrastructure.mDrawImage.imageExtent,
		vk::Format::eR32G32B32A32Uint, // Model Id / Mesh Id / Instance Id
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	);
	mRenderer->mRendererCore.labelResourceDebug(mObjectImage.image, "PickerObjectImage");
	LOG_INFO(mRenderer->mLogger, "Picker Object Image Created");
	mRenderer->mRendererCore.labelResourceDebug(mObjectImage.imageView, "PickerObjectImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Object Image View Created");

	mDepthImage = mRenderer->mRendererResources.createImage(
		mObjectImage.imageExtent,
		vk::Format::eD32Sfloat, // Model Id / Mesh Id / Instance Id
		vk::ImageUsageFlagBits::eDepthStencilAttachment
	);
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.image, "PickerDepthImage");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image Created");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.imageView, "PickerDepthImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image View Created");
}

void Picker::initPipelineLayout()
{
	vk::PushConstantRange pickerPushConstantRange{};
	pickerPushConstantRange.offset = 0;
	pickerPushConstantRange.size = sizeof(ScenePushConstants);
	pickerPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> pickerDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pipelineLayoutCreateInfo.pSetLayouts = pickerDescriptorLayouts.data();
	pipelineLayoutCreateInfo.setLayoutCount = pickerDescriptorLayouts.size();
	pipelineLayoutCreateInfo.pPushConstantRanges = &pickerPushConstantRange;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mPipelineLayout, "PickerPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Pipeline Layout Created");
}

void Picker::initPipeline()
{
	vk::ShaderModule fragShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "picker/picker.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "picker/picker.vert.spv");

	GraphicsPipelineBuilder pickerPipelineBuilder;
	pickerPipelineBuilder.setShaders(vertexShader, fragShader);
	pickerPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
	pickerPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
	pickerPipelineBuilder.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise);
	pickerPipelineBuilder.disableMultisampling();
	pickerPipelineBuilder.disableSampleShading();
	pickerPipelineBuilder.disableBlending();
	pickerPipelineBuilder.setColorAttachmentFormat(mObjectImage.imageFormat); 
	pickerPipelineBuilder.setDepthFormat(mDepthImage.imageFormat);
	pickerPipelineBuilder.enableDepthTest(true, vk::CompareOp::eGreaterOrEqual);
	pickerPipelineBuilder.mPipelineLayout = *mPipelineLayout;

	mPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId,
		pickerPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
		*mPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mPipelineBundle.pipeline, "PickerPipeline");
	LOG_INFO(mRenderer->mLogger, "Picker Pipeline Created");

	mRenderer->mRendererInfrastructure.mLatestPipelineId++;
}

void Picker::cleanup()
{
	mObjectImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Object Image Destroyed");
	mDepthImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image Destroyed");
	mPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Pipeline Destroyed");
	mPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Picker Pipeline Layout Destroyed");
}