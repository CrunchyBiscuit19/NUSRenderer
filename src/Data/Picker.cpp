#include <Data/Picker.h>
#include <Renderer/Renderer.h>
#include <Renderer/RendererScene.h>
#include <Utils/Helper.h>

Picker::Picker(Renderer* renderer) :
	mPickerPipelineLayout(nullptr)
{}

void Picker::init()
{
	initImage();
	initPickerPipelineLayout();
	initPickerPipeline();
}

void Picker::initImage()
{
	mPickerDrawImage = mRenderer->mRendererResources.createImage(
		vk::Extent3D{ mRenderer->mRendererCore.mWindowExtent, 1 },
		vk::Format::eR32G32B32Uint, // Model Id / Mesh Id / Instance Id
		vk::ImageUsageFlagBits::eColorAttachment 
	);
}

void Picker::initPickerPipelineLayout()
{
	vk::PushConstantRange pickerPushConstantRange{};
	pickerPushConstantRange.offset = 0;
	pickerPushConstantRange.size = sizeof(ScenePushConstants);
	pickerPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> pickerDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mPerspectiveDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pipelineLayoutCreateInfo.pSetLayouts = pickerDescriptorLayouts.data();
	pipelineLayoutCreateInfo.setLayoutCount = pickerDescriptorLayouts.size();
	pipelineLayoutCreateInfo.pPushConstantRanges = &pickerPushConstantRange;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mPickerPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mPickerPipelineLayout, "PickerPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Pipeline Layout Created");
}

void Picker::initPickerPipeline()
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
	pickerPipelineBuilder.setColorAttachmentFormat(mPickerDrawImage.imageFormat); 
	pickerPipelineBuilder.setDepthFormat(mRenderer->mRendererInfrastructure.mDepthImage.imageFormat);
	pickerPipelineBuilder.enableDepthtest(true, vk::CompareOp::eGreaterOrEqual);
	pickerPipelineBuilder.mPipelineLayout = *mPickerPipelineLayout;

	mPickerPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId,
		pickerPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
		*mPickerPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mPickerPipelineBundle.pipeline, "PickerPipeline");
	LOG_INFO(mRenderer->mLogger, "Picker Pipeline Created");

	mRenderer->mRendererInfrastructure.mLatestPipelineId++;
}

