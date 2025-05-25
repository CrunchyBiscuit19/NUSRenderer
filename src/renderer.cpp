#include <Renderer.h>
#include <Helper.h>

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
    mResourceManager.init();
    mRendererInfrastructure.init();
    mSceneManager.init();
    mGUI.init();
    mCamera.init();
}

void Renderer::run()
{
    fmt::println("Rendering started");

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
    mSceneManager.cleanup();
    mImmSubmit.cleanup();
    mResourceManager.cleanup();
    mRendererInfrastructure.cleanup();
    mRendererCore.cleanup();
}

void Renderer::draw()
{
    auto start = std::chrono::system_clock::now();

    mRendererCore.mDevice.waitForFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);

    try {
        auto output = mRendererInfrastructure.mSwapchainBundle.mSwapchain.acquireNextImage(1e9, *mRendererInfrastructure.getCurrentFrame().mAvailableSemaphore, nullptr);
        mRendererInfrastructure.mSwapchainIndex = output.second;
    }
    catch (vk::OutOfDateKHRError e) {
        mRendererInfrastructure.mResizeRequested = true;
        return;
    }

    mRendererCore.mDevice.resetFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence);
    vk::CommandBuffer cmd = *mRendererInfrastructure.getCurrentFrame().mCommandBuffer;
    cmd.reset();

    vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmdBeginInfo);

    // Resizing bigger window, don't make swapchain extent go beyond draw image extent
    mRendererInfrastructure.mDrawImage.imageExtent.height = std::min(mRendererInfrastructure.mSwapchainBundle.mExtent.height, mRendererInfrastructure.mDrawImage.imageExtent.height);
    mRendererInfrastructure.mDrawImage.imageExtent.width = std::min(mRendererInfrastructure.mSwapchainBundle.mExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.width);

    drawClearScreen(cmd);
    drawGeometry(cmd);
    if (mSceneManager.mSkyboxActive) { drawSkybox(cmd); }

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
        vk::Extent2D{ mRendererInfrastructure.mIntermediateImage.imageExtent.width, mRendererInfrastructure.mIntermediateImage.imageExtent.height, },
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
    }
    catch (vk::OutOfDateKHRError e) {
        mRendererInfrastructure.mResizeRequested = true;
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::drawClearScreen(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, mResourceManager.mDefaultColorClearValue, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ClearDepthStencilValue { 0, 0 }, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);
    cmd.endRendering();
}

void Renderer::drawGeometry(vk::CommandBuffer cmd)
{
    mStats.mDrawCallCount = 0;

    // [TODO] Sorting and culling
    
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, std::nullopt, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, std::nullopt, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    vk::Pipeline lastPipeline = nullptr;
    std::shared_ptr<PbrMaterial> lastMaterial = nullptr;
    vk::Buffer lastIndexBuffer = nullptr;

    for (auto& renderItem : mSceneManager.mRenderItems) {
        if (*renderItem.primitive->material->mPipeline->pipeline != lastPipeline) {
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->pipeline);
            lastPipeline = *renderItem.primitive->material->mPipeline->pipeline;

            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->material->mPipeline->layout, 0, *mSceneManager.mSceneResources.mSceneDescriptorSet, nullptr);

            vk::Viewport viewport = { 0, 0, static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.width), static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.height), 0.f, 1.f, };
            cmd.setViewport(0, viewport);
            vk::Rect2D scissor = { vk::Offset2D {0, 0}, vk::Extent2D {mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height}, };
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

void Renderer::drawSkybox(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, std::nullopt, vk::ImageLayout::eColorAttachmentOptimal);
    vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, std::nullopt, vk::ImageLayout::eDepthAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

    cmd.beginRendering(renderInfo);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mSceneManager.mSkybox.mSkyboxPipeline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mSceneManager.mSkybox.mSkyboxPipeline.layout, 0, std::vector<vk::DescriptorSet> { *mSceneManager.mSceneResources.mSceneDescriptorSet, * mSceneManager.mSkybox.mSkyboxDescriptorSet }, nullptr);
    vk::Viewport viewport = { 0, 0, static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.width), static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.height), 0.f, 1.f, };
    cmd.setViewport(0, viewport);
    vk::Rect2D scissor = { vk::Offset2D {0, 0}, vk::Extent2D {mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height}, };
    cmd.setScissor(0, scissor);
    cmd.pushConstants<SkyBoxPushConstants>(*mSceneManager.mSkybox.mSkyboxPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, mSceneManager.mSkybox.mSkyboxPushConstants);

    cmd.draw(NUMBER_OF_SKYBOX_VERTICES, 1, 0, 0);
    mStats.mDrawCallCount++;

    cmd.endRendering();
}

void Renderer::drawGui(vk::CommandBuffer cmd, vk::ImageView targetImageView)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(targetImageView, std::nullopt, vk::ImageLayout::eColorAttachmentOptimal);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mRendererInfrastructure.mSwapchainBundle.mExtent, &colorAttachment, nullptr);

    cmd.beginRendering(renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
}

void Renderer::resolveMsaa(vk::CommandBuffer cmd)
{
    vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, std::nullopt, vk::ImageLayout::eColorAttachmentOptimal, *mRendererInfrastructure.mIntermediateImage.imageView);
    const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mRendererInfrastructure.mSwapchainBundle.mExtent, &colorAttachment, nullptr);

    cmd.beginRendering(renderInfo);
    cmd.endRendering();
}

void Renderer::drawUpdate()
{
    const auto start = std::chrono::system_clock::now();

    mSceneManager.deleteInstances();
    mSceneManager.deleteModels();

    if (mRegenRenderItems) { 
        mSceneManager.mRenderItems.clear();
        mSceneManager.generateRenderItems(); 
        mRegenRenderItems = false; 
    }
    for (auto& model : mSceneManager.mModels | std::views::values) {
        if (model.mReloadInstancesBuffer) { model.updateInstances(); }
        model.mReloadInstancesBuffer = false;
    }
    mSceneManager.updateScene();

    mRegenRenderItems = false;
     
    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

Frame::Frame() :
    mCommandPool(nullptr),
    mCommandBuffer(nullptr),
    mRenderFence(nullptr),
    mAvailableSemaphore(nullptr)
{}

void Frame::cleanup()
{
    mAvailableSemaphore.clear();
    mRenderFence.clear();
    mCommandBuffer.clear();
    mCommandPool.clear();
}
