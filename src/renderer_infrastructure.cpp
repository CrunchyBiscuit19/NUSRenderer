#include <renderer.h>
#include <renderer_infrastructure.h>
#include <vk_initializers.h>

#include <VkBootstrap.h>

std::filesystem::path shaderDir = SHADERS_PATH;

RendererInfrastructure::RendererInfrastructure(Renderer* renderer) :
	mRenderer(renderer)
{
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
    for (FrameResources& frame : mRenderer->mFrames) {
		frame.mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);
        vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(*frame.mCommandPool, 1);
        frame.mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);
    }
}

void RendererInfrastructure::initDescriptors()
{
    std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = {
        { vk::DescriptorType::eUniformBuffer, 1 },
    };
    mRenderer->mDescriptorAllocator.init(1, sizes);
}

void RendererInfrastructure::initSyncStructures()
{
    // 1 fence to control when the gpu has finished rendering the frame,
    // 2 semaphores to syncronize rendering with swapchain
    // Fence to start signalled so we can wait on it on the first frame
    vk::FenceCreateInfo fenceCreateInfo = vkinit::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphoreCreateInfo();
    for (FrameResources& frame : mRenderer->mFrames) {
		frame.mRenderFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);
		frame.mSwapchainSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
		frame.mRenderSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
    }
}

void RendererInfrastructure::createSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{ *mRenderer->mRendererCore.mChosenGPU, *mRenderer->mRendererCore.mDevice, *mRenderer->mRendererCore.mSurface };

    mRenderer->mSwapchainImageFormat = vk::Format::eB8G8R8A8Unorm;
    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = static_cast<VkFormat>(mRenderer->mSwapchainImageFormat), .colorSpace = static_cast<VkColorSpaceKHR>(vk::ColorSpaceKHR::eSrgbNonlinear) })
        .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eFifo))
        .set_desired_extent(mRenderer->mRendererCore.mWindowExtent.width, mRenderer->mRendererCore.mWindowExtent.height)
        .add_image_usage_flags(static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eTransferDst))
        .build()
        .value();

    mRenderer->mSwapchainExtent = vkbSwapchain.extent;
    vk::raii::SwapchainKHR swapchain(mRenderer->mRendererCore.mDevice, vkbSwapchain.swapchain);
    mRenderer->mSwapchain = std::move(swapchain);
    for (auto& imageView : vkbSwapchain.get_image_views().value())
        mRenderer->mSwapchainImageViews.emplace_back(vk::raii::ImageView(mRenderer->mRendererCore.mDevice, imageView));

    mRenderer->mDrawImage.imageFormat = vk::Format::eR16G16B16A16Sfloat;
    mRenderer->mDrawImage.imageExtent = vk::Extent3D{
        mRenderer->mRendererCore.mWindowExtent.width,
        mRenderer->mRendererCore.mWindowExtent.height,
        1 };
    vk::ImageUsageFlags drawImageUsages{};
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferSrc;
    drawImageUsages |= vk::ImageUsageFlagBits::eTransferDst;
    drawImageUsages |= vk::ImageUsageFlagBits::eStorage;
    drawImageUsages |= vk::ImageUsageFlagBits::eColorAttachment;
    vk::ImageCreateInfo drawImageCreateInfo = vkinit::imageCreateInfo(mRenderer->mDrawImage.imageFormat, drawImageUsages, mRenderer->mDrawImage.imageExtent);
    VkImageCreateInfo drawImageCreateInfo1 = static_cast<VkImageCreateInfo>(drawImageCreateInfo);

    VmaAllocationCreateInfo imageAllocateInfo = {};
    imageAllocateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imageAllocateInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal);

    VkImage swapchainDrawImage;
    vmaCreateImage(mRenderer->mRendererCore.mVmaAllocator, &drawImageCreateInfo1, &imageAllocateInfo, &swapchainDrawImage, &mRenderer->mDrawImage.allocation, nullptr);
    mRenderer->mDrawImage.image = vk::raii::Image(mRenderer->mRendererCore.mDevice, swapchainDrawImage);
	mRenderer->mDrawImage.allocator = &mRenderer->mRendererCore.mVmaAllocator;

    vk::ImageViewCreateInfo drawImageViewCreateInfo = vkinit::imageViewCreateInfo(mRenderer->mDrawImage.imageFormat, *mRenderer->mDrawImage.image, vk::ImageAspectFlagBits::eColor);
    mRenderer->mDrawImage.imageView = mRenderer->mRendererCore.mDevice.createImageView(drawImageViewCreateInfo);

    mRenderer->mDepthImage.imageFormat = vk::Format::eD32Sfloat;
    mRenderer->mDepthImage.imageExtent = mRenderer->mDrawImage.imageExtent;
    vk::ImageUsageFlags depthImageUsages = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    vk::ImageCreateInfo depthImageCreateInfo = vkinit::imageCreateInfo(mRenderer->mDepthImage.imageFormat, depthImageUsages, mRenderer->mDepthImage.imageExtent);
    VkImageCreateInfo depthImageCreateInfo1 = static_cast<VkImageCreateInfo>(depthImageCreateInfo);

    VkImage swapchainDepthImage;
    vmaCreateImage(mRenderer->mRendererCore.mVmaAllocator, &depthImageCreateInfo1, &imageAllocateInfo, &swapchainDepthImage, &mRenderer->mDepthImage.allocation, nullptr);
    mRenderer->mDepthImage.image = vk::raii::Image(mRenderer->mRendererCore.mDevice, swapchainDepthImage);
    mRenderer->mDepthImage.allocator = &mRenderer->mRendererCore.mVmaAllocator;

    vk::ImageViewCreateInfo depthImageViewCreateInfo = vkinit::imageViewCreateInfo(mRenderer->mDepthImage.imageFormat, *mRenderer->mDepthImage.image, vk::ImageAspectFlagBits::eDepth);
    mRenderer->mDepthImage.imageView = mRenderer->mRendererCore.mDevice.createImageView(depthImageViewCreateInfo);
}

