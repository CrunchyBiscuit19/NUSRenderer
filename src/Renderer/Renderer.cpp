#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <fmt/core.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <ranges>

Renderer::Renderer()
    : mRendererCore(RendererCore(this))
    , mRendererInfrastructure(RendererInfrastructure(this))
    , mRendererResources(RendererResources(this))
    , mRendererScene(RendererScene(this))
    , mImmSubmit(ImmSubmit(this))
    , mGUI(Gui(this))
    , mCamera(Camera(this))
    , mRendererEvent(RendererEvent(this))
{
}

void Renderer::init()
{
    mRendererCore.init();
    mImmSubmit.init();
    mRendererResources.init();
    mRendererInfrastructure.init();
    mRendererScene.init();
    mGUI.init();
    mCamera.init();

    mRendererEvent.addEventCallback([this](SDL_Event& e) -> void {
        if (e.type == SDL_QUIT) {
            for (auto& model : mRendererScene.mSceneManager.mModels | std::views::values) {
                model.markDelete();
            }
            mRendererInfrastructure.mProgramEndFrameNumber = mRendererInfrastructure.mFrameNumber + FRAME_OVERLAP + 1;
        }
        if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                mStopRendering = true;
            if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                mStopRendering = false;
        }
        ImGui_ImplSDL2_ProcessEvent(&e);
    });
}

void Renderer::run()
{
    fmt::println("Rendering started");

    SDL_Event e;

    while (true) {
        auto start = std::chrono::system_clock::now();

        if (mRendererInfrastructure.mProgramEndFrameNumber.has_value() && (mRendererInfrastructure.mFrameNumber < mRendererInfrastructure.mProgramEndFrameNumber.value())) {
            mRendererCore.mDevice.waitIdle();
            break;
        }

        while (SDL_PollEvent(&e) != 0) {
            mRendererEvent.executeEventCallbacks(e);
        }

        if (mStopRendering) { // Do not draw if minimized
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Throttle the speed to avoid endless spinning
            continue;
        }

        SDL_SetRelativeMouseMode(mCamera.mRelativeMode);
        if (mRendererInfrastructure.mResizeRequested) {
            mRendererInfrastructure.resizeSwapchain();
        }

        mGUI.imguiFrame();

        drawUpdate();
        draw();

        mRendererInfrastructure.mFrameNumber++;

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.mFrametime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
    }

    fmt::println("Rendering ended");
}

void Renderer::cleanup()
{
    mGUI.cleanup();
    mRendererScene.cleanup();
    mImmSubmit.cleanup();
    mRendererResources.cleanup();
    mRendererInfrastructure.cleanup();
    mRendererCore.cleanup();
}

void Renderer::setViewportScissors(vk::CommandBuffer cmd)
{
    vk::Extent2D drawImage2dExtent = { mRendererInfrastructure.mDrawImage.imageExtent.width,
        mRendererInfrastructure.mDrawImage.imageExtent.height };

    vk::Viewport viewport = {
        0,
        0,
        static_cast<float>(drawImage2dExtent.width),
        static_cast<float>(drawImage2dExtent.height),
        0.f,
        1.f,
    };
    cmd.setViewport(0, viewport);
    vk::Rect2D scissor = {
        vk::Offset2D { 0, 0 },
        drawImage2dExtent,
    };
    cmd.setScissor(0, scissor);
}

