#include <renderer.h>
#include <vk_images.h>
#include <vk_initializers.h>

#include <fmt/core.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <ranges>

Renderer::Renderer():
    mRendererCore(RendererCore(this)),
    mRendererInfrastructure(RendererInfrastructure(this)),
    mResourceManager(ResourceManager(this)),
    mSceneManager(SceneManager(this)),
    mImmSubmit(ImmSubmit(this)),
    mGUI(GUI(this)),
    mContext(),
    mInstance(nullptr),
    mDebugMessenger(nullptr),
    mChosenGPU(nullptr),
    mDevice(nullptr),
    mSurface(nullptr),
    mComputeQueue(nullptr),
    mGraphicsQueue(nullptr),
    mSwapchain(nullptr),
    mDescriptorAllocator(DescriptorAllocatorGrowable(this)),
    mSceneEncapsulation(SceneEncapsulation(this)),
    mDefaultSampler(nullptr)
{
    mFrames.resize(FRAME_OVERLAP);
    mCamera = Camera();
}

void Renderer::init()
{
    mRendererCore.init();
    mRendererInfrastructure.init();
    mImmSubmit.init();
    mResourceManager.init();
    mSceneManager.init();
    mGUI.init();
    mCamera.init();
    PbrMaterial::createResourcesDescriptorSetLayout(this);
    GLTFModel::createInstanceDescriptorSetLayout(this);
}

void Renderer::run()
{
    SDL_Event e;
    std::optional<uint64_t> programEndFrameNumber = std::nullopt;

    while (true) {
        auto start = std::chrono::system_clock::now();

        if (programEndFrameNumber.has_value() && (mFrameNumber < programEndFrameNumber.value())) {
            break;
        }

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                for (auto& model : mModels | std::views::values) { model.markDelete(); }
                programEndFrameNumber = mFrameNumber + FRAME_OVERLAP + 1;
            }
            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                    mStopRendering = true;
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                    mStopRendering = false;
            }
            mCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (mStopRendering) { // Do not draw if minimized
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Throttle the speed to avoid endless spinning
            continue;
        }

        SDL_SetRelativeMouseMode(mCamera.relativeMode);
        if (mResizeRequested) { mRendererInfrastructure.resizeSwapchain(); }

        mGUI.imguiFrame();

        drawUpdate();
        drawCleanup();
        draw();

        mFrameNumber++;

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.mFrametime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
    }
}

void Renderer::cleanup()
{
    GLTFModel::mInstancesDescriptorSetLayout.clear();
    PbrMaterial::mResourcesDescriptorSetLayout.clear();
    SceneEncapsulation::mSceneDescriptorSetLayout.clear();
    for (auto& frame : mFrames) {
        frame.cleanup();
    }
    mGUI.cleanup();
    mSceneManager.cleanup();
    mImmSubmit.cleanup();
    mResourceManager.cleanup();
    mRendererInfrastructure.cleanup();
    mRendererCore.cleanup();
}

