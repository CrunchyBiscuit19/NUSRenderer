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

	mRendererEvent.addEventCallback([this](SDL_Event& e) -> void
		{
			if (e.type == SDL_QUIT)
			{
				for (auto& model : mRendererScene.mModelsCache | std::views::values)
				{
					model.markDelete();
				}
				mRendererInfrastructure.mProgramEndFrameNumber = mRendererInfrastructure.mFrameNumber + FRAME_OVERLAP + 1;
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
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mRendererScene.mCuller.mPipelineBundle.pipeline);

		for (auto& batch : mRendererScene.mBatches | std::views::values)
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

			mRendererScene.mCuller.mPushConstants.renderItemsBuffer = batch.renderItemsBuffer.address;
			mRendererScene.mCuller.mPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
			mRendererScene.mCuller.mPushConstants.countBuffer = batch.countBuffer.address;
			cmd.pushConstants<CullPushConstants>(mRendererScene.mCuller.mPipelineBundle.layout,
				vk::ShaderStageFlagBits::eCompute, 0,
				mRendererScene.mCuller.mPushConstants);

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
			*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal,
			vk::AttachmentLoadOp::eClear);
		vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
			*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AttachmentLoadOp::eClear);
		const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
			vkhelper::extent3dTo2d(mRendererInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

		cmd.beginRendering(renderInfo);
		cmd.endRendering();
	});

	mPasses.try_emplace(PassType::Pick, [&](vk::CommandBuffer cmd) {
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || mCamera.mRelativeMode || ImGui::GetIO().WantCaptureMouse) {
			return;
		}

		mPasses.at(PassType::PickClear).execute(cmd);

		mTransitions.at(TransitionType::PickerGeneralIntoColorAttachment).execute(
			cmd, *mRendererScene.mPicker.mImage.image);

		mPasses.at(PassType::PickDraw).execute(cmd);

		mTransitions.at(TransitionType::PickerColorAttachmentIntoGeneral).execute(
			cmd, *mRendererScene.mPicker.mImage.image);

		mPasses.at(PassType::PickPick).execute(cmd);
	});

	mPasses.try_emplace(PassType::PickClear, [&](vk::CommandBuffer cmd) {
		vk::ClearColorValue clearColor(UINT_MAX, UINT_MAX, static_cast<uint32_t>(0), static_cast<uint32_t>(0));
		vk::ImageSubresourceRange range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
		cmd.clearColorImage(*mRendererScene.mPicker.mImage.image, vk::ImageLayout::eGeneral, clearColor,range);
	});

	mPasses.try_emplace(PassType::PickDraw, [&](vk::CommandBuffer cmd) {
		vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mRendererScene.mPicker.mImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
		vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mRendererScene.mPicker.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare);
		const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vkhelper::extent3dTo2d(mRendererScene.mPicker.mImage.imageExtent), &colorAttachment, &depthAttachment);

		cmd.beginRendering(renderInfo);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mRendererScene.mPicker.mDrawPipelineBundle.pipeline);
		vkhelper::setViewportScissors(cmd, mRendererScene.mPicker.mImage.imageExtent);
		cmd.bindIndexBuffer(*mRendererScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mRendererScene.mPicker.mDrawPipelineBundle.layout, 0, *mRendererScene.mPerspective.mDescriptorSet, nullptr);

		for (auto& batch : mRendererScene.mBatches | std::views::values) {
			if (batch.renderItems.empty()) { continue; }

			mRendererScene.mPicker.mDrawPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
			cmd.pushConstants<PickerDrawPushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mRendererScene.mPicker.mDrawPushConstants);

			cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem));
		}

		cmd.endRendering();
	});

	mPasses.try_emplace(PassType::PickPick, [&](vk::CommandBuffer cmd) {
		int32_t mouseClickLocation[2] = { static_cast<int32_t>(ImGui::GetIO().MousePos.x), static_cast<int32_t>(ImGui::GetIO().MousePos.y) };
		std::memcpy(mRendererScene.mPicker.mBuffer.info.pMappedData, &mouseClickLocation, 2 * sizeof(int32_t));

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mRendererScene.mPicker.mPickPipelineBundle.pipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mRendererScene.mPicker.mPickPipelineBundle.layout, 0, *mRendererScene.mPicker.mDescriptorSet, nullptr);
		cmd.pushConstants<PickerPickPushConstants>(
			mRendererScene.mPicker.mPickPipelineBundle.layout,
			vk::ShaderStageFlagBits::eCompute, 0,
			mRendererScene.mPicker.mPickPushConstants);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mRendererScene.mPicker.mBuffer.buffer,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostWrite,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

		cmd.dispatch(1, 1, 1);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mRendererScene.mPicker.mBuffer.buffer,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderWrite,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostRead);

		glm::uvec2 read(0);
		std::memcpy(glm::value_ptr(read), static_cast<char*>(mRendererScene.mPicker.mBuffer.info.pMappedData) + sizeof(glm::ivec2), 2 * sizeof(uint32_t));

		int modelId = read.x;

		auto reverseIt = mRendererScene.mModelsReverse.find(modelId);
		if (reverseIt == mRendererScene.mModelsReverse.end()) return; 
		std::string& clickedModelName = reverseIt->second;

		auto cacheIt = mRendererScene.mModelsCache.find(clickedModelName);
		if (cacheIt == mRendererScene.mModelsCache.end()) return; 
		GLTFModel& clickedModel = cacheIt->second;

		int localInstanceIndex = read.y - clickedModel.mMainFirstInstance;
		glm::mat4& clickedInstanceTransformMatrix = clickedModel.mInstances[localInstanceIndex].mData.transformMatrix;

		LOG_DEBUG(mLogger, "Got {},{}", clickedModel.mName, localInstanceIndex);

		mRendererScene.mPicker.imguizmoStart();
		mRendererScene.mPicker.imguizmoManipulate(clickedInstanceTransformMatrix);

		for (int row = 0; row < 4; row++) {
			LOG_DEBUG(mLogger, 
				"{:6.2f} {:6.2f} {:6.2f} {:6.2f}",
				clickedInstanceTransformMatrix[0][row], clickedInstanceTransformMatrix[1][row], clickedInstanceTransformMatrix[2][row], clickedInstanceTransformMatrix[3][row]
			);
		}
	});

	mPasses.try_emplace(PassType::Geometry, [&](vk::CommandBuffer cmd)
		{
			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
			vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
				*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
				vkhelper::extent3dTo2d(mRendererInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

			cmd.beginRendering(renderInfo);

			for (auto& batch : mRendererScene.mBatches | std::views::values)
			{
				if (batch.renderItems.empty()) { continue; }

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipelineBundle->pipeline);

				vkhelper::setViewportScissors(cmd, mRendererInfrastructure.mDrawImage.imageExtent);

				cmd.bindIndexBuffer(*mRendererScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 0,
					*mRendererScene.mPerspective.mDescriptorSet, nullptr);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 1,
					*mRendererScene.mMainMaterialResourcesDescriptorSet, nullptr);

				mRendererScene.mForwardPushConstants.visibleRenderItemsBuffer = batch.visibleRenderItemsBuffer.address;
				cmd.pushConstants<ForwardPushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mRendererScene.mForwardPushConstants);

				cmd.drawIndexedIndirectCount(*batch.visibleRenderItemsBuffer.buffer, 0, *batch.countBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem));

				mStats.mDrawCallCount++;
			}

			cmd.endRendering();
		});

	mPasses.try_emplace(PassType::Skybox, [&](vk::CommandBuffer cmd)
		{
			if (!mRendererScene.mSkybox.mActive) { return; }

			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
			vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
				*mRendererInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal,
				vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
				vkhelper::extent3dTo2d(mRendererInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

			cmd.beginRendering(renderInfo);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mRendererScene.mSkybox.mPipelineBundle.pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mRendererScene.mSkybox.mPipelineBundle.layout, 0,
				std::vector{
					 *mRendererScene.mPerspective.mDescriptorSet,
					 *mRendererScene.mSkybox.mDescriptorSet
				}, nullptr);
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
				vk::Offset2D{0, 0},
				vkhelper::extent3dTo2d(mRendererInfrastructure.mDrawImage.imageExtent),
			};
			cmd.setScissor(0, scissor);
			cmd.pushConstants<SkyBoxPushConstants>(mRendererScene.mSkybox.mPipelineBundle.layout,
				vk::ShaderStageFlagBits::eVertex, 0,
				mRendererScene.mSkybox.mPushConstants);

			cmd.draw(NUMBER_OF_SKYBOX_VERTICES, 1, 0, 0);
			mStats.mDrawCallCount++;

			cmd.endRendering();
		});

	mPasses.try_emplace(PassType::ResolveMSAA, [&](vk::CommandBuffer cmd)
		{
			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mRendererInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal,
				vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eDontCare,
				*mRendererInfrastructure.mIntermediateImage.imageView);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mRendererInfrastructure.mSwapchainBundle.mExtent,
				&colorAttachment, nullptr);

			cmd.beginRendering(renderInfo);
			cmd.endRendering();
		});

	mPasses.try_emplace(PassType::IntermediateToSwapchain, [&](vk::CommandBuffer cmd)
		{
			vkhelper::copyImage(
				cmd,
				*mRendererInfrastructure.mIntermediateImage.image, mRendererInfrastructure.getCurrentSwapchainImage().image,
				vkhelper::extent3dTo2d(mRendererInfrastructure.mIntermediateImage.imageExtent),
				mRendererInfrastructure.mSwapchainBundle.mExtent
			);
		});

	mPasses.try_emplace(PassType::ImGui, [&](vk::CommandBuffer cmd)
		{
			vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
				*mRendererInfrastructure.getCurrentSwapchainImage().imageView, vk::ImageLayout::eColorAttachmentOptimal,
				vk::AttachmentLoadOp::eDontCare);
			const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mRendererInfrastructure.mSwapchainBundle.mExtent,
				&colorAttachment, nullptr);

			cmd.beginRendering(renderInfo);
			mGUI.imguiFrame();
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

		if (mRendererInfrastructure.mProgramEndFrameNumber.has_value() && (mRendererInfrastructure.mFrameNumber <
			mRendererInfrastructure.mProgramEndFrameNumber.value()))
		{
			mRendererCore.mDevice.waitIdle();
			break;
		}

		while (SDL_PollEvent(&e) != 0)
		{
			mRendererEvent.executeEventCallbacks(e);
		}

		if (mStopRendering)
		{
			// Do not draw if minimized
			std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Throttle the speed to avoid endless spinning
			continue;
		}

		SDL_SetRelativeMouseMode(mCamera.mRelativeMode);
		if (mRendererInfrastructure.mResizeRequested)
		{
			mRendererInfrastructure.resizeSwapchain();
		}

		perFrameUpdate();
		draw();

		mRendererInfrastructure.mFrameNumber++;

		auto end = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		mStats.mFrameTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
	}
}

