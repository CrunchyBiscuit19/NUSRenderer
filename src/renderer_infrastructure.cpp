#include <renderer.h>
#include <renderer_infrastructure.h>
#include <vk_initializers.h>
#include <vk_images.h>

#include <VkBootstrap.h>

std::filesystem::path shaderDir = SHADERS_PATH;

RendererInfrastructure::RendererInfrastructure(Renderer* renderer) :
	mRenderer(renderer),
    mSwapchain(nullptr),
    mDescriptorAllocator(DescriptorAllocatorGrowable(renderer))
{
    mFrames.resize(FRAME_OVERLAP);
}   

void RendererInfrastructure::init() {
    createSwapchain();
    initCommands();
    initDescriptors();
	initSyncStructures();   
}

void RendererInfrastructure::initCommands() 
{
    const vk::CommandPoolCreateInfo commandPoolInfo = vkinit::commandPoolCreateInfo(mRenderer->mRendererCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    for (Frame& frame : mFrames) {
		frame.mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);
        vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(*frame.mCommandPool, 1);
        frame.mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);
    }
}

void RendererInfrastructure::initDescriptors()
{
    PbrMaterial::createResourcesDescriptorSetLayout(mRenderer);

    std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = {
        { vk::DescriptorType::eUniformBuffer, 1 }, // Scene UBO
        { vk::DescriptorType::eCombinedImageSampler, 1 }, // Skybox UBO
    };
    mDescriptorAllocator.init(1, sizes);
}

void RendererInfrastructure::initSyncStructures()
{
    // 1 fence to control when the gpu has finished rendering the frame,
    // 2 semaphores to syncronize rendering with swapchain
    // Fence to start signalled so we can wait on it on the first frame
    vk::FenceCreateInfo fenceCreateInfo = vkinit::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphoreCreateInfo();
    for (Frame& frame : mFrames) {
		frame.mRenderFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);
		frame.mSwapchainSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
		frame.mRenderSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
    }
}

void RendererInfrastructure::createSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{ *mRenderer->mRendererCore.mChosenGPU, *mRenderer->mRendererCore.mDevice, *mRenderer->mRendererCore.mSurface };

    mSwapchainImageFormat = vk::Format::eB8G8R8A8Unorm;
    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = static_cast<VkFormat>(mSwapchainImageFormat), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear) })
        .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eFifo))
        .set_desired_extent(mRenderer->mRendererCore.mWindowExtent.width, mRenderer->mRendererCore.mWindowExtent.height)
        .add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
        .build()
        .value();

    mSwapchainExtent = vkbSwapchain.extent;
    vk::raii::SwapchainKHR swapchain(mRenderer->mRendererCore.mDevice, vkbSwapchain.swapchain);
    mSwapchain = std::move(swapchain);
    for (auto& imageView : vkbSwapchain.get_image_views().value())
        mSwapchainImageViews.emplace_back(vk::raii::ImageView(mRenderer->mRendererCore.mDevice, imageView));

    mDrawImage.imageFormat = vk::Format::eR16G16B16A16Sfloat;
    mDrawImage.imageExtent = vk::Extent3D{
        mRenderer->mRendererCore.mWindowExtent.width,
        mRenderer->mRendererCore.mWindowExtent.height,
        1 };
    vk::ImageUsageFlags drawImageUsages{};
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferDst;
    drawImageUsages |= vk::ImageUsageFlagBits::eStorage;
    drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;
    vk::ImageCreateInfo drawImageCreateInfo = vkinit::imageCreateInfo(mDrawImage.imageFormat, drawImageUsages, mDrawImage.imageExtent);
    VkImageCreateInfo drawImageCreateInfo1 = static_cast<VkImageCreateInfo>(drawImageCreateInfo);

    VmaAllocationCreateInfo imageAllocateInfo = {};
    imageAllocateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imageAllocateInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal);

    VkImage swapchainDrawImage;
    vmaCreateImage(mRenderer->mRendererCore.mVmaAllocator, &drawImageCreateInfo1, &imageAllocateInfo, &swapchainDrawImage, &mDrawImage.allocation, nullptr);
    mDrawImage.image = vk::raii::Image(mRenderer->mRendererCore.mDevice, swapchainDrawImage);
	mDrawImage.allocator = &mRenderer->mRendererCore.mVmaAllocator;

    vk::ImageViewCreateInfo drawImageViewCreateInfo = vkinit::imageViewCreateInfo(mDrawImage.imageFormat, *mDrawImage.image, vk::ImageAspectFlagBits::eColor);
    mDrawImage.imageView = mRenderer->mRendererCore.mDevice.createImageView(drawImageViewCreateInfo);

    mDepthImage.imageFormat = vk::Format::eD32Sfloat;
    mDepthImage.imageExtent = mDrawImage.imageExtent;
    vk::ImageUsageFlags depthImageUsages = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    vk::ImageCreateInfo depthImageCreateInfo = vkinit::imageCreateInfo(mDepthImage.imageFormat, depthImageUsages, mDepthImage.imageExtent);
    VkImageCreateInfo depthImageCreateInfo1 = static_cast<VkImageCreateInfo>(depthImageCreateInfo);

    VkImage swapchainDepthImage;
    vmaCreateImage(mRenderer->mRendererCore.mVmaAllocator, &depthImageCreateInfo1, &imageAllocateInfo, &swapchainDepthImage, &mDepthImage.allocation, nullptr);
    mDepthImage.image = vk::raii::Image(mRenderer->mRendererCore.mDevice, swapchainDepthImage);
    mDepthImage.allocator = &mRenderer->mRendererCore.mVmaAllocator;

    vk::ImageViewCreateInfo depthImageViewCreateInfo = vkinit::imageViewCreateInfo(mDepthImage.imageFormat, *mDepthImage.image, vk::ImageAspectFlagBits::eDepth);
    mDepthImage.imageView = mRenderer->mRendererCore.mDevice.createImageView(depthImageViewCreateInfo);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        for (int frame = 0; frame < FRAME_OVERLAP; frame++) {
            vkutil::transitionImage(*cmd, mSwapchain.getImages()[frame],
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
        }
    });
}

