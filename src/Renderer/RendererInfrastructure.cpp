#include <Renderer/Renderer.h>
#include <Renderer/RendererInfrastructure.h>
#include <Utils/Helper.h>
#include <VkBootstrap.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

std::filesystem::path shaderDir = SHADERS_PATH;

RendererInfrastructure::RendererInfrastructure(Renderer* renderer)
    : mRenderer(renderer), mSwapchainBundle(nullptr), mMainDescriptorAllocator(DescriptorAllocatorGrowable(renderer)) {
    mFrames.resize(FRAME_OVERLAP);
}

void RendererInfrastructure::initDescriptors() {
    std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = {
        {vk::DescriptorType::eUniformBuffer, 1},                               // Scene UBO
        {vk::DescriptorType::eStorageImage, 1},                                // Picker Draw Image
        {vk::DescriptorType::eCombinedImageSampler, 1},                        // Skybox Cubemap
        {vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS},  // Material Textures
    };
    mMainDescriptorAllocator.init(MAX_DESCRIPTOR_SETS_PER_POOL, sizes);

    LOG_INFO(mRenderer->mLogger, "Descriptor Allocator Created");
}

void RendererInfrastructure::initFrames() {
    // mRenderFence to control when the GPU has finished rendering the frame, start signalled so we can wait on it on the first frame
    vk::FenceCreateInfo fenceCreateInfo = vkhelper::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    vk::CommandPoolCreateInfo commandPoolInfo =
        vkhelper::commandPoolCreateInfo(mRenderer->mCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkhelper::semaphoreCreateInfo();

    for (u32 i = 0; i < mFrames.size(); i++) {
        mFrames[i].mRenderFence = mRenderer->mCore.mDevice.createFence(fenceCreateInfo);
        mRenderer->mCore.labelResourceDebug(mFrames[i].mRenderFence, fmt::format("FrameFence{}", i).c_str());
        LOG_INFO(mRenderer->mLogger, "Frame {} Render Fence Created", i);

        mFrames[i].mCommandPool = mRenderer->mCore.mDevice.createCommandPool(commandPoolInfo);
        mRenderer->mCore.labelResourceDebug(mFrames[i].mCommandPool, fmt::format("FrameCommandPool{}", i).c_str());
        LOG_INFO(mRenderer->mLogger, "Frame {} Command Pool Created", i);

        vk::CommandBufferAllocateInfo cmdAllocInfo = vkhelper::commandBufferAllocateInfo(*mFrames[i].mCommandPool, 1);
        mFrames[i].mCommandBuffer = std::move(mRenderer->mCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);
        mRenderer->mCore.labelResourceDebug(mFrames[i].mCommandBuffer, fmt::format("FrameCommandBuffer{}", i).c_str());
        LOG_INFO(mRenderer->mLogger, "Frame {} Command Buffer Created", i);

        mFrames[i].mAvailableSemaphore = mRenderer->mCore.mDevice.createSemaphore(semaphoreCreateInfo);
        mRenderer->mCore.labelResourceDebug(mFrames[i].mAvailableSemaphore, fmt::format("FrameAvailableSemaphore{}", i).c_str());
        LOG_INFO(mRenderer->mLogger, "Frame {} Available Semaphore Created", i);

        mFrames[i].mPerspectiveBuffer = mRenderer->mResources.createAddressedBuffer(
            sizeof(PerspectiveData),
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        mRenderer->mCore.labelResourceDebug(mFrames[i].mPerspectiveBuffer.buffer, fmt::format("FramePerspectiveBuffer{}", i).c_str());
        LOG_INFO(mRenderer->mLogger, "Frame {} Perspective Buffer Created", i);

        mFrames[i].mPerspectiveDescriptorSet =
            mRenderer->mInfrastructure.mMainDescriptorAllocator.allocate(*mRenderer->mScene.mPerspective.mDescriptorSetLayout);
        mRenderer->mCore.labelResourceDebug(mFrames[i].mPerspectiveDescriptorSet, fmt::format("FramePerspectiveDescriptorSet{}", i).c_str());
        LOG_INFO(mRenderer->mLogger, "Frame {} Perspective Descriptor Set Created", i);

        DescriptorSetBinder binder;
        binder.bindBuffer(0, *mFrames[i].mPerspectiveBuffer.buffer, sizeof(PerspectiveData), 0, vk::DescriptorType::eUniformBuffer);
        binder.updateSetBindings(mRenderer->mCore.mDevice, *mFrames[i].mPerspectiveDescriptorSet);
        LOG_INFO(mRenderer->mLogger, "Frame {} Perspective Descriptor Set Binded", i);
    }
}

void RendererInfrastructure::initSwapchain() {
    mSwapchainIndex = 0;
    mSwapchainBundle.mFormat = vk::Format::eB8G8R8A8Srgb;
    mSwapchainBundle.mUnormFormat = vk::Format::eB8G8R8A8Unorm;

    vk::ImageFormatListCreateInfo formatListCreateInfo{};
    std::array<vk::Format, 2> formats = {mSwapchainBundle.mFormat, mSwapchainBundle.mUnormFormat};
    formatListCreateInfo.pViewFormats = formats.data();
    formatListCreateInfo.viewFormatCount = formats.size();

    vkb::SwapchainBuilder swapchainBuilder{*mRenderer->mCore.mChosenGPU, *mRenderer->mCore.mDevice, *mRenderer->mCore.mSurface};
    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format(VkSurfaceFormatKHR{
                .format = static_cast<VkFormat>(mSwapchainBundle.mFormat), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear)
            })
            .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eFifo))
            .set_desired_extent(mRenderer->mCore.mWindowExtent.width, mRenderer->mCore.mWindowExtent.height)
            .add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
            .set_desired_min_image_count(NUMBER_OF_SWAPCHAIN_IMAGES)
            .set_create_flags(static_cast<VkSwapchainCreateFlagBitsKHR>(vk::SwapchainCreateFlagBitsKHR::eMutableFormat))
            .add_pNext(&formatListCreateInfo)
            .build()
            .value();

    mSwapchainBundle.mExtent = vkbSwapchain.extent;
    mSwapchainBundle.mSwapchain = vk::raii::SwapchainKHR(mRenderer->mCore.mDevice, vkbSwapchain.swapchain);

    mSwapchainBundle.mImages.reserve(NUMBER_OF_SWAPCHAIN_IMAGES);
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkhelper::semaphoreCreateInfo();
    for (u32 i = 0; i < vkbSwapchain.get_images().value().size(); i++) {
        mSwapchainBundle.mImages.emplace_back(
            vkbSwapchain.get_images().value()[i],
            mRenderer->mCore.mDevice.createImageView(
                vkhelper::imageViewCreateInfo(mSwapchainBundle.mFormat, vkbSwapchain.get_images().value()[i], vk::ImageAspectFlagBits::eColor)
            ),
            mRenderer->mCore.mDevice.createImageView(
                vkhelper::imageViewCreateInfo(mSwapchainBundle.mUnormFormat, vkbSwapchain.get_images().value()[i], vk::ImageAspectFlagBits::eColor)
            ),
            mRenderer->mCore.mDevice.createSemaphore(semaphoreCreateInfo)
        );
    }

    for (u32 i = 0; i < mSwapchainBundle.mImages.size(); i++) {
        mRenderer->mCore.labelResourceDebug(mSwapchainBundle.mImages[i].image, fmt::format("SwapchainImage{}", i).c_str());
        mRenderer->mCore.labelResourceDebug(mSwapchainBundle.mImages[i].imageView, fmt::format("SwapchainImageView{}", i).c_str());
        mRenderer->mCore.labelResourceDebug(mSwapchainBundle.mImages[i].uNormImageView, fmt::format("SwapchainUnormImageView{}", i).c_str());
        mRenderer->mCore.labelResourceDebug(mSwapchainBundle.mImages[i].renderedSemaphore, fmt::format("SwapchainRenderedSemaphore{}", i).c_str());
    }

    mDrawImage = mRenderer->mResources.createImage(
        vk::Extent3D{mRenderer->mCore.mWindowExtent, 1},
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
        false,
        true
    );
    mDepthImage =
        mRenderer->mResources.createImage(mDrawImage.imageExtent, vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment, false, true);
    mIntermediateImage = mRenderer->mResources.createImage(
        mDrawImage.imageExtent,
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment
    );

    mRenderer->mCore.labelResourceDebug(mDrawImage.image, "DrawImage");
    mRenderer->mCore.labelResourceDebug(mDrawImage.imageView, "DrawImageView");
    mRenderer->mCore.labelResourceDebug(mDepthImage.image, "DepthImage");
    mRenderer->mCore.labelResourceDebug(mDepthImage.imageView, "DepthImageView");
    mRenderer->mCore.labelResourceDebug(mIntermediateImage.image, "IntermediateImage");
    mRenderer->mCore.labelResourceDebug(mIntermediateImage.imageView, "IntermediateImageView");

    mRenderer->mImmSubmit.mCallbacks.emplace_back([this](Renderer* renderer, vk::CommandBuffer cmd) {
        for (u32 i = 0; i < mSwapchainBundle.mImages.size(); i++) {
            vkhelper::transitionImage(
                cmd,
                mSwapchainBundle.mImages[i].image,
                vk::ImageLayout::eUndefined,
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::ePresentSrcKHR,
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone
            );
        }
        vkhelper::transitionImage(
            cmd,
            *mDrawImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite
        );
        vkhelper::transitionImage(
            cmd,
            *mDepthImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        );
        vkhelper::transitionImage(
            cmd,
            *mIntermediateImage.image,
            vk::ImageLayout::eUndefined,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead
        );
    });

    LOG_INFO(mRenderer->mLogger, "Swapchain Created");
}