void Renderer::draw()
{
    auto start = std::chrono::system_clock::now();

    mDevice.waitForFences(*getCurrentFrame().mRenderFence, true, 1e9);  // Wait until the gpu has finished rendering the frame of this index (become signalled)
    mDevice.resetFences(*getCurrentFrame().mRenderFence); // Flip to unsignalled

    // Request image from the swapchain, mSwapchainSemaphore signalled only when next image is acquired.
    uint32_t swapchainImageIndex = 0;
    try {
        auto output = mSwapchain.acquireNextImage(1e9, *getCurrentFrame().mSwapchainSemaphore, nullptr);
        swapchainImageIndex = output.second;
    }
    catch (vk::OutOfDateKHRError e) {
        mResizeRequested = true;
        return;
    }

    vk::CommandBuffer cmd = *getCurrentFrame().mCommandBuffer;
    cmd.reset();
    vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmdBeginInfo);

    // Multiply by render scale for dynamic resolution
    // When resizing bigger, don't make swapchain extent go beyond draw image extent
    mDrawImage.imageExtent.height = std::min(mSwapchainExtent.height, mDrawImage.imageExtent.height);
    mDrawImage.imageExtent.width = std::min(mSwapchainExtent.width, mDrawImage.imageExtent.width);

    // Transition stock and draw image into transfer layouts
    vkutil::transitionImage(cmd, *mDefaultImages[DefaultImage::Blue].image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
    vkutil::transitionImage(cmd, *mDrawImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    // Copy stock image as the initial colour for the draw image (the background)
    vkutil::copyImage(cmd, *mDefaultImages[DefaultImage::Blue].image, *mDrawImage.image,
        vk::Extent2D{ mDefaultImages[DefaultImage::Blue].imageExtent.width, mDefaultImages[DefaultImage::Blue].imageExtent.height, },
        vk::Extent2D{ mDrawImage.imageExtent.width, mDrawImage.imageExtent.height, });

    // Transition to color output for drawing geometry
    vkutil::transitionImage(cmd, *mDrawImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
    vkutil::transitionImage(cmd, *mDepthImage.image,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

    drawGeometry(cmd);

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transitionImage(cmd, *mDrawImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    vkutil::transitionImage(cmd, mSwapchain.getImages()[swapchainImageIndex],
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal);

    // Copy draw image into the swapchain image
    vkutil::copyImage(cmd, *mDrawImage.image, mSwapchain.getImages()[swapchainImageIndex],
        vk::Extent2D{ mDrawImage.imageExtent.width, mDrawImage.imageExtent.height, },
        mSwapchainExtent);

    // Set swapchain image to be attachment optimal to draw it
    vkutil::transitionImage(cmd, mSwapchain.getImages()[swapchainImageIndex],
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

    drawGui(cmd, *mSwapchainImageViews[swapchainImageIndex]);

    // Set swapchain image layout to presentable layout
    vkutil::transitionImage(cmd, mSwapchain.getImages()[swapchainImageIndex],
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

    cmd.end();

    // Prepare the submission to the queue. (Reading semaphore states)
    vk::CommandBufferSubmitInfo cmdinfo = vkinit::commandBufferSubmitInfo(cmd);
    vk::SemaphoreSubmitInfo waitInfo = vkinit::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput, *getCurrentFrame().mSwapchainSemaphore);
    vk::SemaphoreSubmitInfo signalInfo = vkinit::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eAllGraphics, *getCurrentFrame().mRenderSemaphore);
    const vk::SubmitInfo2 submit = vkinit::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

    // Submit command buffer to the queue and execute it.
    // _renderFence will block CPU from going to next frame, stays unsignalled until this is done.
    // _swapchainSemaphore gets waited on until it is signalled when the next image is acquired.
    // _renderSemaphore will be signalled by this function when this queue's commands are executed.
    mGraphicsQueue.submit2(submit, *getCurrentFrame().mRenderFence);

    // Prepare present.
    // Wait on the _renderSemaphore for queue commands to finish before image is presented.
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &(*mSwapchain);
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &(*getCurrentFrame().mRenderSemaphore);
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    try {
        mGraphicsQueue.presentKHR(presentInfo);
    }
    catch (vk::OutOfDateKHRError e) {
        mResizeRequested = true;
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::drawGui(vk::CommandBuffer cmd, vk::ImageView targetImageView)
{
    vk::RenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(targetImageView, nullptr, vk::ImageLayout::eGeneral);
    const vk::RenderingInfo renderInfo = vkinit::renderingInfo(mSwapchainExtent, &colorAttachment, nullptr);

    cmd.beginRendering(renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
}

void Renderer::drawGeometry(vk::CommandBuffer cmd)
{
    mStats.mDrawCallCount = 0;

    // [TODO] Sorting and culling
    
    vk::RenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(*mDrawImage.imageView, nullptr, vk::ImageLayout::eGeneral);
    vk::RenderingAttachmentInfo depthAttachment = vkinit::depthAttachmentInfo(*mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkinit::renderingInfo(vk::Extent2D{ mDrawImage.imageExtent.width, mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    vk::Pipeline lastPipeline = nullptr;
    std::shared_ptr<PbrMaterial> lastMaterial = nullptr;
    vk::Buffer lastInstancesBuffer = nullptr;
    vk::Buffer lastIndexBuffer = nullptr;

    for (auto& renderItem : mRenderItems) {
        if (*renderItem.primitive->material->mPipeline->pipeline != lastPipeline) {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->pipeline);
            lastPipeline = *renderItem.primitive->material->mPipeline->pipeline;

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->layout, 0, *mSceneEncapsulation.mSceneDescriptorSet, nullptr);

            vk::Viewport viewport = {
                0,
                0,
                static_cast<float>(mDrawImage.imageExtent.width),
                static_cast<float>(mDrawImage.imageExtent.height),
                0.f,
                1.f,
            };
            cmd.setViewport(0, viewport);
            vk::Rect2D scissor = {
                vk::Offset2D {0, 0},
                vk::Extent2D {mDrawImage.imageExtent.width, mDrawImage.imageExtent.height},
            };
            cmd.setScissor(0, scissor);
        }

        if (renderItem.primitive->material != lastMaterial) {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->layout, 1, *renderItem.primitive->material->mResourcesDescriptorSet, nullptr);
            lastMaterial = renderItem.primitive->material;
        }
        
        if (*renderItem.model->mInstancesBuffer.buffer != lastInstancesBuffer) {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->layout, 2, *renderItem.model->mInstancesDescriptorSet, nullptr);
            lastInstancesBuffer = *renderItem.model->mInstancesBuffer.buffer;
        }

        if (*renderItem.mesh->mIndexBuffer.buffer != lastIndexBuffer) {
            cmd.bindIndexBuffer(*renderItem.mesh->mIndexBuffer.buffer, 0, vk::IndexType::eUint32);
            lastIndexBuffer = *renderItem.mesh->mIndexBuffer.buffer;
        }

        mPushConstants.vertexBuffer = renderItem.vertexBufferAddress;
        mPushConstants.worldMatrix = renderItem.transform;
        mPushConstants.materialBuffer = renderItem.materialConstantBufferAddress;
        mPushConstants.materialIndex = renderItem.primitive->material->mMaterialIndex;
        cmd.pushConstants<PushConstants>(*renderItem.primitive->material->mPipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, mPushConstants);

        cmd.drawIndexed(renderItem.primitive->indexCount, renderItem.model->mInstances.size(), renderItem.primitive->indexStart, 0, 0);

        mStats.mDrawCallCount++;
    };

    cmd.endRendering();
}

void Renderer::drawCleanup()
{
    mRegenRenderItems = false;
}

void Renderer::drawUpdate()
{
    const auto start = std::chrono::system_clock::now();

    mSceneManager.deleteInstances();
    mSceneManager.deleteModels();

    // Only when add / remove models, then need to regen render items
    if (mRegenRenderItems) { 
        mRenderItems.clear();
        mSceneManager.generateRenderItems(); 
        mRegenRenderItems = false; 
    }
    for (auto& model : mModels | std::views::values) {
        model.updateInstances();
    }
    mSceneManager.updateScene();
     
    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

FrameResources::FrameResources() :
    mCommandPool(nullptr),
    mCommandBuffer(nullptr),
    mSwapchainSemaphore(nullptr),
    mRenderSemaphore(nullptr),
    mRenderFence(nullptr)
{}

void FrameResources::cleanup()
{
    mRenderFence.clear();
    mRenderSemaphore.clear();
    mSwapchainSemaphore.clear();
    mCommandBuffer.clear();
    mCommandPool.clear();
}
