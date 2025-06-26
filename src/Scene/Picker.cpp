#include <Scene/Picker.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <quill/LogMacros.h>
#include <glm/gtc/type_ptr.hpp>

Picker::Picker(Renderer* renderer) :
	mRenderer(renderer),
	mDrawPipelineLayout(nullptr),
	mPickPipelineLayout(nullptr),
	mDescriptorSet(nullptr),
	mDescriptorSetLayout(nullptr),
	mImguizmoOperation(ImGuizmo::TRANSLATE),
	mClickedInstance(nullptr)
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
	mBuffer = mRenderer->mResources.createBuffer(sizeof(PickerData),
		vk::BufferUsageFlagBits::eTransferDst |
		vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_CPU_TO_GPU);
	mRenderer->mCore.labelResourceDebug(mBuffer.buffer, "PickerBuffer");
	LOG_INFO(mRenderer->mLogger, "Picker Buffer Created");
}

void Picker::initImage()
{
	mImage = mRenderer->mResources.createImage(
		mRenderer->mInfrastructure.mDrawImage.imageExtent,
		vk::Format::eR32G32Uint, // Model Id / Instance Id
		vk::ImageUsageFlagBits::eColorAttachment |
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eStorage
	);
	mRenderer->mCore.labelResourceDebug(mImage.image, "PickerDrawImage");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Image Created");
	mRenderer->mCore.labelResourceDebug(mImage.imageView, "PickerDrawImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Image View Created");

	mDepthImage = mRenderer->mResources.createImage(
		mImage.imageExtent,
		vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment
	);
	mRenderer->mCore.labelResourceDebug(mDepthImage.image, "PickerDepthImage");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image Created");
	mRenderer->mCore.labelResourceDebug(mDepthImage.imageView, "PickerDepthImageView");
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
	mDescriptorSetLayout = builder.build(mRenderer->mCore.mDevice, vk::ShaderStageFlagBits::eCompute);
	mDescriptorSet = mRenderer->mInfrastructure.mMainDescriptorAllocator.allocate(*mDescriptorSetLayout);
	LOG_INFO(mRenderer->mLogger, "Picker Descriptor Set and Layout Created");

	DescriptorSetBinder writer;
	writer.bindImage(0, *mImage.imageView, nullptr, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
	writer.updateSetBindings(mRenderer->mCore.mDevice, *mDescriptorSet);
}

void Picker::initDrawPipeline()
{
	vk::PushConstantRange drawPushConstantRange{};
	drawPushConstantRange.offset = 0;
	drawPushConstantRange.size = sizeof(PickerDrawPushConstants);
	drawPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector drawDescriptorLayouts = {
		*mRenderer->mScene.mPerspective.mDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo drawPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	drawPipelineLayoutCreateInfo.pSetLayouts = drawDescriptorLayouts.data();
	drawPipelineLayoutCreateInfo.setLayoutCount = drawDescriptorLayouts.size();
	drawPipelineLayoutCreateInfo.pPushConstantRanges = &drawPushConstantRange;
	drawPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mDrawPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(drawPipelineLayoutCreateInfo);
	mRenderer->mCore.labelResourceDebug(mDrawPipelineLayout, "PickerDrawPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Draw Pipeline Layout Created");

	vk::ShaderModule fragShader = mRenderer->mResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "PickerDraw.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "PickerDraw.vert.spv");

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
		mRenderer->mInfrastructure.mLatestPipelineId++,
		pickerPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice),
		*mDrawPipelineLayout
	);
	mRenderer->mCore.labelResourceDebug(mDrawPipelineBundle.pipeline, "PickerDrawPipeline");
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

	mPickPipelineLayout = mRenderer->mCore.mDevice.createPipelineLayout(pickPipelineLayoutCreateInfo);
	mRenderer->mCore.labelResourceDebug(mPickPipelineLayout, "PickerPickPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Layout Created");

	vk::ShaderModule compShader = mRenderer->mResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "PickerPick.comp.spv");

	ComputePipelineBuilder cullPipelineBuilder;
	cullPipelineBuilder.setShader(compShader);
	cullPipelineBuilder.mPipelineLayout = *mPickPipelineLayout;

	mPickPipelineBundle = PipelineBundle(
		mRenderer->mInfrastructure.mLatestPipelineId++,
		cullPipelineBuilder.buildPipeline(mRenderer->mCore.mDevice),
		*mPickPipelineLayout
	);
	mRenderer->mCore.labelResourceDebug(mPickPipelineBundle.pipeline, "PickerPickPipeline");
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Created");
}

void Picker::initDrawPushConstants()
{
	mDrawPushConstants.vertexBuffer = mRenderer->mScene.mMainVertexBuffer.address;
	mDrawPushConstants.nodeTransformsBuffer = mRenderer->mScene.mMainNodeTransformsBuffer.address;
	mDrawPushConstants.instancesBuffer = mRenderer->mScene.mMainInstancesBuffer.address;
}

void Picker::initPickPushConstants()
{
	mPickPushConstants.pickerBuffer = mRenderer->mCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mBuffer.buffer));
}

void Picker::changeImguizmoOperation()
{
	switch (mImguizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		mImguizmoOperation = ImGuizmo::ROTATE;
		break;
	case ImGuizmo::ROTATE:
		mImguizmoOperation = ImGuizmo::SCALEU;
		break;
	case ImGuizmo::SCALEU:
		mImguizmoOperation = ImGuizmo::TRANSLATE;
		break;
	default:
		mImguizmoOperation = ImGuizmo::TRANSLATE;
	}
}

void Picker::imguizmoFrame() const
{
	if (mClickedInstance == nullptr) return;

	ImGuizmo::BeginFrame();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetGizmoSizeClipSpace(IMGUIZMO_SIZE);

	ImGuizmo::SetRect(
		0,
		0,
		static_cast<float>(mRenderer->mInfrastructure.mDrawImage.imageExtent.width),
		static_cast<float>(mRenderer->mInfrastructure.mDrawImage.imageExtent.height)
	);
	mRenderer->mScene.mPerspective.mData.proj[1][1] *= -1; // Flip Y-axis in projection for ImGui coordinate system
	ImGuizmo::Manipulate(
		glm::value_ptr(mRenderer->mScene.mPerspective.mData.view),
		glm::value_ptr(mRenderer->mScene.mPerspective.mData.proj),
		mImguizmoOperation,
		ImGuizmo::WORLD,
		glm::value_ptr(mClickedInstance->mData.transformMatrix)
	);

	if (ImGuizmo::IsUsing()) {
		mClickedInstance->mModel->mReloadInstances = true;
	}
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