void RendererInfrastructure::destroySwapchain()
{
    mRenderer->mDepthImage.cleanup();
    mRenderer->mDrawImage.cleanup();

    mRenderer->mSwapchain.clear();
    mRenderer->mSwapchainImageViews.clear();
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

    mRenderer->mResizeRequested = false;
}

PipelineBundle* RendererInfrastructure::getMaterialPipeline(PipelineOptions pipelineOptions)
{
    if (mRenderer->mMaterialPipelines.contains(pipelineOptions)) {
        return &mRenderer->mMaterialPipelines[pipelineOptions];
    }
    createMaterialPipeline(pipelineOptions);
    return &mRenderer->mMaterialPipelines[pipelineOptions];
}

void RendererInfrastructure::createMaterialPipeline(PipelineOptions pipelineOptions)
{
    vk::raii::ShaderModule fragShader = vkutil::loadShaderModule(shaderDir / "mesh.frag.spv", mRenderer->mRendererCore.mDevice); // TODO Custom paths
    vk::raii::ShaderModule vertexShader = vkutil::loadShaderModule(shaderDir / "mesh.vert.spv", mRenderer->mRendererCore.mDevice);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> descriptorLayouts = { *SceneEncapsulation::mSceneDescriptorSetLayout, *PbrMaterial::mResourcesDescriptorSetLayout, *GLTFModel::mInstancesDescriptorSetLayout };
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
    pipelineBuilder.setColorAttachmentFormat(mRenderer->mDrawImage.imageFormat);
    pipelineBuilder.setDepthFormat(mRenderer->mDepthImage.imageFormat);
    if (transparency) {
        pipelineBuilder.enableBlendingAdditive();
        pipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual);
    }
    pipelineBuilder.mPipelineLayout = *pipelineLayout;

    PipelineBundle materialPipeline = PipelineBundle {
        std::move(pipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice)),
        std::move(pipelineLayout) 
    }; 
    mRenderer->mMaterialPipelines.emplace(pipelineOptions, std::move(materialPipeline));
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
    mRenderer->mComputePipelines.emplace(pipelineOptions, std::move(computePipeline));
}

void RendererInfrastructure::destroyPipelines()
{
    mRenderer->mMaterialPipelines.clear();
    mRenderer->mComputePipelines.clear();
}

void RendererInfrastructure::cleanup() {
    destroyPipelines();
    destroySwapchain();
    mRenderer->mDescriptorAllocator.cleanup();
}