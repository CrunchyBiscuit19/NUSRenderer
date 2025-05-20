#include <renderer.h>
#include <vk_images.h>
#include <vk_initializers.h>

#include <fmt/core.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <ranges>

Renderer::Renderer():
    mRendererCore(RendererCore(this)),
    mRendererInfrastructure(RendererInfrastructure(this)),
    mResourceManager(ResourceManager(this)),
    mSceneManager(SceneManager(this)),
    mImmSubmit(ImmSubmit(this)),
    mGUI(GUI(this))
{
    
    mCamera = Camera();
}

void Renderer::init()
{
    mRendererCore.init();
    mImmSubmit.init();
    mRendererInfrastructure.init();
    mResourceManager.init();
    mSceneManager.init();
    mGUI.init();
    mCamera.init();
}

void Renderer::run()
{
    SDL_Event e;
    std::optional<uint64_t> programEndFrameNumber = std::nullopt;

    while (true) {
        auto start = std::chrono::system_clock::now();

        if (programEndFrameNumber.has_value() && (mRendererInfrastructure.mFrameNumber < programEndFrameNumber.value())) {
            mRendererCore.mDevice.waitIdle();
            break;
        }

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                for (auto& model : mSceneManager.mModels | std::views::values) { model.markDelete(); }
                programEndFrameNumber = mRendererInfrastructure.mFrameNumber + FRAME_OVERLAP + 1;
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
        if (mRendererInfrastructure.mResizeRequested) { mRendererInfrastructure.resizeSwapchain(); }

        mGUI.imguiFrame();

        drawUpdate();
        drawCleanup();
        draw();

        mRendererInfrastructure.mFrameNumber++;

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.mFrametime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
    }
}