void Renderer::perFrameUpdate()
{
	const auto start = std::chrono::system_clock::now();

	mRendererScene.mPerspective.update();

	mRendererScene.deleteModels();
	mRendererScene.deleteInstances();

	for (auto& model : mRendererScene.mModelsCache | std::views::values) {
		if (model.mReloadLocalInstancesBuffer) {
			model.updateInstances();
		}
	}

	if (mRendererScene.mFlags.modelAddedFlag || mRendererScene.mFlags.modelDestroyedFlag)
	{
		mRendererScene.realignOffsets();
		mRendererScene.reloadMainBuffers();
		mRendererScene.regenerateRenderItems();
	}
	else if (mRendererScene.mFlags.instanceAddedFlag || mRendererScene.mFlags.instanceDestroyedFlag)
	{
		mRendererScene.realignInstancesOffset();
		mRendererScene.reloadMainInstancesBuffer();
		mRendererScene.regenerateRenderItems();
	}
	else if (mRendererScene.mFlags.reloadMainInstancesBuffer)
	{
		mRendererScene.reloadMainInstancesBuffer();
	}

	mRendererScene.resetFlags();

	mImmSubmit.queuedSubmit();
	mImmSubmit.mCallbacks.clear();

	const auto end = std::chrono::system_clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::draw()
{
	auto start = std::chrono::system_clock::now();

	mRendererCore.mDevice.waitForFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);
	mRendererCore.mDevice.resetFences(*mRendererInfrastructure.getCurrentFrame().mRenderFence);
	try
	{
		mRendererInfrastructure.mSwapchainIndex = mRendererInfrastructure.mSwapchainBundle.mSwapchain.acquireNextImage(
			1e9, *mRendererInfrastructure.getCurrentFrame().mAvailableSemaphore, nullptr).second;
	}
	catch (vk::OutOfDateKHRError e)
	{
		mRendererInfrastructure.mResizeRequested = true;
		return;
	}

	vk::CommandBuffer cmd = *mRendererInfrastructure.getCurrentFrame().mCommandBuffer;
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
		cmd, *mRendererInfrastructure.mIntermediateImage.image);

	mPasses.at(PassType::ResolveMSAA).execute(cmd);

	mTransitions.at(TransitionType::IntermediateColorAttachmentIntoTransferSrc).execute(
		cmd, *mRendererInfrastructure.mIntermediateImage.image);
	mTransitions.at(TransitionType::SwapchainPresentIntoTransferDst).execute(
		cmd, mRendererInfrastructure.getCurrentSwapchainImage().image);

	mPasses.at(PassType::IntermediateToSwapchain).execute(cmd);

	mTransitions.at(TransitionType::SwapchainTransferDstIntoColorAttachment).execute(
		cmd, mRendererInfrastructure.getCurrentSwapchainImage().image);

	mPasses.at(PassType::ImGui).execute(cmd);

	mTransitions.at(TransitionType::SwapchainColorAttachmentIntoPresent).execute(
		cmd, mRendererInfrastructure.getCurrentSwapchainImage().image);

	cmd.end();

	vk::CommandBufferSubmitInfo cmdinfo = vkhelper::commandBufferSubmitInfo(cmd);
	vk::SemaphoreSubmitInfo waitInfo = vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		*mRendererInfrastructure.getCurrentFrame().
		mAvailableSemaphore);
	vk::SemaphoreSubmitInfo signalInfo = vkhelper::semaphoreSubmitInfo(
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		*mRendererInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
	const vk::SubmitInfo2 submit = vkhelper::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

	mRendererCore.mGraphicsQueue.submit2(submit, *mRendererInfrastructure.getCurrentFrame().mRenderFence);

	// Prepare present. Wait on the mRenderSemaphore for queue commands to finish before image is presented.
	vk::PresentInfoKHR presentInfo = {};
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &(*mRendererInfrastructure.mSwapchainBundle.mSwapchain);
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &(*mRendererInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &mRendererInfrastructure.mSwapchainIndex;

	try
	{
		mRendererCore.mGraphicsQueue.presentKHR(presentInfo);
	}
	catch (vk::OutOfDateKHRError e)
	{
		mRendererInfrastructure.mResizeRequested = true;
	}

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
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
