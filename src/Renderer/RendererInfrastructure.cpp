#include <Renderer/Renderer.h>
#include <Renderer/RendererInfrastructure.h>
#include <Utils/Helper.h>

#include <VkBootstrap.h>
#include <fmt/core.h>

std::filesystem::path shaderDir = SHADERS_PATH;

RendererInfrastructure::RendererInfrastructure(Renderer* renderer) :
	mRenderer(renderer),
	mSwapchainBundle(nullptr),
	mMainDescriptorAllocator(DescriptorAllocatorGrowable(renderer)),
	mMaterialPipelineLayout(nullptr),
	mCullPipeline(nullptr),
	mCullPipelineLayout(nullptr)
{
	mFrames.resize(FRAME_OVERLAP);
}

void RendererInfrastructure::init() {
	initSwapchain();
	initCullPipeline();
	initPipelineLayouts();
	initDescriptors();
	initFrames();
}

void RendererInfrastructure::initDescriptors()
{
	std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = {
		{ vk::DescriptorType::eUniformBuffer, 1 },          // Scene UBO
		{ vk::DescriptorType::eCombinedImageSampler, 1 },   // Skybox Cubemap
		{ vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS },   // Material Textures
	};
	mMainDescriptorAllocator.init(1, sizes);

	LOG_INFO(mRenderer->mLogger, "Descriptor Allocator Created");
}

void RendererInfrastructure::initFrames()
{
	// mRenderFence to control when the GPU has finished rendering the frame, start signalled so we can wait on it on the first frame
	vk::FenceCreateInfo fenceCreateInfo = vkhelper::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
	vk::CommandPoolCreateInfo commandPoolInfo = vkhelper::commandPoolCreateInfo(mRenderer->mRendererCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkhelper::semaphoreCreateInfo();

	for (int i = 0; i < mFrames.size(); i++) {
		mFrames[i].mRenderFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mRenderFence, fmt::format("FrameFence{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Render Fence Created", i);

		mFrames[i].mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mCommandPool, fmt::format("FrameCommandPool{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Command Pool Created", i);

		vk::CommandBufferAllocateInfo cmdAllocInfo = vkhelper::commandBufferAllocateInfo(*mFrames[i].mCommandPool, 1);
		mFrames[i].mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mCommandBuffer, fmt::format("FrameCommandBuffer{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Command Buffer Created", i);

		mFrames[i].mAvailableSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mAvailableSemaphore, fmt::format("FrameAvailableSemaphore{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Available Semaphore Created", i);
	}
}

void RendererInfrastructure::initCullPipeline()
{
	createCullPipeline();
	LOG_INFO(mRenderer->mLogger, "Culling Pipeline Created");
}

void RendererInfrastructure::initPipelineLayouts()
{
	vk::PushConstantRange materialPushConstantRange{};
	materialPushConstantRange.offset = 0;
	materialPushConstantRange.size = sizeof(ScenePushConstants);
	materialPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> materialDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mPerspectiveDescriptorSetLayout,
		*mRenderer->mRendererScene.mSceneManager.mMainMaterialResourcesDescriptorSetLayout
	};

	vk::PipelineLayoutCreateInfo materialPipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	materialPipelineLayoutCreateInfo.pSetLayouts = materialDescriptorLayouts.data();
	materialPipelineLayoutCreateInfo.setLayoutCount = materialDescriptorLayouts.size();
	materialPipelineLayoutCreateInfo.pPushConstantRanges = &materialPushConstantRange;
	materialPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mMaterialPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(materialPipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mMaterialPipelineLayout, "MaterialPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Material Pipeline Layout Created");

	vk::PushConstantRange cullPushConstantRange{};
	cullPushConstantRange.offset = 0;
	cullPushConstantRange.size = sizeof(CullPushConstants);
	cullPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;

	vk::PipelineLayoutCreateInfo cullLayoutInfo{};
	cullLayoutInfo.setLayoutCount = 0;
	cullLayoutInfo.pSetLayouts = nullptr;
	cullLayoutInfo.pPushConstantRanges = &cullPushConstantRange;
	cullLayoutInfo.pushConstantRangeCount = 1;

	mCullPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(cullLayoutInfo);
	mRenderer->mRendererCore.labelResourceDebug(mCullPipelineLayout, "CullPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Layout Created");

	vk::PushConstantRange skyBoxPushConstantRange{};
	skyBoxPushConstantRange.offset = 0;
	skyBoxPushConstantRange.size = sizeof(SkyBoxPushConstants);
	skyBoxPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> skyboxDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mPerspectiveDescriptorSetLayout,
		*mRenderer->mRendererScene.mSkybox.mSkyboxDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pipelineLayoutCreateInfo.pSetLayouts = skyboxDescriptorLayouts.data();
	pipelineLayoutCreateInfo.setLayoutCount = skyboxDescriptorLayouts.size();
	pipelineLayoutCreateInfo.pPushConstantRanges = &skyBoxPushConstantRange;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mRenderer->mRendererScene.mSkybox.mSkyboxPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mRenderer->mRendererScene.mSkybox.mSkyboxPipelineLayout, "SkyboxPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Layout Created");

}

