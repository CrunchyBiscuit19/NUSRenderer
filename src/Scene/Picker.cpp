#include <Scene/Picker.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <quill/LogMacros.h>
#include <imguizmo.h>
#include <glm/gtc/type_ptr.hpp>

Picker::Picker(Renderer* renderer) :
	mRenderer(renderer),
	mDrawPipelineLayout(nullptr),
	mPickPipelineLayout(nullptr),
	mDescriptorSet(nullptr),
	mDescriptorSetLayout(nullptr)
{}

void Picker::init()
{
	initBuffer();
	initImage();
	initDescriptor();
	initDrawPipeline();
	initPickPipeline();
	initDrawPushConstants();
	initPickPushConstants();
}

void Picker::initBuffer()
{
	mBuffer = mRenderer->mRendererResources.createBuffer(sizeof(PickerData),
		vk::BufferUsageFlagBits::eTransferDst |
		vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_CPU_TO_GPU);
	mRenderer->mRendererCore.labelResourceDebug(mBuffer.buffer, "PickerBuffer");
	LOG_INFO(mRenderer->mLogger, "Picker Buffer Created");
}

void Picker::initImage()
{
	mImage = mRenderer->mRendererResources.createImage(
		mRenderer->mRendererInfrastructure.mDrawImage.imageExtent,
		vk::Format::eR32G32Uint, // Model Id / Instance Id
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eStorage
	);
	mRenderer->mRendererCore.labelResourceDebug(mImage.image, "PickerDrawImage");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Image Created");
	mRenderer->mRendererCore.labelResourceDebug(mImage.imageView, "PickerDrawImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Image View Created");

	mDepthImage = mRenderer->mRendererResources.createImage(
		mImage.imageExtent,
		vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment
	);
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.image, "PickerDepthImage");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image Created");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.imageView, "PickerDepthImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image View Created");

	mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd) {
		vkhelper::transitionImage(cmd, *mImage.image,
			vk::PipelineStageFlagBits2::eNone,
			vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
		vkhelper::transitionImage(cmd, *mDepthImage.image,
			vk::PipelineStageFlagBits2::eNone,
			vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests,
			vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);
	});
}

void Picker::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eStorageImage);
	mDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eCompute);
	mDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mDescriptorSetLayout);
	LOG_INFO(mRenderer->mLogger, "Picker Descriptor Set and Layout Created");

	DescriptorSetBinder writer;
	writer.bindImage(0, *mImage.imageView, nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mDescriptorSet);
}

void Picker::initDrawPipeline()
{
	vk::PushConstantRange drawPushConstantRange{};
	drawPushConstantRange.offset = 0;
	drawPushConstantRange.size = sizeof(PickerDrawPushConstants);
	drawPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector drawDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo drawPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	drawPipelineLayoutCreateInfo.pSetLayouts = drawDescriptorLayouts.data();
	drawPipelineLayoutCreateInfo.setLayoutCount = drawDescriptorLayouts.size();
	drawPipelineLayoutCreateInfo.pPushConstantRanges = &drawPushConstantRange;
	drawPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mDrawPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(drawPipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mDrawPipelineLayout, "PickerDrawPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Pipeline Layout Created");

	vk::ShaderModule fragShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "Picker/PickerDraw.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "Picker/PickerDraw.vert.spv");

	GraphicsPipelineBuilder pickerPipelineBuilder;
	pickerPipelineBuilder.setShaders(vertexShader, fragShader);
	pickerPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
	pickerPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
	pickerPipelineBuilder.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise);
	pickerPipelineBuilder.disableMultisampling();
	pickerPipelineBuilder.disableSampleShading();
	pickerPipelineBuilder.disableBlending();
	pickerPipelineBuilder.setColorAttachmentFormat(mImage.imageFormat);
	pickerPipelineBuilder.setDepthFormat(mDepthImage.imageFormat);
	pickerPipelineBuilder.enableDepthTest(true, vk::CompareOp::eGreaterOrEqual);
	pickerPipelineBuilder.mPipelineLayout = *mDrawPipelineLayout;

	mDrawPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId++,
		pickerPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
		*mDrawPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mDrawPipelineBundle.pipeline, "PickerDrawPipeline");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Pipeline Created");
}

void Picker::initPickPipeline()
{
	vk::PushConstantRange pickPushConstantRange{};
	pickPushConstantRange.offset = 0;
	pickPushConstantRange.size = sizeof(PickerPickPushConstants);
	pickPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

	std::vector pickDescriptorLayouts = {
		*mDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pickPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pickPipelineLayoutCreateInfo.pSetLayouts = pickDescriptorLayouts.data();
	pickPipelineLayoutCreateInfo.setLayoutCount = pickDescriptorLayouts.size();
	pickPipelineLayoutCreateInfo.pPushConstantRanges = &pickPushConstantRange;
	pickPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mPickPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pickPipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mPickPipelineLayout, "PickerPickPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Layout Created");

	vk::ShaderModule compShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "Picker/PickerPick.comp.spv");

	ComputePipelineBuilder cullPipelineBuilder;
	cullPipelineBuilder.setShader(compShader);
	cullPipelineBuilder.mPipelineLayout = *mPickPipelineLayout;

	mPickPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId++,
		cullPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
		*mPickPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mPickPipelineBundle.pipeline, "PickerPickPipeline");
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Created");
}

void Picker::initDrawPushConstants()
{
	mDrawPushConstants.vertexBuffer = mRenderer->mRendererScene.mMainVertexBuffer.address;
	mDrawPushConstants.nodeTransformsBuffer = mRenderer->mRendererScene.mMainNodeTransformsBuffer.address;
	mDrawPushConstants.instancesBuffer = mRenderer->mRendererScene.mMainInstancesBuffer.address;
}

void Picker::initPickPushConstants()
{
	mPickPushConstants.pickerBuffer = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mBuffer.buffer));
}

void Picker::imguizmoStart()
{
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::BeginFrame();
	ImGuizmo::SetGizmoSizeClipSpace(0.15f);
}

void Picker::imguizmoManipulate(GLTFModel& clickedModel)
{
	ImGuizmo::OPERATION imguizmoOperation = ImGuizmo::TRANSLATE;

	ImGuizmo::SetRect(
		0, 
		0, 
		static_cast<float>(mRenderer->mRendererInfrastructure.mDrawImage.imageExtent.width), 
		static_cast<float>(mRenderer->mRendererInfrastructure.mDrawImage.imageExtent.height)
	);
	ImGuizmo::Manipulate(
		glm::value_ptr(mRenderer->mRendererScene.mPerspective.mData.view),
		glm::value_ptr(mRenderer->mRendererScene.mPerspective.mData.proj),
		imguizmoOperation,
		ImGuizmo::WORLD,
		glm::value_ptr(clickedModel.mModelMatrix)
	);
}

void Picker::cleanup()
{
	mBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Buffer Destroyed");
	mImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Image Destroyed");
	mDepthImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image Destroyed");
	mDescriptorSet.clear();
	mDescriptorSetLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Picker Descriptor Set and Layout Destroyed");
	mDrawPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Draw Pipeline Destroyed");
	mDrawPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Picker Draw Pipeline Layout Destroyed");
	mPickPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Destroyed");
	mPickPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Layout Destroyed");
}