void RendererInfrastructure::destroySwapchain() {
    mDepthImage.cleanup();
    mIntermediateImage.cleanup();
    mDrawImage.cleanup();

    mSwapchainBundle.mSwapchain.clear();
    mSwapchainBundle.mImages.clear();
}

void RendererInfrastructure::resizeSwapchain() {
    mRenderer->mCore.mDevice.waitIdle();

    destroySwapchain();

    i32 w, h;
    SDL_GetWindowSize(mRenderer->mCore.mWindow, &w, &h);
    mRenderer->mCore.mWindowExtent.width = w;
    mRenderer->mCore.mWindowExtent.height = h;

    initSwapchain();

    LOG_INFO(mRenderer->mLogger, "Swapchain Resized");

    mResizeRequested = false;
}

void RendererInfrastructure::cleanup() {
    for (auto& frame : mFrames) {
        frame.cleanup();
    }
    LOG_INFO(mRenderer->mLogger, "Frames Destroyed");
    destroySwapchain();
    LOG_INFO(mRenderer->mLogger, "Swapchain Destroyed");
    mMainDescriptorAllocator.cleanup();
    LOG_INFO(mRenderer->mLogger, "Descriptor Allocator Destroyed");
}

Frame::Frame() : mCommandPool(nullptr), mCommandBuffer(nullptr), mRenderFence(nullptr), mAvailableSemaphore(nullptr), mPerspectiveDescriptorSet(nullptr) {}

void Frame::cleanup() {
    mPerspectiveDescriptorSet.clear();
    mPerspectiveBuffer.cleanup();
    mAvailableSemaphore.clear();
    mRenderFence.clear();
    mCommandBuffer.clear();
    mCommandPool.clear();
}