void RendererInfrastructure::initSwapchain()
{
	mSwapchainIndex = 0;
	mSwapchainBundle.mFormat = vk::Format::eB8G8R8A8Unorm;

	vkb::SwapchainBuilder swapchainBuilder{ *mRenderer->mRendererCore.mChosenGPU, *mRenderer->mRendererCore.mDevice, *mRenderer->mRendererCore.mSurface };
	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.set_desired_format(VkSurfaceFormatKHR{ .format = static_cast<VkFormat>(mSwapchainBundle.mFormat), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear) })
		.set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eFifo))
		.set_desired_extent(mRenderer->mRendererCore.mWindowExtent.width, mRenderer->mRendererCore.mWindowExtent.height)
		.add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
		.set_desired_min_image_count(NUMBER_OF_SWAPCHAIN_IMAGES)
		.build()
		.value();

	mSwapchainBundle.mExtent = vkbSwapchain.extent;
	mSwapchainBundle.mSwapchain = vk::raii::SwapchainKHR(mRenderer->mRendererCore.mDevice, vkbSwapchain.swapchain);

	mSwapchainBundle.mImages.reserve(NUMBER_OF_SWAPCHAIN_IMAGES);
	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkhelper::semaphoreCreateInfo();
	for (int i = 0; i < vkbSwapchain.get_images().value().size(); i++) {
		mSwapchainBundle.mImages.emplace_back(
			vkbSwapchain.get_images().value()[i],
			mRenderer->mRendererCore.mDevice.createImageView(vkhelper::imageViewCreateInfo(mSwapchainBundle.mFormat, vkbSwapchain.get_images().value()[i], vk::ImageAspectFlagBits::eColor)),
			mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo)
		);
	}

	for (int i = 0; i < mSwapchainBundle.mImages.size(); i++) {
		mRenderer->mRendererCore.labelResourceDebug(mSwapchainBundle.mImages[i].image, fmt::format("SwapchainImage{}", i).c_str());
		mRenderer->mRendererCore.labelResourceDebug(mSwapchainBundle.mImages[i].imageView, fmt::format("SwapchainImageView{}", i).c_str());
		mRenderer->mRendererCore.labelResourceDebug(mSwapchainBundle.mImages[i].renderedSemaphore, fmt::format("SwapchainRenderedSemaphore{}", i).c_str());
	}

	mDrawImage = mRenderer->mRendererResources.createImage(
		vk::Extent3D{ mRenderer->mRendererCore.mWindowExtent, 1 },
		vk::Format::eR16G16B16A16Sfloat,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
		false, true);
	mDepthImage = mRenderer->mRendererResources.createImage(
		mDrawImage.imageExtent,
		vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		false, true);
	mIntermediateImage = mRenderer->mRendererResources.createImage(
		mDrawImage.imageExtent,
		vk::Format::eR16G16B16A16Sfloat,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment);

	mRenderer->mRendererCore.labelResourceDebug(mDrawImage.image, "DrawImage");
	mRenderer->mRendererCore.labelResourceDebug(mDrawImage.imageView, "DrawImageView");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.image, "DepthImage");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.imageView, "DepthImageView");
	mRenderer->mRendererCore.labelResourceDebug(mIntermediateImage.image, "IntermediateImage");
	mRenderer->mRendererCore.labelResourceDebug(mIntermediateImage.imageView, "IntermediateImageView");

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		for (int i = 0; i < mSwapchainBundle.mImages.size(); i++) {
			vkhelper::transitionImage(*cmd, mSwapchainBundle.mImages[i].image,
				vk::PipelineStageFlagBits2::eNone,
				vk::AccessFlagBits2::eNone,
				vk::PipelineStageFlagBits2::eNone,
				vk::AccessFlagBits2::eNone,
				vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
		}
		vkhelper::transitionImage(*cmd, *mDrawImage.image,
			vk::PipelineStageFlagBits2::eNone,
			vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
		vkhelper::transitionImage(*cmd, *mDepthImage.image,
			vk::PipelineStageFlagBits2::eNone,
			vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests,
			vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);
		vkhelper::transitionImage(*cmd, *mIntermediateImage.image,
			vk::PipelineStageFlagBits2::eNone,
			vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
	});

	LOG_INFO(mRenderer->mLogger, "Swapchain Created");
}

void RendererInfrastructure::destroySwapchain()
{
	mDepthImage.cleanup();
	mIntermediateImage.cleanup();
	mDrawImage.cleanup();

	mSwapchainBundle.mSwapchain.clear();
	mSwapchainBundle.mImages.clear();
}

void RendererInfrastructure::resizeSwapchain()
{
	mRenderer->mRendererCore.mDevice.waitIdle();

	destroySwapchain();

	int w, h;
	SDL_GetWindowSize(mRenderer->mRendererCore.mWindow, &w, &h);
	mRenderer->mRendererCore.mWindowExtent.width = w;
	mRenderer->mRendererCore.mWindowExtent.height = h;

	initSwapchain();

	LOG_INFO(mRenderer->mLogger, "Swapchain Resized");

	mResizeRequested = false;
}

