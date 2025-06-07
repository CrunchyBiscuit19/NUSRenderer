#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <fmt/core.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <ranges>

Renderer::Renderer() :
	mRendererCore(RendererCore(this)),
	mRendererInfrastructure(RendererInfrastructure(this)),
	mRendererResources(RendererResources(this)),
	mRendererScene(RendererScene(this)),
	mImmSubmit(ImmSubmit(this)),
	mGUI(Gui(this)),
	mCamera(Camera(this)),
	mRendererEvent(RendererEvent(this))
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
			for (auto& model : mRendererScene.mSceneManager.mModels | std::views::values) { model.markDelete(); }
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
	mRendererScene.cleanup();
	mImmSubmit.cleanup();
	mRendererResources.cleanup();
	mRendererInfrastructure.cleanup();
	mRendererCore.cleanup();
}

void Renderer::draw()
{
	auto start = std::chrono::system_clock::now();

	mRendererCore.mDevice.waitForFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);
	mRendererCore.mDevice.resetFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence);

	try {
		mRendererInfrastructure.mSwapchainIndex = mRendererInfrastructure.mSwapchainBundle.mSwapchain.acquireNextImage(1e9, *mRendererInfrastructure.getCurrentFrame().mAvailableSemaphore, nullptr).second;
	}
	catch (vk::OutOfDateKHRError e) {
		mRendererInfrastructure.mResizeRequested = true;
		return;
	}

	vk::CommandBuffer cmd = *mRendererInfrastructure.getCurrentFrame().mCommandBuffer;
	cmd.reset();

	vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(cmdBeginInfo);

	// Resizing bigger window, don't make swapchain extent go beyond draw image extent
	mRendererInfrastructure.mDrawImage.imageExtent.height = std::min(mRendererInfrastructure.mSwapchainBundle.mExtent.height, mRendererInfrastructure.mDrawImage.imageExtent.height);
	mRendererInfrastructure.mDrawImage.imageExtent.width = std::min(mRendererInfrastructure.mSwapchainBundle.mExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.width);

	drawClearScreen(cmd);
	drawGeometry(cmd);
	if (mRendererScene.mSkybox.mActive) { drawSkybox(cmd); }

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
	vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear);
	vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear);
	const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

	cmd.beginRendering(renderInfo);
	cmd.endRendering();
}

void Renderer::drawGeometry(vk::CommandBuffer cmd)
{
	mStats.mDrawCallCount = 0;

	vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
	vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
	const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

	cmd.beginRendering(renderInfo);

	vk::Pipeline lastPipeline = nullptr;
	PbrMaterial* lastMaterial = nullptr;
	vk::Buffer lastIndexBuffer = nullptr;

	for (auto& renderItem : mRendererScene.mSceneManager.mRenderItems) {
		if (*renderItem.primitive->mMaterial->mPipeline->pipeline != lastPipeline) {
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->mMaterial->mPipeline->pipeline);
			lastPipeline = *renderItem.primitive->mMaterial->mPipeline->pipeline;

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->mMaterial->mPipeline->layout, 0, *mRendererScene.mPerspective.mPerspectiveDescriptorSet, nullptr);

			vk::Viewport viewport = { 0, 0, static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.width), static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.height), 0.f, 1.f, };
			cmd.setViewport(0, viewport);
			vk::Rect2D scissor = { vk::Offset2D {0, 0}, vk::Extent2D {mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height}, };
			cmd.setScissor(0, scissor);
		}

		if (renderItem.primitive->mMaterial != lastMaterial) {
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderItem.primitive->mMaterial->mPipeline->layout, 1, *mRendererScene.mSceneManager.mMainMaterialResourcesDescriptorSet, nullptr);
			lastMaterial = renderItem.primitive->mMaterial;
		}

		if (*renderItem.mesh->mIndexBuffer.buffer != lastIndexBuffer) {
			cmd.bindIndexBuffer(*renderItem.mesh->mIndexBuffer.buffer, 0, vk::IndexType::eUint32);
			lastIndexBuffer = *renderItem.mesh->mIndexBuffer.buffer;
		}

		mRendererScene.mSceneManager.mScenePushConstants.vertexBuffer = renderItem.vertexBufferAddress;
		mRendererScene.mSceneManager.mScenePushConstants.instanceBuffer = renderItem.instancesBufferAddress;
		mRendererScene.mSceneManager.mScenePushConstants.materialBuffer = mRendererScene.mSceneManager.mMainMaterialConstantsBuffer.address;
		mRendererScene.mSceneManager.mScenePushConstants.materialIndex = renderItem.model->mMainFirstMaterial + renderItem.primitive->mMaterial->mRelativeMaterialIndex;
		mRendererScene.mSceneManager.mScenePushConstants.worldMatrix = renderItem.transform;
		cmd.pushConstants<ScenePushConstants>(*renderItem.primitive->mMaterial->mPipeline->layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, mRendererScene.mSceneManager.mScenePushConstants);

		cmd.drawIndexed(renderItem.primitive->mIndexCount, renderItem.model->mInstances.size(), renderItem.primitive->mRelativeFirstIndex, 0, 0);

		mStats.mDrawCallCount++;
	};

	cmd.endRendering();
}

void Renderer::drawSkybox(vk::CommandBuffer cmd)
{
	vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
	vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare);
	const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vk::Extent2D{ mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height }, &colorAttachment, &depthAttachment);

	cmd.beginRendering(renderInfo);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mRendererScene.mSkybox.mSkyboxPipeline.pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mRendererScene.mSkybox.mSkyboxPipeline.layout, 0, std::vector<vk::DescriptorSet> { *mRendererScene.mPerspective.mPerspectiveDescriptorSet, * mRendererScene.mSkybox.mSkyboxDescriptorSet }, nullptr);
	vk::Viewport viewport = { 0, 0, static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.width), static_cast<float>(mRendererInfrastructure.mDrawImage.imageExtent.height), 0.f, 1.f, };
	cmd.setViewport(0, viewport);
	vk::Rect2D scissor = { vk::Offset2D {0, 0}, vk::Extent2D {mRendererInfrastructure.mDrawImage.imageExtent.width, mRendererInfrastructure.mDrawImage.imageExtent.height}, };
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

	bool reloadMainInstancesBuffer = false;
	for (auto& model : mRendererScene.mSceneManager.mModels | std::views::values) {
		if (model.mReloadInstancesBuffer) {
			reloadMainInstancesBuffer = true;
			model.updateInstances(); 
			model.mReloadInstancesBuffer = false;
		}
	}
	if (!mModelAddedDeleted && reloadMainInstancesBuffer) { 
		mRendererScene.mSceneManager.reloadMainInstancesBuffer(); 
	}

	if (mModelAddedDeleted) {
		mRendererScene.mSceneManager.alignOffsets();
		mRendererScene.mSceneManager.regenerateRenderItems();
		mRendererScene.mSceneManager.reloadScene();
		mModelAddedDeleted = false;
	}

	const auto end = std::chrono::system_clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

Frame::Frame() :
	mCommandPool(nullptr),
	mCommandBuffer(nullptr),
	mRenderFence(nullptr),
	mAvailableSemaphore(nullptr)
{
}

void Frame::cleanup()
{
	mAvailableSemaphore.clear();
	mRenderFence.clear();
	mCommandBuffer.clear();
	mCommandPool.clear();
}
