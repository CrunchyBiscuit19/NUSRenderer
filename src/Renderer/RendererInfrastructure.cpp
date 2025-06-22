#include <Renderer/Renderer.h>
#include <Renderer/RendererInfrastructure.h>
#include <Utils/Helper.h>

#include <VkBootstrap.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

std::filesystem::path shaderDir = SHADERS_PATH;

RendererInfrastructure::RendererInfrastructure(Renderer* renderer) :
	mRenderer(renderer),
	mSwapchainBundle(nullptr),
	mMainDescriptorAllocator(DescriptorAllocatorGrowable(renderer))
{
	mFrames.resize(FRAME_OVERLAP);
}

void RendererInfrastructure::init()
{
	initSwapchain();
	initDescriptors();
	initFrames();
}

void RendererInfrastructure::initDescriptors()
{
	std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = {
		{vk::DescriptorType::eUniformBuffer, 1}, // Scene UBO
		{vk::DescriptorType::eStorageImage, 1}, // Picker Draw Image
		{vk::DescriptorType::eCombinedImageSampler, 1}, // Skybox Cubemap
		{vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS}, // Material Textures
	};
	mMainDescriptorAllocator.init(1, sizes);

	LOG_INFO(mRenderer->mLogger, "Descriptor Allocator Created");
}

