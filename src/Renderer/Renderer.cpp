#include <Renderer/Renderer.h>

#include <fmt/core.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>
#include <quill/LogMacros.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.hpp>z

#include <ranges>

Renderer* Pass::renderer = nullptr;

Renderer::Renderer()
	: mCore(RendererCore(this))
	, mInfrastructure(RendererInfrastructure(this))
	, mResources(RendererResources(this))
	, mScene(RendererScene(this))
	, mImmSubmit(ImmSubmit(this))
	, mGui(Gui(this))
	, mCamera(Camera(this))
	, mEventHandler(RendererEvent(this))
{}

void Renderer::init()
{
	initLogger();
	initComponents();
	initPasses();
	initTransitions();

	LOG_INFO(mLogger, "Rendering Started");
}

void Renderer::initLogger()
{
	quill::Backend::start();

	if (LOG_TO_FILE)
	{
		auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
			fmt::format("{}Run.log", LOGS_PATH).c_str(), []()
			{
				quill::FileSinkConfig cfg;
				cfg.set_open_mode('w');
				cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
				return cfg;
			}(), quill::FileEventNotifier{});
		mLogger = quill::Frontend::create_or_get_logger("FileLogger", std::move(fileSink));
	}
	mLogger = quill::Frontend::create_or_get_logger("ConsoleLogger",
		quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink1"));
	mLogger->set_log_level(quill::LogLevel::TraceL3);
}

void Renderer::initComponents()
{
	mCore.init();
	mImmSubmit.init();
	mResources.init();
	mInfrastructure.init();
	mScene.init();
	mGui.init();
	mCamera.init();

	PbrMaterial::initMaterialPipelineLayout(this);
	mImmSubmit.queuedSubmit();
	mImmSubmit.mCallbacks.clear();

	mEventHandler.addEventCallback([this](SDL_Event& e) -> void
		{
			if (e.type == SDL_QUIT)
			{
				for (auto& model : mScene.mModelsCache | std::views::values)
				{
					model.markDelete();
				}
				mInfrastructure.mProgramEndFrameNumber = mInfrastructure.mFrameNumber + FRAME_OVERLAP + 1;
			}
			if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
					mStopRendering = true;
				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
					mStopRendering = false;
			}
			ImGui_ImplSDL2_ProcessEvent(&e);
		});
}