void Renderer::draw()
{
    auto start = std::chrono::system_clock::now();

    mRendererCore.mDevice.waitForFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);
    mRendererCore.mDevice.resetFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence);

    try {
        mRendererInfrastructure.mSwapchainIndex = mRendererInfrastructure.mSwapchainBundle.mSwapchain.acquireNextImage(1e9, *mRendererInfrastructure.getCurrentFrame().mAvailableSemaphore, nullptr).second;
    } catch (vk::OutOfDateKHRError e) {
        mRendererInfrastructure.mResizeRequested = true;
        return;
    }

    vk::CommandBuffer cmd = *mRendererInfrastructure.getCurrentFrame().mCommandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmdBeginInfo);

    cullRenderItems(cmd);

    // Resizing bigger window, don't make swapchain extent go beyond draw image extent
    mRendererInfrastructure.mDrawImage.imageExtent.height = std::min(mRendererInfrastructure.mSwapchainBundle.mExtent.height, mRendererInfrastructure.mDrawImage.imageExtent.height);
    mRendererInfrastructure.mDrawImage.imageExtent.width = std::min(mRendererInfrastructure.mSwapchainBundle.mExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.width);

    drawClearScreen(cmd);
    drawGeometry(cmd);
    if (mRendererScene.mSkybox.mActive) {
        drawSkybox(cmd);
    }

    // Transition intermediate image into resolvable color compatible layout
    vkhelper::transitionImage(cmd, *mRendererInfrastructure.mIntermediateImage.image,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eColorAttachmentOptimal);

    resolveMsaa(cmd); // Resolve multisampled draw image into singly-sampled intermediate image

    // Transition intermediate image and swapchain image to transfer layouts to copy over
    vkhelper::transitionImage(cmd, *mRendererInfrastructure.mIntermediateImage.image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
    vkhelper::transitionImage(cmd, mRendererInfrastructure.getCurrentSwapchainImage().image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferDstOptimal);

    vkhelper::copyImage(cmd, *mRendererInfrastructure.mIntermediateImage.image, mRendererInfrastructure.getCurrentSwapchainImage().image,
        vk::Extent2D {
            mRendererInfrastructure.mIntermediateImage.imageExtent.width,
            mRendererInfrastructure.mIntermediateImage.imageExtent.height,
        },
        mRendererInfrastructure.mSwapchainBundle.mExtent);

    // Transition swapchain image to color optimal to draw GUI into final swapchain image
    vkhelper::transitionImage(cmd, mRendererInfrastructure.getCurrentSwapchainImage().image,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

    drawGui(cmd, *mRendererInfrastructure.getCurrentSwapchainImage().imageView);

    // Set swapchain image layout to presentable layout
    vkhelper::transitionImage(cmd, mRendererInfrastructure.getCurrentSwapchainImage().image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

    cmd.end();

    // Prepare the submission to the queue. (Reading semaphore states)
    vk::CommandBufferSubmitInfo cmdinfo = vkhelper::commandBufferSubmitInfo(cmd);
    vk::SemaphoreSubmitInfo waitInfo = vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput, *mRendererInfrastructure.getCurrentFrame().mAvailableSemaphore);
    vk::SemaphoreSubmitInfo signalInfo = vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput, *mRendererInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
    const vk::SubmitInfo2 submit = vkhelper::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

    mRendererCore.mGraphicsQueue.submit2(submit, *mRendererInfrastructure.getCurrentFrame().mRenderFence);

    // Prepare present.
    // Wait on the mRenderSemaphore for queue commands to finish before image is presented.
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &(*mRendererInfrastructure.mSwapchainBundle.mSwapchain);
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &(*mRendererInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &mRendererInfrastructure.mSwapchainIndex;

    try {
        mRendererCore.mGraphicsQueue.presentKHR(presentInfo);
    } catch (vk::OutOfDateKHRError e) {
        mRendererInfrastructure.mResizeRequested = true;
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::cullRenderItems(vk::CommandBuffer cmd)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mRendererInfrastructure.mCullPipeline.pipeline);

    for (auto& batch : mRendererScene.mSceneManager.mBatches | std::views::values) {    


        vkhelper::createBufferPipelineBarrier( // Wait for count buffers to be reset to zero
            cmd,
            *batch.countBuffer.buffer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);

        mRendererScene.mSceneManager.mCullPushConstants.renderItemsBuffer = batch.renderItemsBuffer.address;
        mRendererScene.mSceneManager.mCullPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
        mRendererScene.mSceneManager.mCullPushConstants.countBuffer = batch.countBuffer.address;
        cmd.pushConstants<CullPushConstants>(*mRendererInfrastructure.mCullPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, mRendererScene.mSceneManager.mCullPushConstants);

        cmd.dispatch(std::ceil(batch.renderItems.size() / MAX_CULL_LOCAL_SIZE), 1, 1);

        vkhelper::createBufferPipelineBarrier( // Wait for culling to write finish all visible render items
            cmd,
            *batch.visibleRenderItemsBuffer.buffer,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead);
    }
}

void Renderer::drawClearScreen(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D { mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);
    cmd.endRendering();
}

void Renderer::drawGeometry(vk::CommandBuffer cmd)
{
    mStats.mDrawCallCount = 0;

    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D { mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    for (auto& batch : mRendererScene.mSceneManager.mBatches | std::views::values) {
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipeline->pipeline);
        
        setViewportScissors(cmd);
        
        cmd.bindIndexBuffer(*mRendererScene.mSceneManager.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *batch.pipeline->layout, 0, *mRendererScene.mPerspective.mPerspectiveDescriptorSet, nullptr);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *batch.pipeline->layout, 1, *mRendererScene.mSceneManager.mMainMaterialResourcesDescriptorSet, nullptr);

        mRendererScene.mSceneManager.mScenePushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
        cmd.pushConstants<ScenePushConstants>(*batch.pipeline->layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, mRendererScene.mSceneManager.mScenePushConstants);

        cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_INDIRECT_COMMANDS, sizeof(RenderItem));

        mStats.mDrawCallCount++;
    }

    cmd.endRendering();
}