void RendererInfrastructure::destroySwapchain()
{
    mDepthImage.cleanup();
    mDrawImage.cleanup();

    mSwapchain.clear();
    mSwapchainImageViews.clear();
}

void RendererInfrastructure::resizeSwapchain()
{
    mRenderer->mRendererCore.mDevice.waitIdle();

    destroySwapchain();

    int w, h;
    SDL_GetWindowSize(mRenderer->mRendererCore.mWindow, &w, &h);
    mRenderer->mRendererCore.mWindowExtent.width = w;
    mRenderer->mRendererCore.mWindowExtent.height = h;

    createSwapchain();

    mResizeRequested = false;
}

PipelineBundle* RendererInfrastructure::getMaterialPipeline(PipelineOptions pipelineOptions)
{
    if (mMaterialPipelines.contains(pipelineOptions)) {
        return &mMaterialPipelines[pipelineOptions];
    }
    createMaterialPipeline(pipelineOptions);
    return &mMaterialPipelines[pipelineOptions];
}

void RendererInfrastructure::createMaterialPipeline(PipelineOptions pipelineOptions)
{
    vk::raii::ShaderModule fragShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "mesh.frag.spv", mRenderer->mRendererCore.mDevice); 
    vk::raii::ShaderModule vertexShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "mesh.vert.spv", mRenderer->mRendererCore.mDevice);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> descriptorLayouts = { 
        *mRenderer->mSceneManager.mSceneEncapsulation.mSceneDescriptorSetLayout, 
        *PbrMaterial::mResourcesDescriptorSetLayout 
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkinit::pipelineLayoutCreateInfo();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorLayouts.data();
    pipelineLayoutCreateInfo.setLayoutCount = descriptorLayouts.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    vk::raii::PipelineLayout pipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);

    vk::CullModeFlags cullMode;
    (pipelineOptions.doubleSided) ? (cullMode = vk::CullModeFlagBits::eNone) : (cullMode = vk::CullModeFlagBits::eBack);
    bool transparency;
    (pipelineOptions.alphaMode == fastgltf::AlphaMode::Blend) ? (transparency = true) : (transparency = false);

    GraphicsPipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(*vertexShader, *fragShader);
    pipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    pipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
    pipelineBuilder.setCullMode(cullMode, vk::FrontFace::eClockwise);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthtest(true, vk::CompareOp::eGreaterOrEqual);
    pipelineBuilder.setColorAttachmentFormat(mDrawImage.imageFormat);
    pipelineBuilder.setDepthFormat(mDepthImage.imageFormat);
    if (transparency) {
        pipelineBuilder.enableBlendingAdditive();
        pipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual);
    }
    pipelineBuilder.mPipelineLayout = *pipelineLayout;

    PipelineBundle materialPipeline = PipelineBundle {
        std::move(pipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice)),
        std::move(pipelineLayout) 
    }; 
    mMaterialPipelines.emplace(pipelineOptions, std::move(materialPipeline));
}

void RendererInfrastructure::createComputePipeline(PipelineOptions pipelineOptions)
{
    vk::raii::ShaderModule computeShaderModule = vkutil::loadShaderModule("Placeholder path", mRenderer->mRendererCore.mDevice);

    vk::PipelineLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.setLayoutCount = 0;
    computeLayoutInfo.pSetLayouts = nullptr;

    vk::raii::PipelineLayout computePipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(computeLayoutInfo);

    ComputePipelineBuilder computePipelineBuilder;
    computePipelineBuilder.setShader(*computeShaderModule);
    computePipelineBuilder.mPipelineLayout = *computePipelineLayout;

    PipelineBundle computePipeline = PipelineBundle{
        std::move(computePipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice)),
        std::move(computePipelineLayout) 
    }; 
    mComputePipelines.emplace(pipelineOptions, std::move(computePipeline));
}

void RendererInfrastructure::destroyPipelines()
{
    mMaterialPipelines.clear();
    mComputePipelines.clear();
}

void RendererInfrastructure::cleanup() {
    PbrMaterial::mResourcesDescriptorSetLayout.clear();
    for (auto& frame : mFrames) {
        frame.cleanup();
    }
    destroyPipelines();
    destroySwapchain();
    mDescriptorAllocator.cleanup();
}