void Renderer::initPasses()
{
	mPasses.try_emplace(PassType::Cull, [&](vk::CommandBuffer cmd) {
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mCuller.mPipelineBundle.pipeline);

		for (auto& batch : mScene.mBatches | std::views::values)
		{
			if (batch.renderItems.empty()) { continue; }

			cmd.fillBuffer(*batch.countBuffer.buffer, 0, vk::WholeSize, 0);

			vkhelper::createBufferPipelineBarrier( // Wait for count buffers to be reset to zero
				cmd,
				*batch.countBuffer.buffer,
				vk::PipelineStageFlagBits2::eTransfer,
				vk::AccessFlagBits2::eTransferWrite,
				vk::PipelineStageFlagBits2::eComputeShader,
				vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

			mScene.mCuller.mPushConstants.renderItemsBuffer = batch.renderItemsBuffer.address;
			mScene.mCuller.mPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
			mScene.mCuller.mPushConstants.countBuffer = batch.countBuffer.address;
			cmd.pushConstants<CullPushConstants>(mScene.mCuller.mPipelineBundle.layout,
				vk::ShaderStageFlagBits::eCompute, 0,
				mScene.mCuller.mPushConstants);

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
	});

	mPasses.try_emplace(PassType::ClearScreen, [&](vk::CommandBuffer cmd) {
		vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
			*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AttachmentLoadOp::eClear);
		vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
			*mInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AttachmentLoadOp::eClear);
		const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
			vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

		cmd.beginRendering(renderInfo);
		cmd.endRendering();
	});

	mPasses.try_emplace(PassType::Pick, [&](vk::CommandBuffer cmd) {
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || mCamera.mRelativeMode || ImGui::GetIO().WantCaptureMouse) {
			return;
		}

		mPasses.at(PassType::PickClear).execute(cmd);

		mTransitions.at(TransitionType::PickerGeneralIntoColorAttachment).execute(
			cmd, *mScene.mPicker.mImage.image);

		mPasses.at(PassType::PickDraw).execute(cmd);

		mTransitions.at(TransitionType::PickerColorAttachmentIntoGeneral).execute(
			cmd, *mScene.mPicker.mImage.image);

		mPasses.at(PassType::PickPick).execute(cmd);
	});

	mPasses.try_emplace(PassType::PickClear, [&](vk::CommandBuffer cmd) {
		vk::ClearColorValue clearColor(UINT_MAX, UINT_MAX, static_cast<uint32_t>(0), static_cast<uint32_t>(0));
		vk::ImageSubresourceRange range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
		cmd.clearColorImage(*mScene.mPicker.mImage.image, vk::ImageLayout::eGeneral, clearColor,range);
	});

	mPasses.try_emplace(PassType::PickDraw, [&](vk::CommandBuffer cmd) {
		vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mScene.mPicker.mImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
		vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mScene.mPicker.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare);
		const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vkhelper::extent3dTo2d(mScene.mPicker.mImage.imageExtent), &colorAttachment, &depthAttachment);

		cmd.beginRendering(renderInfo);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mPicker.mDrawPipelineBundle.pipeline);
		vkhelper::setViewportScissors(cmd, mScene.mPicker.mImage.imageExtent);
		cmd.bindIndexBuffer(*mScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mScene.mPicker.mDrawPipelineBundle.layout, 0, *mScene.mPerspective.mDescriptorSet, nullptr);

		for (auto& batch : mScene.mBatches | std::views::values) {
			if (batch.renderItems.empty()) { continue; }

			mScene.mPicker.mDrawPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
			cmd.pushConstants<PickerDrawPushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mPicker.mDrawPushConstants);

			cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem));
		}

		cmd.endRendering();
	});

	mPasses.try_emplace(PassType::PickPick, [&](vk::CommandBuffer cmd) {
		int32_t mouseClickLocation[2] = { static_cast<int32_t>(ImGui::GetIO().MousePos.x), static_cast<int32_t>(ImGui::GetIO().MousePos.y) };
		std::memcpy(mScene.mPicker.mBuffer.info.pMappedData, &mouseClickLocation, 2 * sizeof(int32_t));

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mPicker.mPickPipelineBundle.pipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mScene.mPicker.mPickPipelineBundle.layout, 0, *mScene.mPicker.mDescriptorSet, nullptr);
		cmd.pushConstants<PickerPickPushConstants>(
			mScene.mPicker.mPickPipelineBundle.layout,
			vk::ShaderStageFlagBits::eCompute, 0,
			mScene.mPicker.mPickPushConstants);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mScene.mPicker.mBuffer.buffer,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostWrite,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

		cmd.dispatch(1, 1, 1);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mScene.mPicker.mBuffer.buffer,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderWrite,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostRead);

		glm::uvec2 read(0);
		std::memcpy(glm::value_ptr(read), static_cast<char*>(mScene.mPicker.mBuffer.info.pMappedData) + sizeof(glm::ivec2), 2 * sizeof(uint32_t));

		uint32_t modelId = read.x;

		auto reverseIt = mScene.mModelsReverse.find(static_cast<int>(modelId));
		if (reverseIt == mScene.mModelsReverse.end()) {
			mScene.mPicker.mClickedInstance = nullptr;
			return;
		}
		std::string& clickedModelName = reverseIt->second;

		auto cacheIt = mScene.mModelsCache.find(clickedModelName);
		if (cacheIt == mScene.mModelsCache.end()) {
			mScene.mPicker.mClickedInstance = nullptr;
			return;
		}
		GLTFModel& clickedModel = cacheIt->second;

		uint32_t localInstanceIndex = read.y - clickedModel.mMainFirstInstance;
		mScene.mPicker.mClickedInstance = &clickedModel.mInstances[localInstanceIndex];
	});

	mPasses.try_emplace(PassType::Geometry, [&](vk::CommandBuffer cmd)
		{
			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
			vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
				*mInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
				vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

			cmd.beginRendering(renderInfo);

			for (auto& batch : mScene.mBatches | std::views::values)
			{
				if (batch.renderItems.empty()) { continue; }

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipelineBundle->pipeline);

				vkhelper::setViewportScissors(cmd, mInfrastructure.mDrawImage.imageExtent);

				cmd.bindIndexBuffer(*mScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 0,
					*mScene.mPerspective.mDescriptorSet, nullptr);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 1,
					*mScene.mMainMaterialResourcesDescriptorSet, nullptr);

				mScene.mForwardPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
				cmd.pushConstants<ForwardPushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mForwardPushConstants);

				cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem));

				mStats.mDrawCallCount++;
			}

			cmd.endRendering();
		});

	mPasses.try_emplace(PassType::Skybox, [&](vk::CommandBuffer cmd)
		{
			if (!mScene.mSkybox.mActive) { return; }

			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
			vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
				*mInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal,
				vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
				vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

			cmd.beginRendering(renderInfo);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mSkybox.mPipelineBundle.pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mScene.mSkybox.mPipelineBundle.layout, 0,
				std::vector{
					 *mScene.mPerspective.mDescriptorSet,
					 *mScene.mSkybox.mDescriptorSet
				}, nullptr);
			vk::Viewport viewport = {
				0,
				0,
				static_cast<float>(mInfrastructure.mDrawImage.imageExtent.width),
				static_cast<float>(mInfrastructure.mDrawImage.imageExtent.height),
				0.f,
				1.f,
			};
			cmd.setViewport(0, viewport);
			vk::Rect2D scissor = {
				vk::Offset2D{0, 0},
				vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent),
			};
			cmd.setScissor(0, scissor);
			cmd.pushConstants<SkyBoxPushConstants>(mScene.mSkybox.mPipelineBundle.layout,
				vk::ShaderStageFlagBits::eVertex, 0,
				mScene.mSkybox.mPushConstants);

			cmd.draw(NUMBER_OF_SKYBOX_VERTICES, 1, 0, 0);
			mStats.mDrawCallCount++;

			cmd.endRendering();
		});

	mPasses.try_emplace(PassType::ResolveMSAA, [&](vk::CommandBuffer cmd)
		{
			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal,
				vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare,
				*mInfrastructure.mIntermediateImage.imageView);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mInfrastructure.mSwapchainBundle.mExtent,
				&colorAttachment, nullptr);

			cmd.beginRendering(renderInfo);
			cmd.endRendering();
		});

	mPasses.try_emplace(PassType::IntermediateToSwapchain, [&](vk::CommandBuffer cmd)
		{
			vkhelper::copyImage(
				cmd,
				*mInfrastructure.mIntermediateImage.image, mInfrastructure.getCurrentSwapchainImage().image,
				vkhelper::extent3dTo2d(mInfrastructure.mIntermediateImage.imageExtent),
				mInfrastructure.mSwapchainBundle.mExtent
			);
		});

	mPasses.try_emplace(PassType::ImGui, [&](vk::CommandBuffer cmd)
		{
			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mInfrastructure.getCurrentSwapchainImage().imageView, vk::ImageLayout::eColorAttachmentOptimal,
				vk::AttachmentLoadOp::eDontCare);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mInfrastructure.mSwapchainBundle.mExtent,
				&colorAttachment, nullptr);

			cmd.beginRendering(renderInfo);
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
			cmd.endRendering();
		});
}

