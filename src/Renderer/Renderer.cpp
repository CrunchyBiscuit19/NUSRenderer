#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <fmt/core.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>
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
    quill::Backend::start();

    if (LOG_TO_FILE) {
        auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(fmt::format("{}Run.log", LOGS_PATH).c_str(), []() {
                quill::FileSinkConfig cfg;
                cfg.set_open_mode('w');
                cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
                return cfg;
        }(), quill::FileEventNotifier{});
        mLogger = quill::Frontend::create_or_get_logger("FileLogger", std::move(fileSink));
    }
    else {
        mLogger = quill::Frontend::create_or_get_logger("ConsoleLogger", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink1"));
    }
    mLogger->set_log_level(quill::LogLevel::TraceL3);

    LOG_INFO(mLogger, "Rendering Started");

    mRendererCore.init();
    mImmSubmit.init();
    mRendererResources.init();
    mRendererInfrastructure.init();
    mRendererScene.init();
    mGUI.init();
    mCamera.init();
    PbrMaterial::initMaterialPipelineLayout(this);

    mImmSubmit.queuedSubmit();
    mImmSubmit.mCallbacks.clear();

    mRendererEvent.addEventCallback([this](SDL_Event& e) -> void {
        if (e.type == SDL_QUIT) {
            for (auto& model : mRendererScene.mMainScene.mModels | std::views::values) {
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
    SDL_Event e;

    while (true) {
        auto start = std::chrono::system_clock::now();

        mStats.reset();

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

        perFrameUpdate();
        draw();

        mRendererInfrastructure.mFrameNumber++;

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.mFrameTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
    }
}

void Renderer::cleanup()
{
    PbrMaterial::cleanup(this);

    mGUI.cleanup();
    mRendererScene.cleanup();
    mImmSubmit.cleanup();
    mRendererResources.cleanup();
    mRendererInfrastructure.cleanup();
    mRendererCore.cleanup();

    LOG_INFO(mLogger, "Rendering Ended");
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

    clearScreen(cmd);
    //drawPick(cmd);
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
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mRendererScene.mCuller.mPipelineBundle.pipeline);

    for (auto& batch : mRendererScene.mMainScene.mBatches | std::views::values) {    
        if (batch.renderItems.size() == 0) { continue; }

        cmd.fillBuffer(*batch.countBuffer.buffer, 0, vk::WholeSize, 0);

        vkhelper::createBufferPipelineBarrier( // Wait for count buffers to be reset to zero
            cmd,
            *batch.countBuffer.buffer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eComputeShader, 
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

        mRendererScene.mCuller.mPushConstants.renderItemsBuffer = batch.renderItemsBuffer.address;
        mRendererScene.mCuller.mPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
        mRendererScene.mCuller.mPushConstants.countBuffer = batch.countBuffer.address;
        cmd.pushConstants<CullPushConstants>(mRendererScene.mCuller.mPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mRendererScene.mCuller.mPushConstants);

        cmd.dispatch(std::ceil(batch.renderItems.size() / static_cast<float>(MAX_CULL_LOCAL_SIZE)), 1, 1);

        vkhelper::createBufferPipelineBarrier( // Wait for culling to write finish all visible render items
            cmd,
            *batch.visibleRenderItemsBuffer.buffer,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eVertexShader, 
            vk::AccessFlagBits2::eShaderRead);

        vkhelper::createBufferPipelineBarrier( // Wait for count buffers to be written to
            cmd,
            *batch.countBuffer.buffer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eDrawIndirect,
            vk::AccessFlagBits2::eIndirectCommandRead);
    }
}

void Renderer::clearScreen(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D { mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);
    cmd.endRendering();
}

/*void Renderer::drawPick(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererScene.mPicker.mObjectImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererScene.mPicker.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererScene.mPicker.mObjectImage.imageExtent.width, mRendererScene.mPicker.mObjectImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mRendererScene.mPicker.mPipelineBundle.pipeline);
    setViewportScissors(cmd);
    cmd.bindIndexBuffer(*mRendererScene.mMainScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mRendererScene.mPicker.mPipelineLayout, 0, *mRendererScene.mPerspective.mDescriptorSet, nullptr);

    for (auto& batch : mRendererScene.mMainScene.mBatches | std::views::values) {
        if (batch.renderItems.size() == 0) { continue; }

        mRendererScene.mMainScene.mScenePushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
        cmd.pushConstants<ScenePushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mRendererScene.mMainScene.mScenePushConstants);

        cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem));
    }

    cmd.endRendering();
}*/

void Renderer::drawGeometry(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D { mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    for (auto& batch : mRendererScene.mMainScene.mBatches | std::views::values) {
        if (batch.renderItems.size() == 0) { continue; }

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipelineBundle->pipeline);
        
        setViewportScissors(cmd);
        
        cmd.bindIndexBuffer(*mRendererScene.mMainScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 0, *mRendererScene.mPerspective.mDescriptorSet, nullptr);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 1, *mRendererScene.mMainScene.mMainMaterialResourcesDescriptorSet, nullptr);

        mRendererScene.mMainScene.mScenePushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
        cmd.pushConstants<ScenePushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mRendererScene.mMainScene.mScenePushConstants);

        cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem));

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

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mRendererScene.mSkybox.mPipelineBundle.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mRendererScene.mSkybox.mPipelineBundle.layout, 0, 
        std::vector<vk::DescriptorSet> { *mRendererScene.mPerspective.mDescriptorSet, *mRendererScene.mSkybox.mDescriptorSet }, nullptr);
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
    cmd.pushConstants<SkyBoxPushConstants>(mRendererScene.mSkybox.mPipelineBundle.layout, vk::ShaderStageFlagBits::eVertex, 0, mRendererScene.mSkybox.mPushConstants);

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

void Renderer::perFrameUpdate()
{
    const auto start = std::chrono::system_clock::now();

    mRendererScene.mPerspective.update();

    mRendererScene.mMainScene.deleteModels();
    mRendererScene.mMainScene.deleteInstances();

    for (auto& model : mRendererScene.mMainScene.mModels | std::views::values) {
        if (model.mReloadLocalInstancesBuffer) {
            model.updateInstances();
            model.mReloadLocalInstancesBuffer = false;
        }
    }

    if (mRendererScene.mMainScene.mFlags.modelAddedFlag || mRendererScene.mMainScene.mFlags.modelDestroyedFlag) {
        mRendererScene.mMainScene.realignOffsets();
        mRendererScene.mMainScene.reloadMainBuffers();
        mRendererScene.mMainScene.regenerateRenderItems();
    } else if (mRendererScene.mMainScene.mFlags.instanceAddedFlag || mRendererScene.mMainScene.mFlags.instanceDestroyedFlag) {
        mRendererScene.mMainScene.realignInstancesOffset();
        mRendererScene.mMainScene.reloadMainInstancesBuffer();
        mRendererScene.mMainScene.regenerateRenderItems();
    } else if (mRendererScene.mMainScene.mFlags.reloadMainInstancesBuffer) {
        mRendererScene.mMainScene.reloadMainInstancesBuffer();
    }

    mRendererScene.mMainScene.resetFlags();

    mImmSubmit.queuedSubmit();
    mImmSubmit.mCallbacks.clear();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}