#include <renderer.h>
#include <renderer_infrastructure.h>
#include <vk_initializers.h>
#include <vk_images.h>

#include <VkBootstrap.h>
#include <fmt/core.h>

std::filesystem::path shaderDir = SHADERS_PATH;

RendererInfrastructure::RendererInfrastructure(Renderer* renderer) :
	mRenderer(renderer),
    mSwapchainBundle(nullptr),
    mDescriptorAllocator(DescriptorAllocatorGrowable(renderer))
{
    mFrames.resize(FRAME_OVERLAP);
}   

void RendererInfrastructure::init() {
    initSwapchain();
    initDescriptors();
	initFrames();   
}

void RendererInfrastructure::initDescriptors()
{
    PbrMaterial::createResourcesDescriptorSetLayout(mRenderer);

    std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = {
        { vk::DescriptorType::eUniformBuffer, 1 },          // Scene UBO
        { vk::DescriptorType::eCombinedImageSampler, 1 },   // Skybox Cubemap
    };
    mDescriptorAllocator.init(1, sizes);
}

void RendererInfrastructure::initFrames()
{
    // mRenderFence to control when the GPU has finished rendering the frame, start signalled so we can wait on it on the first frame
    vk::FenceCreateInfo fenceCreateInfo = vkinit::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    vk::CommandPoolCreateInfo commandPoolInfo = vkinit::commandPoolCreateInfo(mRenderer->mRendererCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphoreCreateInfo();
    
    for (Frame& frame : mFrames) {
		frame.mRenderFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);
		frame.mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);
        vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(*frame.mCommandPool, 1);
        frame.mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);
        frame.mAvailableSemaphore = mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo);
    }
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
    vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphoreCreateInfo();
    for (int i = 0; i < vkbSwapchain.get_images().value().size(); i++) {
        mSwapchainBundle.mImages.emplace_back(
            vkbSwapchain.get_images().value()[i],
            mRenderer->mRendererCore.mDevice.createImageView(vkinit::imageViewCreateInfo(mSwapchainBundle.mFormat, vkbSwapchain.get_images().value()[i], vk::ImageAspectFlagBits::eColor)),
            mRenderer->mRendererCore.mDevice.createSemaphore(semaphoreCreateInfo)
        );
    }

    mDrawImage = mRenderer->mResourceManager.createImage(
        vk::Extent3D{ mRenderer->mRendererCore.mWindowExtent, 1 }, 
        vk::Format::eR16G16B16A16Sfloat, 
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment, 
        false, true, false);
    mIntermediateImage = mRenderer->mResourceManager.createImage(
        vk::Extent3D{ mRenderer->mRendererCore.mWindowExtent, 1 },
        vk::Format::eR16G16B16A16Sfloat,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
        false, false, false);
    mDepthImage = mRenderer->mResourceManager.createImage(
        mDrawImage.imageExtent,
        vk::Format::eD32Sfloat,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        false, true, false);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        for (int i = 0; i < mSwapchainBundle.mImages.size(); i++) {
            vkutil::transitionImage(*cmd, mSwapchainBundle.mImages[i].image,
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eNone,
                vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
        }
        vkutil::transitionImage(cmd, *mDrawImage.image,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        vkutil::transitionImage(*cmd, *mIntermediateImage.image,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
        vkutil::transitionImage(cmd, *mDepthImage.image,
            vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);
    });
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
    vk::raii::ShaderModule fragShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "mesh/mesh.frag.spv", mRenderer->mRendererCore.mDevice); 
    vk::raii::ShaderModule vertexShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "mesh/mesh.vert.spv", mRenderer->mRendererCore.mDevice);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> descriptorLayouts = { 
        *mRenderer->mSceneManager.mSceneResources.mSceneDescriptorSetLayout, 
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
    pipelineBuilder.setCullMode(cullMode, vk::FrontFace::eCounterClockwise);
    pipelineBuilder.enableMultisampling();
    pipelineBuilder.enableSampleShading(); // [TODO] Enable both
    transparency ? pipelineBuilder.enableBlendingAdditive() : pipelineBuilder.disableBlending();
    transparency ? pipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual) : pipelineBuilder.enableDepthtest(true, vk::CompareOp::eGreaterOrEqual);;
    pipelineBuilder.enableDepthtest(true, vk::CompareOp::eGreaterOrEqual);
    pipelineBuilder.setColorAttachmentFormat(mDrawImage.imageFormat);
    pipelineBuilder.setDepthFormat(mDepthImage.imageFormat);
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

void RendererInfrastructure::createSkyboxPipeline()
{
    vk::raii::ShaderModule fragShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "skybox/skybox.frag.spv", mRenderer->mRendererCore.mDevice);
    vk::raii::ShaderModule vertexShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "skybox/skybox.vert.spv", mRenderer->mRendererCore.mDevice);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SkyBoxPushConstants);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> descriptorLayouts = {
        *mRenderer->mSceneManager.mSceneResources.mSceneDescriptorSetLayout,
        *mRenderer->mSceneManager.mSkybox.mSkyboxDescriptorSetLayout
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkinit::pipelineLayoutCreateInfo();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorLayouts.data();
    pipelineLayoutCreateInfo.setLayoutCount = descriptorLayouts.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    vk::raii::PipelineLayout pipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);

    GraphicsPipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(*vertexShader, *fragShader);
    pipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    pipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
    pipelineBuilder.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise);
    pipelineBuilder.enableMultisampling();
    pipelineBuilder.enableSampleShading(); // [TODO] Enable both
    pipelineBuilder.enableBlendingSkybox();
    pipelineBuilder.setColorAttachmentFormat(mRenderer->mRendererInfrastructure.mDrawImage.imageFormat);
    pipelineBuilder.setDepthFormat(mRenderer->mRendererInfrastructure.mDepthImage.imageFormat);
    pipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual);
    pipelineBuilder.mPipelineLayout = *pipelineLayout;

    mRenderer->mSceneManager.mSkybox.mSkyboxPipeline = PipelineBundle{
        std::move(pipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice)),
        std::move(pipelineLayout)
    };
}

void RendererInfrastructure::cleanup() {
    PbrMaterial::mResourcesDescriptorSetLayout.clear();
    for (auto& frame : mFrames) {
        frame.cleanup();
    }
    mMaterialPipelines.clear();
    mComputePipelines.clear();
    destroySwapchain();
    mDescriptorAllocator.cleanup();
}