void Renderer::cleanup()
{
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

    mRendererCore.mDevice.waitForFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);  // Wait until the gpu has finished rendering the frame of this index (become signalled)
    mRendererCore.mDevice.resetFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence); // Flip to unsignalled

    // Request image from the swapchain, mSwapchainSemaphore signalled only when next image is acquired.
    uint32_t swapchainImageIndex = 0;
    try {
        auto output = mRendererInfrastructure.mSwapchain.acquireNextImage(1e9, *mRendererInfrastructure.getCurrentFrame().mSwapchainSemaphore, nullptr);
        swapchainImageIndex = output.second;
    }
    catch (vk::OutOfDateKHRError e) {
        mRendererInfrastructure.mResizeRequested = true;
        return;
    }

    vk::CommandBuffer cmd = *mRendererInfrastructure.getCurrentFrame().mCommandBuffer;
    cmd.reset();
    vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmdBeginInfo);

    // Resizing bigger window, don't make swapchain extent go beyond draw image extent
    mRendererInfrastructure.mDrawImage.imageExtent.height = std::min(mRendererInfrastructure.mSwapchainExtent.height, mRendererInfrastructure.mDrawImage.imageExtent.height);
    mRendererInfrastructure.mDrawImage.imageExtent.width = std::min(mRendererInfrastructure.mSwapchainExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.width);

    // Transition stock and draw image into transfer layouts
    vkutil::transitionImage(cmd, *mResourceManager.mDefaultImages[DefaultImage::Blue].image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
    vkutil::transitionImage(cmd, *mRendererInfrastructure.mDrawImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    // Copy stock image as the initial colour for the draw image (the background)
    vkutil::copyImage(cmd, *mResourceManager.mDefaultImages[DefaultImage::Blue].image, *mRendererInfrastructure.mDrawImage.image,
        vk::Extent2D{ mResourceManager.mDefaultImages[DefaultImage::Blue].imageExtent.width, mResourceManager.mDefaultImages[DefaultImage::Blue].imageExtent.height, },
        vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height, });

    // Transition to color output for drawing geometry
    vkutil::transitionImage(cmd, *mRendererInfrastructure.mDrawImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
    vkutil::transitionImage(cmd, *mRendererInfrastructure.mDepthImage.image,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal);

    drawGeometry(cmd);

    // Transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transitionImage(cmd, *mRendererInfrastructure.mDrawImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    vkutil::transitionImage(cmd, mRendererInfrastructure.mSwapchain.getImages()[swapchainImageIndex],
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal);

    // Copy draw image into the swapchain image
    vkutil::copyImage(cmd, *mRendererInfrastructure.mDrawImage.image, mRendererInfrastructure.mSwapchain.getImages()[swapchainImageIndex],
        vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height, },
        mRendererInfrastructure.mSwapchainExtent);

    // Set swapchain image to be attachment optimal to draw it
    vkutil::transitionImage(cmd, mRendererInfrastructure.mSwapchain.getImages()[swapchainImageIndex],
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

    drawGui(cmd, *mRendererInfrastructure.mSwapchainImageViews[swapchainImageIndex]);

    // Set swapchain image layout to presentable layout
    vkutil::transitionImage(cmd, mRendererInfrastructure.mSwapchain.getImages()[swapchainImageIndex],
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

    cmd.end();

    // Prepare the submission to the queue. (Reading semaphore states)
    vk::CommandBufferSubmitInfo cmdinfo = vkinit::commandBufferSubmitInfo(cmd);
    vk::SemaphoreSubmitInfo waitInfo = vkinit::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput, *mRendererInfrastructure.getCurrentFrame().mSwapchainSemaphore);
    vk::SemaphoreSubmitInfo signalInfo = vkinit::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eAllGraphics, *mRendererInfrastructure.getCurrentFrame().mRenderSemaphore);
    const vk::SubmitInfo2 submit = vkinit::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

    // Submit command buffer to the queue and execute it.
    // _renderFence will block CPU from going to next frame, stays unsignalled until this is done.
    // _swapchainSemaphore gets waited on until it is signalled when the next image is acquired.
    // _renderSemaphore will be signalled by this function when this queue's commands are executed.
    mRendererCore.mGraphicsQueue.submit2(submit, *mRendererInfrastructure.getCurrentFrame().mRenderFence);

    // Prepare present.
    // Wait on the _renderSemaphore for queue commands to finish before image is presented.
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &(*mRendererInfrastructure.mSwapchain);
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &(*mRendererInfrastructure.getCurrentFrame().mRenderSemaphore);
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    try {
        mRendererCore.mGraphicsQueue.presentKHR(presentInfo);
    }
    catch (vk::OutOfDateKHRError e) {
        mRendererInfrastructure.mResizeRequested = true;
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::drawGui(vk::CommandBuffer cmd, vk::ImageView targetImageView)
{
    vk::RenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(targetImageView, nullptr, vk::ImageLayout::eGeneral);
    const vk::RenderingInfo renderInfo = vkinit::renderingInfo(mRendererInfrastructure.mSwapchainExtent, &colorAttachment, nullptr);

    cmd.beginRendering(renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
}

void Renderer::drawGeometry(vk::CommandBuffer cmd)
{
    mStats.mDrawCallCount = 0;

    // [TODO] Sorting and culling
    
    vk::RenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, nullptr, vk::ImageLayout::eGeneral);
    vk::RenderingAttachmentInfo depthAttachment = vkinit::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkinit::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    vk::Pipeline lastPipeline = nullptr;
    std::shared_ptr<PbrMaterial> lastMaterial = nullptr;
    vk::Buffer lastIndexBuffer = nullptr;

    for (auto& renderItem : mSceneManager.mRenderItems) {
        if (*renderItem.primitive->material->mPipeline->pipeline != lastPipeline) {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->pipeline);
            lastPipeline = *renderItem.primitive->material->mPipeline->pipeline;

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->layout, 0, *mSceneManager.mSceneEncapsulation.mSceneDescriptorSet, nullptr);

            vk::Viewport viewport = {
                0,
                0,
                static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.width),
                static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.height),
                0.f,
                1.f,
            };
            cmd.setViewport(0, viewport);
            vk::Rect2D scissor = {
                vk::Offset2D {0, 0},
                vk::Extent2D {mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height},
            };
            cmd.setScissor(0, scissor);
        }

        if (renderItem.primitive->material != lastMaterial) {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->layout, 1, *renderItem.primitive->material->mResourcesDescriptorSet, nullptr);
            lastMaterial = renderItem.primitive->material;
        }

        if (*renderItem.mesh->mIndexBuffer.buffer != lastIndexBuffer) {
            cmd.bindIndexBuffer(*renderItem.mesh->mIndexBuffer.buffer, 0, vk::IndexType::eUint32);
            lastIndexBuffer = *renderItem.mesh->mIndexBuffer.buffer;
        }

        mSceneManager.mPushConstants.vertexBuffer = renderItem.vertexBufferAddress;
        mSceneManager.mPushConstants.instanceBuffer = renderItem.instancesBufferAddress;
        mSceneManager.mPushConstants.materialBuffer = renderItem.materialConstantsBufferAddress;
        mSceneManager.mPushConstants.materialIndex = renderItem.primitive->material->mMaterialIndex;
        mSceneManager.mPushConstants.worldMatrix = renderItem.transform;
        cmd.pushConstants<PushConstants>(*renderItem.primitive->material->mPipeline->layout, vk::ShaderStageFlagBits::eVertex, 0, mSceneManager.mPushConstants);

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
        mSceneManager.mRenderItems.clear();
        mSceneManager.generateRenderItems(); 
        mRegenRenderItems = false; 
    }
    for (auto& model : mSceneManager.mModels | std::views::values) {
        model.updateInstances();
    }
    mSceneManager.updateScene();
     
    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

Frame::Frame() :
    mCommandPool(nullptr),
    mCommandBuffer(nullptr),
    mSwapchainSemaphore(nullptr),
    mRenderSemaphore(nullptr),
    mRenderFence(nullptr)
{}

void Frame::cleanup()
{
    mRenderFence.clear();
    mRenderSemaphore.clear();
    mSwapchainSemaphore.clear();
    mCommandBuffer.clear();
    mCommandPool.clear();
}