void Renderer::drawSkybox(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D { mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mRendererScene.mSkybox.mSkyboxPipeline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mRendererScene.mSkybox.mSkyboxPipeline.layout, 0, std::vector<vk::DescriptorSet> { *mRendererScene.mPerspective.mPerspectiveDescriptorSet, *mRendererScene.mSkybox.mSkyboxDescriptorSet }, nullptr);
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
        vk::Offset2D { 0, 0 },
        vk::Extent2D { mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height },
    };
    cmd.setScissor(0, scissor);
    cmd.pushConstants<SkyBoxPushConstants>(*mRendererScene.mSkybox.mSkyboxPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, mRendererScene.mSkybox.mSkyboxPushConstants);

    cmd.draw(NUMBER_OF_SKYBOX_VERTICES, 1, 0, 0);
    mStats.mDrawCallCount++;

    cmd.endRendering();
}

void Renderer::resolveMsaa(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare, *mRendererInfrastructure.mIntermediateImage.imageView);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mRendererInfrastructure.mSwapchainBundle.mExtent, &colorAttachment, nullptr);

    cmd.beginRendering(renderInfo);
    cmd.endRendering();
}

void Renderer::drawGui(vk::CommandBuffer cmd, vk::ImageView swapchainImageView)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(swapchainImageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eDontCare);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mRendererInfrastructure.mSwapchainBundle.mExtent, &colorAttachment, nullptr);

    cmd.beginRendering(renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
}

void Renderer::drawUpdate()
{
    const auto start = std::chrono::system_clock::now();

    mRendererScene.mPerspective.update();

    mRendererScene.mSceneManager.deleteModels();
    mRendererScene.mSceneManager.deleteInstances();

    for (auto& model : mRendererScene.mSceneManager.mModels | std::views::values) {
        if (model.mUpdateInstancesFlag) {
            model.updateInstances();
            model.mUpdateInstancesFlag = false;
            mRendererScene.mSceneManager.mFlags.mInstanceUpdatedFlag = true;
        }
    }

    if (mRendererScene.mSceneManager.mFlags.mModelAddedFlag || mRendererScene.mSceneManager.mFlags.mModelDestroyedFlag) {
        mRendererScene.mSceneManager.reloadMainBuffers();
        mRendererScene.mSceneManager.realignOffsets();
        mRendererScene.mSceneManager.regenerateRenderItems();
    } else if (mRendererScene.mSceneManager.mFlags.mInstanceAddedFlag || mRendererScene.mSceneManager.mFlags.mInstanceDestroyedFlag) {
        mRendererScene.mSceneManager.reloadMainInstancesBuffer();
        mRendererScene.mSceneManager.realignInstancesOffset();
    } else if (mRendererScene.mSceneManager.mFlags.mInstanceUpdatedFlag) {
        mRendererScene.mSceneManager.reloadMainInstancesBuffer();
    }

    mRendererScene.mSceneManager.resetFlags();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
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