vk::Pipeline RendererInfrastructure::getMaterialPipeline(PipelineOptions materialPipelineOptions)
{
	if (auto it = mMaterialPipelinesCache.find(materialPipelineOptions); it != mMaterialPipelinesCache.end()) {
		return *it->second;
	}
	createMaterialPipeline(materialPipelineOptions);
	return *mMaterialPipelinesCache.at(materialPipelineOptions);
}

void RendererInfrastructure::createMaterialPipeline(PipelineOptions materialPipelineOptions)
{
	vk::ShaderModule fragShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "mesh/mesh.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "mesh/mesh.vert.spv");

	vk::CullModeFlags cullMode;
	(materialPipelineOptions.doubleSided) ? (cullMode = vk::CullModeFlagBits::eNone) : (cullMode = vk::CullModeFlagBits::eBack);
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
	transparency ? materialPipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual) : materialPipelineBuilder.enableDepthtest(true, vk::CompareOp::eGreaterOrEqual);;
	materialPipelineBuilder.enableDepthtest(true, vk::CompareOp::eGreaterOrEqual);
	materialPipelineBuilder.setColorAttachmentFormat(mDrawImage.imageFormat);
	materialPipelineBuilder.setDepthFormat(mDepthImage.imageFormat);
	materialPipelineBuilder.mPipelineLayout = *mMaterialPipelineLayout;

	auto [it, _] = mMaterialPipelinesCache.try_emplace(materialPipelineOptions, mRenderer->mRendererCore.mDevice, nullptr, materialPipelineBuilder.createPipelineCreateInfo());
	mRenderer->mRendererCore.labelResourceDebug(it->second, fmt::format("MaterialPipeline{}", mLatestPipelineId).c_str());
	LOG_INFO(mRenderer->mLogger, "{}", fmt::format("Material Pipeline {} Created", mLatestPipelineId).c_str());

	mLatestPipelineId++;
}

void RendererInfrastructure::createCullPipeline()
{
	vk::ShaderModule computeShaderModule = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "cull/cull.comp.spv");

	ComputePipelineBuilder computePipelineBuilder;
	computePipelineBuilder.setShader(computeShaderModule);
	computePipelineBuilder.mPipelineLayout = *mCullPipelineLayout;

	mCullPipeline = vk::raii::Pipeline(mRenderer->mRendererCore.mDevice, nullptr, computePipelineBuilder.createPipelineCreateInfo());
	mRenderer->mRendererCore.labelResourceDebug(mCullPipeline, "CullPipeline");
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Created");

	mLatestPipelineId++;
}

void RendererInfrastructure::createSkyboxPipeline()
{
	vk::ShaderModule fragShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "skybox/skybox.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "skybox/skybox.vert.spv");

	GraphicsPipelineBuilder skyboxPipelineBuilder;
	skyboxPipelineBuilder.setShaders(vertexShader, fragShader);
	skyboxPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
	skyboxPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
	skyboxPipelineBuilder.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise);
	skyboxPipelineBuilder.enableMultisampling();
	skyboxPipelineBuilder.enableSampleShading();
	skyboxPipelineBuilder.enableBlendingSkybox();
	skyboxPipelineBuilder.setColorAttachmentFormat(mRenderer->mRendererInfrastructure.mDrawImage.imageFormat);
	skyboxPipelineBuilder.setDepthFormat(mRenderer->mRendererInfrastructure.mDepthImage.imageFormat);
	skyboxPipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual);
	skyboxPipelineBuilder.mPipelineLayout = *mRenderer->mRendererScene.mSkybox.mSkyboxPipelineLayout;

	mRenderer->mRendererScene.mSkybox.mSkyboxPipeline = vk::raii::Pipeline(mRenderer->mRendererCore.mDevice, nullptr, skyboxPipelineBuilder.createPipelineCreateInfo());
	mRenderer->mRendererCore.labelResourceDebug(mRenderer->mRendererScene.mSkybox.mSkyboxPipeline, "SkyboxPipeline");
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Created");

	mLatestPipelineId++;
}

void RendererInfrastructure::cleanup() {
	for (auto& frame : mFrames) {
		frame.cleanup();
	}
	LOG_INFO(mRenderer->mLogger, "Frames Destroyed");
	mMaterialPipelinesCache.clear();
	LOG_INFO(mRenderer->mLogger, "Material Pipelines Destroyed");
	mMaterialPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Material Pipelines Layout Destroyed");
	mCullPipeline.clear();
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Destroyed");
	mCullPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Cull Pipeline Layout Destroyed");
	destroySwapchain();
	LOG_INFO(mRenderer->mLogger, "Swapchain Destroyed");
	mMainDescriptorAllocator.cleanup();
	LOG_INFO(mRenderer->mLogger, "Descriptor Allocator Destroyed");
}

Frame::Frame()
	: mCommandPool(nullptr)
	, mCommandBuffer(nullptr)
	, mRenderFence(nullptr)
	, mAvailableSemaphore(nullptr)
{}

void Frame::cleanup()
{
	mAvailableSemaphore.clear();
	mRenderFence.clear();
	mCommandBuffer.clear();
	mCommandPool.clear();
}