void Renderer::initTransitions()
{
	mTransitions.try_emplace(TransitionType::PickerGeneralIntoColorAttachment,
		vk::PipelineStageFlagBits2::eComputeShader,
		vk::AccessFlagBits2::eShaderRead,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderWrite,
		vk::ImageLayout::eGeneral,
		vk::ImageLayout::eColorAttachmentOptimal
	);

	mTransitions.try_emplace(TransitionType::PickerColorAttachmentIntoGeneral,
		vk::PipelineStageFlagBits2::eFragmentShader,
		vk::AccessFlagBits2::eShaderWrite,
		vk::PipelineStageFlagBits2::eComputeShader,
		vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eGeneral
	);

	mTransitions.try_emplace(TransitionType::IntermediateTransferSrcIntoColorAttachment,
		vk::PipelineStageFlagBits2::eTransfer,
		vk::AccessFlagBits2::eTransferRead,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::ImageLayout::eColorAttachmentOptimal
	);

	mTransitions.try_emplace(TransitionType::IntermediateColorAttachmentIntoTransferSrc,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eTransfer,
		vk::AccessFlagBits2::eTransferRead,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eTransferSrcOptimal
	);

	mTransitions.try_emplace(TransitionType::SwapchainPresentIntoTransferDst,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eNone,
		vk::PipelineStageFlagBits2::eTransfer,
		vk::AccessFlagBits2::eTransferWrite,
		vk::ImageLayout::ePresentSrcKHR,
		vk::ImageLayout::eTransferDstOptimal
	);

	mTransitions.try_emplace(TransitionType::SwapchainTransferDstIntoColorAttachment,
		vk::PipelineStageFlagBits2::eTransfer,
		vk::AccessFlagBits2::eTransferWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eColorAttachmentOptimal
	);

	mTransitions.try_emplace(TransitionType::SwapchainColorAttachmentIntoPresent,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eNone,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR
	);
}

