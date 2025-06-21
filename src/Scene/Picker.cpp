#include <Scene/Picker.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <quill/LogMacros.h>

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
	initCallback();
}

void Picker::initPushConstants()
{
	mPushConstants.vertexBuffer = mRenderer->mRendererScene.mMainVertexBuffer.address;
	mPushConstants.nodeTransformsBuffer = mRenderer->mRendererScene.mMainNodeTransformsBuffer.address;
	mPushConstants.instancesBuffer = mRenderer->mRendererScene.mMainInstancesBuffer.address;
}

void Picker::initBuffer()
{
	mBuffer = mRenderer->mRendererResources.createBuffer(sizeof(PickerData),
		vk::BufferUsageFlagBits::eTransferDst |
		vk::BufferUsageFlagBits::eUniformBuffer,
		VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void Picker::initImage()
{
	mImage = mRenderer->mRendererResources.createImage(
		mRenderer->mRendererInfrastructure.mDrawImage.imageExtent,
		vk::Format::eR32G32B32A32Uint, // Model Id / Mesh Id / Instance Id
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
	);
	mRenderer->mRendererCore.labelResourceDebug(mImage.image, "PickerObjectImage");
	LOG_INFO(mRenderer->mLogger, "Picker Object Image Created");
	mRenderer->mRendererCore.labelResourceDebug(mImage.imageView, "PickerObjectImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Object Image View Created");

	mDepthImage = mRenderer->mRendererResources.createImage(
		mImage.imageExtent,
		vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment
	);
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.image, "PickerDepthImage");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image Created");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.imageView, "PickerDepthImageView");
	LOG_INFO(mRenderer->mLogger, "Picker Depth Image View Created");
}

void Picker::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	builder.addBinding(1, vk::DescriptorType::eCombinedImageSampler);
	mDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eCompute);
	mDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mDescriptorSetLayout);
	LOG_INFO(mRenderer->mLogger, "Picker Descriptor Set and Layout Created");

	DescriptorSetBinder writer;
	writer.bindBuffer(0, *mBuffer.buffer, sizeof(PickerData), 0, vk::DescriptorType::eUniformBuffer);
	writer.bindImage(1, *mImage.imageView, mRenderer->mRendererResources.getSampler(), vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mDescriptorSet);
}


void Picker::initDrawPipeline()
{
	vk::PushConstantRange drawPushConstantRange{};
	drawPushConstantRange.offset = 0;
	drawPushConstantRange.size = sizeof(ForwardPushConstants);
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
		std::filesystem::path(SHADERS_PATH) / "Picker/Picker.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "Picker/Picker.vert.spv");

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
	std::vector pickDescriptorLayouts = {
		*mDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pickPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pickPipelineLayoutCreateInfo.pSetLayouts = pickDescriptorLayouts.data();
	pickPipelineLayoutCreateInfo.setLayoutCount = pickDescriptorLayouts.size();
	pickPipelineLayoutCreateInfo.pushConstantRangeCount = 0;

	mPickPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pickPipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mPickPipelineLayout, "PickerPickPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Picker Pick Pipeline Layout Created");

	vk::ShaderModule compShader = mRenderer->mRendererResources.getShader(
		std::filesystem::path(SHADERS_PATH) / "Picker/Picker.comp.spv");

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

void Picker::initCallback()
{
	mRenderer->mRendererEvent.addEventCallback([this](SDL_Event& e) -> void
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !mRenderer->mCamera.mRelativeMode && !ImGui::GetIO().WantCaptureMouse) {
			mRenderer->mImmSubmit.mCallbacks.emplace_back([this](const Renderer* renderer, vk::CommandBuffer cmd) {
				float mouseClickLocation[2];
				mouseClickLocation[0] = ImGui::GetIO().MousePos.x / static_cast<float>(renderer->mRendererCore.mWindowExtent.width);
				mouseClickLocation[1] = ImGui::GetIO().MousePos.y / static_cast<float>(renderer->mRendererCore.mWindowExtent.height);

				std::memcpy(mBuffer.info.pMappedData, &mouseClickLocation, 2 * sizeof(float));

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mPickPipelineBundle.pipeline);

				vkhelper::createBufferPipelineBarrier( // Wait for pick buffer to be written to
					cmd,
					*mBuffer.buffer,
					vk::PipelineStageFlagBits2::eHost,
					vk::AccessFlagBits2::eHostWrite,
					vk::PipelineStageFlagBits2::eComputeShader,
					vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

				cmd.dispatch(1, 1, 1);

				vkhelper::createBufferPipelineBarrier( // Wait for pick buffer to be written to
					cmd,
					*mBuffer.buffer,
					vk::PipelineStageFlagBits2::eComputeShader,
					vk::AccessFlagBits2::eShaderWrite,
					vk::PipelineStageFlagBits2::eHost,
					vk::AccessFlagBits2::eHostRead);

				glm::uvec2 readData(0);
				std::memcpy(&readData.x, static_cast<char*>(mBuffer.info.pMappedData) + sizeof(glm::vec2), 2 * sizeof(uint32_t));

				LOG_DEBUG(renderer->mLogger, "Clicked into ({:.3f},{:.3f}) Got ({},{})", mouseClickLocation[0], mouseClickLocation[1], readData.x, readData.y);
			});
		}
	});
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