void RendererInfrastructure::initFrames()
{
	// mRenderFence to control when the GPU has finished rendering the frame, start signalled so we can wait on it on the first frame
	vk::FenceCreateInfo fenceCreateInfo = vkhelper::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
	vk::CommandPoolCreateInfo commandPoolInfo = vkhelper::commandPoolCreateInfo(
		mRenderer->mRendererCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkhelper::semaphoreCreateInfo();

	for (int i = 0; i < mFrames.size(); i++)
	{
		mFrames[i].mRenderFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mRenderFence, fmt::format("FrameFence{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Render Fence Created", i);

		mFrames[i].mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mCommandPool,
		                                            fmt::format("FrameCommandPool{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Command Pool Created", i);

		vk::CommandBufferAllocateInfo cmdAllocInfo = vkhelper::commandBufferAllocateInfo(*mFrames[i].mCommandPool, 1);
		mFrames[i].mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mCommandBuffer,
		                                            fmt::format("FrameCommandBuffer{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Command Buffer Created", i);

		mFrames[i].mAvailableSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
		mRenderer->mRendererCore.labelResourceDebug(mFrames[i].mAvailableSemaphore,
		                                            fmt::format("FrameAvailableSemaphore{}", i).c_str());
		LOG_INFO(mRenderer->mLogger, "Frame {} Available Semaphore Created", i);
	}
}

void RendererInfrastructure::initSwapchain()
{
	mSwapchainIndex = 0;
	mSwapchainBundle.mFormat = vk::Format::eB8G8R8A8Unorm;

	vkb::SwapchainBuilder swapchainBuilder{
		*mRenderer->mRendererCore.mChosenGPU, *mRenderer->mRendererCore.mDevice, *mRenderer->mRendererCore.mSurface
	};
	vkb::Swapchain vkbSwapchain = swapchainBuilder
	                              .set_desired_format(VkSurfaceFormatKHR{
		                              .format = static_cast<VkFormat>(mSwapchainBundle.mFormat),
		                              .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear)
	                              })
	                              .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eFifo))
	                              .set_desired_extent(mRenderer->mRendererCore.mWindowExtent.width,
	                                                  mRenderer->mRendererCore.mWindowExtent.height)
	                              .add_image_usage_flags(
		                              static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
	                              .set_desired_min_image_count(NUMBER_OF_SWAPCHAIN_IMAGES)
	                              .build()
	                              .value();

	mSwapchainBundle.mExtent = vkbSwapchain.extent;
	mSwapchainBundle.mSwapchain = vk::raii::SwapchainKHR(mRenderer->mRendererCore.mDevice, vkbSwapchain.swapchain);

	mSwapchainBundle.mImages.reserve(NUMBER_OF_SWAPCHAIN_IMAGES);
	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkhelper::semaphoreCreateInfo();
	for (int i = 0; i < vkbSwapchain.get_images().value().size(); i++)
	{
		mSwapchainBundle.mImages.emplace_back(
			vkbSwapchain.get_images().value()[i],
			mRenderer->mRendererCore.mDevice.createImageView(vkhelper::imageViewCreateInfo(
				mSwapchainBundle.mFormat, vkbSwapchain.get_images().value()[i], vk::ImageAspectFlagBits::eColor)),
			mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo)
		);
	}

	for (int i = 0; i < mSwapchainBundle.mImages.size(); i++)
	{
		mRenderer->mRendererCore.labelResourceDebug(mSwapchainBundle.mImages[i].image,
		                                            fmt::format("SwapchainImage{}", i).c_str());
		mRenderer->mRendererCore.labelResourceDebug(mSwapchainBundle.mImages[i].imageView,
		                                            fmt::format("SwapchainImageView{}", i).c_str());
		mRenderer->mRendererCore.labelResourceDebug(mSwapchainBundle.mImages[i].renderedSemaphore,
		                                            fmt::format("SwapchainRenderedSemaphore{}", i).c_str());
	}

	mDrawImage = mRenderer->mRendererResources.createImage(
		vk::Extent3D{mRenderer->mRendererCore.mWindowExtent, 1},
		vk::Format::eR16G16B16A16Sfloat,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eColorAttachment,
		false, true);
	mDepthImage = mRenderer->mRendererResources.createImage(
		mDrawImage.imageExtent,
		vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		false, true);
	mIntermediateImage = mRenderer->mRendererResources.createImage(
		mDrawImage.imageExtent,
		vk::Format::eR16G16B16A16Sfloat,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eColorAttachment);

	mRenderer->mRendererCore.labelResourceDebug(mDrawImage.image, "DrawImage");
	mRenderer->mRendererCore.labelResourceDebug(mDrawImage.imageView, "DrawImageView");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.image, "DepthImage");
	mRenderer->mRendererCore.labelResourceDebug(mDepthImage.imageView, "DepthImageView");
	mRenderer->mRendererCore.labelResourceDebug(mIntermediateImage.image, "IntermediateImage");
	mRenderer->mRendererCore.labelResourceDebug(mIntermediateImage.imageView, "IntermediateImageView");

	mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd)
	{
		for (int i = 0; i < mSwapchainBundle.mImages.size(); i++)
		{
			vkhelper::transitionImage(cmd, mSwapchainBundle.mImages[i].image,
			                          vk::PipelineStageFlagBits2::eNone,
			                          vk::AccessFlagBits2::eNone,
			                          vk::PipelineStageFlagBits2::eNone,
			                          vk::AccessFlagBits2::eNone,
			                          vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
		}
		vkhelper::transitionImage(cmd, *mDrawImage.image,
		                          vk::PipelineStageFlagBits2::eNone,
		                          vk::AccessFlagBits2::eNone,
		                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		                          vk::AccessFlagBits2::eColorAttachmentWrite,
		                          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
		vkhelper::transitionImage(cmd, *mDepthImage.image,
		                          vk::PipelineStageFlagBits2::eNone,
		                          vk::AccessFlagBits2::eNone,
		                          vk::PipelineStageFlagBits2::eEarlyFragmentTests,
		                          vk::AccessFlagBits2::eDepthStencilAttachmentRead |
		                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		                          vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);
		vkhelper::transitionImage(cmd, *mIntermediateImage.image,
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

void RendererInfrastructure::cleanup()
{
	for (auto& frame : mFrames)
	{
		frame.cleanup();
	}
	LOG_INFO(mRenderer->mLogger, "Frames Destroyed");
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
{
}

void Frame::cleanup()
{
	mAvailableSemaphore.clear();
	mRenderFence.clear();
	mCommandBuffer.clear();
	mCommandPool.clear();
}