void Renderer::run()
{
	SDL_Event e;

	while (true)
	{
		auto start = std::chrono::system_clock::now();

		mStats.reset();

		if (mInfrastructure.mProgramEndFrameNumber.has_value() && (mInfrastructure.mFrameNumber <
			mInfrastructure.mProgramEndFrameNumber.value()))
		{
			mCore.mDevice.waitIdle();
			break;
		}

		while (SDL_PollEvent(&e) != 0)
		{
			mEventHandler.executeEventCallbacks(e);
		}

		if (mStopRendering)
		{
			// Do not draw if minimized
			std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Throttle the speed to avoid endless spinning
			continue;
		}

		SDL_SetRelativeMouseMode(mCamera.mRelativeMode);
		if (mInfrastructure.mResizeRequested)
		{
			mInfrastructure.resizeSwapchain();
		}

		mGui.updateFrame();
		perFrameUpdate();

		draw();

		mInfrastructure.mFrameNumber++;

		auto end = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		mStats.mFrameTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
	}
}

void Renderer::perFrameUpdate()
{
	const auto start = std::chrono::system_clock::now();

	mScene.mPerspective.update();

	mScene.deleteModels();
	mScene.deleteInstances();

	for (auto& model : mScene.mModelsCache | std::views::values) {
		if (model.mReloadInstances) {
			model.reloadInstances();
			mScene.mFlags.reloadMainInstancesBuffer = true;
		}
	}

	if (mScene.mFlags.modelAddedFlag || mScene.mFlags.modelDestroyedFlag)
	{
		mScene.realignOffsets();
		mScene.reloadMainBuffers();
		mScene.regenerateRenderItems();
	}
	else if (mScene.mFlags.instanceAddedFlag || mScene.mFlags.instanceDestroyedFlag)
	{
		mScene.realignInstancesOffset();
		mScene.reloadMainInstancesBuffer();
		mScene.regenerateRenderItems();
	}
	else if (mScene.mFlags.reloadMainInstancesBuffer)
	{
		mScene.reloadMainInstancesBuffer();
	}

	mScene.resetFlags();

	mImmSubmit.queuedSubmit();
	mImmSubmit.mCallbacks.clear();

	const auto end = std::chrono::system_clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::draw()
{
	auto start = std::chrono::system_clock::now();

	mCore.mDevice.waitForFences(*mInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);
	mCore.mDevice.resetFences(*mInfrastructure.getCurrentFrame().mRenderFence);
	try
	{
		mInfrastructure.mSwapchainIndex = mInfrastructure.mSwapchainBundle.mSwapchain.acquireNextImage(
			1e9, *mInfrastructure.getCurrentFrame().mAvailableSemaphore, nullptr).second;
	}
	catch (vk::OutOfDateKHRError e)
	{
		mInfrastructure.mResizeRequested = true;
		return;
	}

	vk::CommandBuffer cmd = *mInfrastructure.getCurrentFrame().mCommandBuffer;
	cmd.reset();
	vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(cmdBeginInfo);

	mPasses.at(PassType::Cull).execute(cmd);

	mPasses.at(PassType::ClearScreen).execute(cmd);

	mPasses.at(PassType::Pick).execute(cmd);

	mPasses.at(PassType::Geometry).execute(cmd);
	mPasses.at(PassType::Skybox).execute(cmd);

	mTransitions.at(TransitionType::IntermediateTransferSrcIntoColorAttachment).execute(
		cmd, *mInfrastructure.mIntermediateImage.image);

	mPasses.at(PassType::ResolveMSAA).execute(cmd);

	mTransitions.at(TransitionType::IntermediateColorAttachmentIntoTransferSrc).execute(
		cmd, *mInfrastructure.mIntermediateImage.image);
	mTransitions.at(TransitionType::SwapchainPresentIntoTransferDst).execute(
		cmd, mInfrastructure.getCurrentSwapchainImage().image);

	mPasses.at(PassType::IntermediateToSwapchain).execute(cmd);

	mTransitions.at(TransitionType::SwapchainTransferDstIntoColorAttachment).execute(
		cmd, mInfrastructure.getCurrentSwapchainImage().image);

	mPasses.at(PassType::ImGui).execute(cmd);

	mTransitions.at(TransitionType::SwapchainColorAttachmentIntoPresent).execute(
		cmd, mInfrastructure.getCurrentSwapchainImage().image);

	cmd.end();

	vk::CommandBufferSubmitInfo cmdinfo = vkhelper::commandBufferSubmitInfo(cmd);
	vk::SemaphoreSubmitInfo waitInfo = vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		*mInfrastructure.getCurrentFrame().
		mAvailableSemaphore);
	vk::SemaphoreSubmitInfo signalInfo = vkhelper::semaphoreSubmitInfo(
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		*mInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
	const vk::SubmitInfo2 submit = vkhelper::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

	mCore.mGraphicsQueue.submit2(submit, *mInfrastructure.getCurrentFrame().mRenderFence);

	// Prepare present. Wait on the mRenderSemaphore for queue commands to finish before image is presented.
	vk::PresentInfoKHR presentInfo = {};
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &(*mInfrastructure.mSwapchainBundle.mSwapchain);
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &(*mInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &mInfrastructure.mSwapchainIndex;

	try
	{
		mCore.mGraphicsQueue.presentKHR(presentInfo);
	}
	catch (vk::OutOfDateKHRError e)
	{
		mInfrastructure.mResizeRequested = true;
	}

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::cleanup()
{
	PbrMaterial::cleanup(this);

	mGui.cleanup();
	mScene.cleanup();
	mImmSubmit.cleanup();
	mResources.cleanup();
	mInfrastructure.cleanup();
	mCore.cleanup();

	LOG_INFO(mLogger, "Rendering Ended");
}
