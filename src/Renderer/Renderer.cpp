#include <Renderer/Renderer.h>
#include <fmt/core.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

#include <glm/gtc/type_ptr.hpp>z
#include <ranges>

Renderer* Pass::renderer = nullptr;

Renderer::Renderer()
    : mCore(RendererCore(this)),
      mInfrastructure(RendererInfrastructure(this)),
      mResources(RendererResources(this)),
      mScene(RendererScene(this)),
      mImmSubmit(ImmSubmit(this)),
      mGui(Gui(this)),
      mCamera(Camera(this)),
      mEventHandler(RendererEvent(this)),
      mStats(RendererStats(this)) {}

void Renderer::init() {
    initLogger();
    initComponents();
    initPasses();

    LOG_INFO(mLogger, "Rendering Started");
}

void Renderer::initLogger() {
    quill::Backend::start();

    auto fileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        fmt::format("{}Run.log", LOGS_PATH).c_str(),
        []() {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('w');
            cfg.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
            return cfg;
        }(),
        quill::FileEventNotifier{}
    );
    auto latestFileSink = quill::Frontend::create_or_get_sink<quill::FileSink>(
        fmt::format("{}Latest.log", LOGS_PATH).c_str(),
        []() {
            quill::FileSinkConfig cfg;
            cfg.set_open_mode('w');
            return cfg;
        }(),
        quill::FileEventNotifier{}
    );
    auto consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink1");

    if (LOG_LOCATION == LogLocation::File) {
        mLogger = quill::Frontend::create_or_get_logger("LOGGER", {std::move(fileSink), std::move(latestFileSink)});
    } else if (LOG_LOCATION == LogLocation::Console) {
        mLogger = quill::Frontend::create_or_get_logger("LOGGER", std::move(consoleSink));
    } else if (LOG_LOCATION == LogLocation::Both) {
        mLogger = quill::Frontend::create_or_get_logger("LOGGER", {std::move(fileSink), std::move(latestFileSink), std::move(consoleSink)});
    }
    mLogger->set_log_level(quill::LogLevel::TraceL3);
}

void Renderer::initComponents() {
    mCore.init();
    mImmSubmit.init();
    mResources.initStaging();
    mResources.initDefaultImages();
    mResources.initDefaultSampler();
    mInfrastructure.initSwapchain();
    mInfrastructure.initDescriptors();
    mCamera.initControls();
    mCamera.initBuffers();
    mStats.initBuffers();
    mScene.initBuffers();
    mScene.initDescriptor();
    mScene.initKeyBinding();
    mScene.initComponents();
    mInfrastructure.initFrames();
    mGui.initDescriptors();
    mGui.initBackend();
    mGui.initLinearColors();
    mGui.initFileBrowsers();
    mGui.initComponents();
    mGui.initKeyBinding();
    mScene.initPushConstants();

    PbrMaterial::initMaterialPipelineLayout(this);
    mImmSubmit.queuedSubmit();
    mImmSubmit.mCallbacks.clear();

    mEventHandler.addEventCallback([this](SDL_Event& e) -> void {
        if (e.type == SDL_QUIT) {
            for (auto& model : mScene.mModelsCache | std::views::values) {
                model.markDelete();
            }
            mInfrastructure.mProgramEndFrameNumber = mInfrastructure.mFrameNumber + FRAME_OVERLAP + 1;
        }
        if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) mStopRendering = true;
            if (e.window.event == SDL_WINDOWEVENT_RESTORED) mStopRendering = false;
        }
        ImGui_ImplSDL2_ProcessEvent(&e);
    });
}

void Renderer::initPasses() {
    mPasses.try_emplace(PassType::Cull, [&](vk::CommandBuffer cmd) {
        if (mScene.mCuller.mFreezeCulling) return;
        mPasses.at(PassType::CullReset).execute(cmd);
        mPasses.at(PassType::CullDepthPyramid).execute(cmd);
        mPasses.at(PassType::CullCull).execute(cmd);
        mPasses.at(PassType::CullCompact).execute(cmd);
        mPasses.at(PassType::CullSyncBeforeDraw).execute(cmd);
    });

    mPasses.try_emplace(PassType::CullReset, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mCuller.mResetPipelineBundle.pipeline);

        cmd.fillBuffer(*mStats.mRenderInstancesCountBuffer.buffer, 0, vk::WholeSize, 0);

        mStats.mRenderInstancesCountBuffer.barrier(  // Wait for stats total count buffer to be reset to zero
            cmd,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        // Wait for visible instances indices buffer to be used finish by the indirect draw commands
        mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);

        cmd.fillBuffer(*mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.buffer, 0, vk::WholeSize, UINT32_MAX);

        mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.barrier(  // Zero out visisble instances indices buffer before writing into it in CullCompact
            cmd,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                // Wait for post cull render items count buffer to be used finish by the indirect draw commands
                batch.postCullRenderItemsCountBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);

                cmd.fillBuffer(*batch.postCullRenderItemsCountBuffer.buffer, 0, vk::WholeSize, 0);

                batch.postCullRenderItemsCountBuffer.barrier(  // Zero out render items buffer before writing into it in CullCompact
                    cmd,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                // Wait for post cull render items buffer to be used finish by the indirect draw commands
                batch.postCullRenderItemsBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);

                cmd.fillBuffer(*batch.postCullRenderItemsBuffer.buffer, 0, vk::WholeSize, 0);

                batch.postCullRenderItemsBuffer.barrier(  // Zero out render items buffer before writing into it in CullCompact
                    cmd,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                batch.preCullRenderItemsBuffer.barrier(  // Wait for preCullRenderItemsBuffer to be copied into postCullRenderItemsBuffer in CullCompact
                    cmd,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                mScene.mCuller.mResetPushConstants.preCullRenderItemsBuffer = batch.preCullRenderItemsBuffer.address.value();
                mScene.mCuller.mResetPushConstants.preCullRenderItemsLimit = batch.renderItems.size();
                cmd.pushConstants<CullerResetPushConstants>(
                    mScene.mCuller.mResetPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mResetPushConstants
                );

                cmd.dispatch(vkhelper::fastCeil(batch.renderItems.size(), MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });

    mPasses.try_emplace(PassType::CullDepthPyramid, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mCuller.mDepthPyramidPipelineBundle.pipeline);

        vk::Extent3D depthPyramidExtent = mScene.mCuller.mDepthPyramidImage.extent;
        vk::Extent3D depthFullExtent = mInfrastructure.mDepthImage.extent;

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mScene.mCuller.mDepthPyramidPipelineBundle.layout, 0, *mScene.mCuller.mDepthPyramidDescriptorSet, nullptr
        );

        mScene.mCuller.mDepthPyramidPushConstants.readFromFull = true;
        mScene.mCuller.mDepthPyramidPushConstants.level = 0;
        cmd.pushConstants<CullerDepthPyramidPushConstants>(
            mScene.mCuller.mDepthPyramidPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mDepthPyramidPushConstants
        );

        mScene.mCuller.mDepthPyramidImage.transition(
            cmd, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        cmd.dispatch(
            vkhelper::fastCeil(mInfrastructure.mDepthImage.extent.width, MAX_2D_WORKGROUP_THREADS),
            vkhelper::fastCeil(mInfrastructure.mDepthImage.extent.height, MAX_2D_WORKGROUP_THREADS),
            1
        );

        mScene.mCuller.mDepthPyramidPushConstants.readFromFull = false;

        for (u32 i = 0; i < mScene.mCuller.mDepthPyramidLevels - 1; i++) {
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute, mScene.mCuller.mDepthPyramidPipelineBundle.layout, 0, *mScene.mCuller.mDepthPyramidDescriptorSet, nullptr
            );

            mScene.mCuller.mDepthPyramidPushConstants.level = i;
            cmd.pushConstants<CullerDepthPyramidPushConstants>(
                mScene.mCuller.mDepthPyramidPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mDepthPyramidPushConstants
            );

            mScene.mCuller.mDepthPyramidImage.barrier(
                cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
            );

            cmd.dispatch(
                vkhelper::fastCeil(depthPyramidExtent.width >> i, MAX_2D_WORKGROUP_THREADS),
                vkhelper::fastCeil(depthPyramidExtent.height >> i, MAX_2D_WORKGROUP_THREADS),
                1
            );
        }
    });

    mPasses.try_emplace(PassType::CullCull, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mCuller.mCullPipelineBundle.pipeline);

        mScene.mCuller.mCullPushConstants.perspectiveBuffer = mInfrastructure.getCurrentFrame().mPerspectiveBuffer.address.value();

        mScene.mCuller.mDepthPyramidImage.transition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
        );

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                batch.preCullRenderItemsBuffer.barrier(  // Wait for all render items to have instance count reset in CullReset
                    cmd,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eCompute, mScene.mCuller.mCullPipelineBundle.layout, 0, *mScene.mCuller.mCullDescriptorSet, nullptr
                );

                mScene.mCuller.mCullPushConstants.preCullRenderItemsBuffer = batch.preCullRenderItemsBuffer.address.value();
                mScene.mCuller.mCullPushConstants.renderInstancesLimit = batch.renderInstances.size();
                mScene.mCuller.mCullPushConstants.renderInstancesBuffer = batch.renderInstancesBuffer.address.value();
                cmd.pushConstants<CullerCullPushConstants>(
                    mScene.mCuller.mCullPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mCullPushConstants
                );

                cmd.dispatch(vkhelper::fastCeil(batch.renderInstances.size(), MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });

    mPasses.try_emplace(PassType::CullCompact, [&](vk::CommandBuffer cmd) {
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mCuller.mCompactPipelineBundle.pipeline);

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                // Wait for preCullRenderItemsBuffer to have instanceCount written in CullCull
                batch.preCullRenderItemsBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead);

                // Wait for postCullRenderItemsBuffer to be used finish by the indirect draw commands
                batch.postCullRenderItemsBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite);

                batch.postCullRenderItemsCountBuffer.barrier(  // Wait for postCullRenderItemsCountBuffer to be used finish by the indirect draw commands
                    cmd,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                mScene.mCuller.mCompactPushConstants.preCullRenderItemsBuffer = batch.preCullRenderItemsBuffer.address.value();
                mScene.mCuller.mCompactPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address.value();
                mScene.mCuller.mCompactPushConstants.postCullRenderItemsCountBuffer = batch.postCullRenderItemsCountBuffer.address.value();
                mScene.mCuller.mCompactPushConstants.preCullRenderItemsLimit = batch.renderItems.size();
                cmd.pushConstants<CullerCompactPushConstants>(
                    mScene.mCuller.mCompactPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mCompactPushConstants
                );

                cmd.dispatch(vkhelper::fastCeil(batch.renderItems.size(), MAX_1D_WORKGROUP_THREADS), 1, 1);
            }
        }
    });

    mPasses.try_emplace(PassType::CullSyncBeforeDraw, [&](vk::CommandBuffer cmd) {
        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                // Wait for visible instances indices buffer to be written to in CullCull
                mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderRead);

                // Wait for postCullRenderItemsBuffer to be written to in CullCompact
                batch.postCullRenderItemsBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead);

                // Wait for postCullRenderItemsBuffer to be written to in CullCompact
                batch.postCullRenderItemsCountBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead);
            }
        }
    });

    mPasses.try_emplace(PassType::ClearScreen, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 3> colorAttachments = {
            vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.view, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear),
            vkhelper::colorAttachmentInfo(*mScene.mTransparency.mAccumImage.view, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear),
            vkhelper::colorAttachmentInfo(*mScene.mTransparency.mRevealageImage.view, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear),

        };
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.view, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear);

        colorAttachments[1].clearValue.color = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f);
        colorAttachments[2].clearValue.color = vk::ClearColorValue(1.f, 0.f, 0.f, 0.f);

        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
            vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.extent), colorAttachments.data(), &depthAttachment, colorAttachments.size()
        );

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::Pick, [&](vk::CommandBuffer cmd) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || mCamera.mRelativeMode || ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        mScene.mPicker.mImage.transition(
            cmd,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentReadNoncoherentEXT
        );

        mPasses.at(PassType::PickClear).execute(cmd);

        mPasses.at(PassType::PickDraw).execute(cmd);

        mScene.mPicker.mImage.transition(
            cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderSampledRead
        );

        mPasses.at(PassType::PickPick).execute(cmd);
    });

    mPasses.try_emplace(PassType::PickClear, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 1> colorAttachments = {
            vkhelper::colorAttachmentInfo(*mScene.mPicker.mImage.view, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear)
        };
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mScene.mPicker.mDepthImage.view, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear);

        colorAttachments[0].clearValue.color = vk::ClearColorValue(0, 0, 0, 0);

        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(vkhelper::extent3dTo2d(mScene.mPicker.mImage.extent), colorAttachments.data(), &depthAttachment, colorAttachments.size());

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::PickDraw, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mScene.mPicker.mImage.view, vk::ImageLayout::eColorAttachmentOptimal);
        vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(*mScene.mPicker.mDepthImage.view, vk::ImageLayout::eDepthAttachmentOptimal);
        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vkhelper::extent3dTo2d(mScene.mPicker.mImage.extent), &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mPicker.mDrawPipelineBundle.pipeline);

        vkhelper::setViewportScissors(cmd, mScene.mPicker.mImage.extent);

        cmd.bindIndexBuffer(*mScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mScene.mPicker.mDrawPipelineBundle.layout,
            0,
            *mInfrastructure.getCurrentFrame().mPerspectiveDescriptorSet,
            nullptr
        );

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                mScene.mPicker.mDrawPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address.value();
                cmd.pushConstants<PickerDrawPushConstants>(
                    batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mPicker.mDrawPushConstants
                );

                cmd.drawIndexedIndirectCount(
                    *batch.postCullRenderItemsBuffer.buffer, 0, *batch.postCullRenderItemsCountBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem)
                );
            }
        }

        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::PickPick, [&](vk::CommandBuffer cmd) {
        std::array<i32, 2> mouseClickLocation = {static_cast<i32>(ImGui::GetIO().MousePos.x), static_cast<i32>(ImGui::GetIO().MousePos.y)};
        std::memcpy(mScene.mPicker.mBuffer.info.pMappedData, mouseClickLocation.data(), sizeof(glm::ivec2));

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *mScene.mPicker.mPickPipelineBundle.pipeline);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mScene.mPicker.mPickPipelineBundle.layout, 0, *mScene.mPicker.mDescriptorSet, nullptr);

        cmd.pushConstants<PickerPickPushConstants>(
            mScene.mPicker.mPickPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mPicker.mPickPushConstants
        );

        mScene.mPicker.mBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

        cmd.dispatch(1, 1, 1);

        mScene.mPicker.mBuffer.barrier(cmd, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead | vk::AccessFlagBits2::eHostWrite);

        glm::uvec2 read(0);
        std::memcpy(glm::value_ptr(read), static_cast<char*>(mScene.mPicker.mBuffer.info.pMappedData) + sizeof(glm::ivec2), sizeof(glm::uvec2));
        if (read.x == 0 || read.y == 0) {
            mScene.mPicker.mClickedInstance = nullptr;
            return;
        }

        u32 modelId = read.x - 1;

        if (!mScene.mModelsReverse.contains(modelId)) {
            mScene.mPicker.mClickedInstance = nullptr;
            return;
        }
        std::string& clickedModelName = mScene.mModelsReverse.at(modelId);

        if (!mScene.mModelsCache.contains(clickedModelName)) {
            mScene.mPicker.mClickedInstance = nullptr;
            return;
        }
        GLTFModel& clickedModel = mScene.mModelsCache.at(clickedModelName);

        u32 localInstanceIndex = (read.y - 1) - clickedModel.mMainFirstInstance;
        mScene.mPicker.mClickedInstance = &clickedModel.mInstances[localInstanceIndex];
    });

    mPasses.try_emplace(PassType::Skybox, [&](vk::CommandBuffer cmd) {
        if (!mScene.mSkybox.mActive) {
            return;
        }

        vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.view, vk::ImageLayout::eColorAttachmentOptimal);
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.view, vk::ImageLayout::eDepthAttachmentOptimal);
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.extent), &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mSkybox.mPipelineBundle.pipeline);
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mScene.mSkybox.mPipelineBundle.layout,
            0,
            std::vector{*mInfrastructure.getCurrentFrame().mPerspectiveDescriptorSet, *mScene.mSkybox.mDescriptorSet},
            nullptr
        );
        vkhelper::setViewportScissors(cmd, mInfrastructure.mDrawImage.extent);
        cmd.pushConstants<SkyBoxPushConstants>(mScene.mSkybox.mPipelineBundle.layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mSkybox.mPushConstants);

        cmd.draw(NUMBER_OF_SKYBOX_VERTICES, 1, 0, 0);
        mStats.mDrawCallCount++;

        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::Opaque, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 3> colorAttachments = {
            vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.view, vk::ImageLayout::eColorAttachmentOptimal),
            vk::RenderingAttachmentInfo{},
            vk::RenderingAttachmentInfo{},
        };
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.view, vk::ImageLayout::eDepthAttachmentOptimal);

        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
            vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.extent), colorAttachments.data(), &depthAttachment, colorAttachments.size()
        );

        cmd.beginRendering(renderInfo);

        std::array<std::unordered_map<u32, Batch>*, 2> opaqueMaskBatchTypes = {
            mScene.mBatchTypes[static_cast<i32>(BatchType::Opaque)],
            mScene.mBatchTypes[static_cast<i32>(BatchType::Mask)],
        };
        for (auto batchType : opaqueMaskBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipelineBundle->pipeline);

                vkhelper::setViewportScissors(cmd, mInfrastructure.mDrawImage.extent);

                cmd.bindIndexBuffer(*mScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 0, *mInfrastructure.getCurrentFrame().mPerspectiveDescriptorSet, nullptr
                );
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 1, *mScene.mMainMaterialResourcesDescriptorSet, nullptr);

                mScene.mGeometryPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address.value();
                cmd.pushConstants<GeometryPushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mGeometryPushConstants);

                cmd.drawIndexedIndirectCount(
                    *batch.postCullRenderItemsBuffer.buffer, 0, *batch.postCullRenderItemsCountBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem)
                );

                mStats.mDrawCallCount++;
                mStats.mPreCullRenderInstancesCount += batch.renderInstances.size();
            }
        }

        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::Transparent, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 3> colorAttachments = {
            vk::RenderingAttachmentInfo{},
            vkhelper::colorAttachmentInfo(*mScene.mTransparency.mAccumImage.view, vk::ImageLayout::eColorAttachmentOptimal),
            vkhelper::colorAttachmentInfo(*mScene.mTransparency.mRevealageImage.view, vk::ImageLayout::eColorAttachmentOptimal),
        };
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.view, vk::ImageLayout::eDepthAttachmentOptimal);

        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(
            vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.extent), colorAttachments.data(), &depthAttachment, colorAttachments.size()
        );

        cmd.beginRendering(renderInfo);

        for (auto& batch : mScene.mTransparentBatches | std::views::values) {
            if (batch.renderItems.empty()) {
                continue;
            }

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipelineBundle->pipeline);

            vkhelper::setViewportScissors(cmd, mInfrastructure.mDrawImage.extent);

            cmd.bindIndexBuffer(*mScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 0, *mInfrastructure.getCurrentFrame().mPerspectiveDescriptorSet, nullptr
            );
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 1, *mScene.mMainMaterialResourcesDescriptorSet, nullptr);

            mScene.mGeometryPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address.value();
            cmd.pushConstants<GeometryPushConstants>(batch.pipelineBundle->layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mGeometryPushConstants);

            cmd.drawIndexedIndirectCount(
                *batch.postCullRenderItemsBuffer.buffer, 0, *batch.postCullRenderItemsCountBuffer.buffer, 0, MAX_RENDER_ITEMS, sizeof(RenderItem)
            );

            mStats.mDrawCallCount++;
            mStats.mPreCullRenderInstancesCount += batch.renderInstances.size();
        }

        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::Composite, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.view, vk::ImageLayout::eColorAttachmentOptimal);
        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.extent), &colorAttachment, nullptr, 1);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mTransparency.mPipelineBundle.pipeline);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mScene.mTransparency.mPipelineBundle.layout, 0, *mScene.mTransparency.mDescriptorSet, nullptr);

        vkhelper::setViewportScissors(cmd, mInfrastructure.mDrawImage.extent);

        cmd.draw(NUM_FULLSCREEN_QUAD_VERTICES, 1, 0, 0);

        mStats.mDrawCallCount++;

        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::ResolveMSAA, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
            *mInfrastructure.mDrawImage.view,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eDontCare,
            *mInfrastructure.mIntermediateImage.view
        );
        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mInfrastructure.mSwapchainBundle.mExtent, &colorAttachment, nullptr);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::FinalColorToSwapchain, [&](vk::CommandBuffer cmd) {
        if (MSAA_ENABLE) {
            vkhelper::copyImage(
                cmd,
                *mInfrastructure.mIntermediateImage.image,
                mInfrastructure.getCurrentSwapchainImage().image,
                vkhelper::extent3dTo2d(mInfrastructure.mIntermediateImage.extent),
                mInfrastructure.mSwapchainBundle.mExtent
            );
        } else {
            vkhelper::copyImage(
                cmd,
                *mInfrastructure.mDrawImage.image,
                mInfrastructure.getCurrentSwapchainImage().image,
                vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.extent),
                mInfrastructure.mSwapchainBundle.mExtent
            );
        }
    });

    mPasses.try_emplace(PassType::ImGui, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 2> colorAttachments = {
            vkhelper::colorAttachmentInfo(
                *mInfrastructure.getCurrentSwapchainImage().view, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eDontCare
            ),
            vkhelper::colorAttachmentInfo(
                *mInfrastructure.getCurrentSwapchainImage().uNormView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eDontCare
            ),
        };
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(mInfrastructure.mSwapchainBundle.mExtent, colorAttachments.data(), nullptr, colorAttachments.size());

        cmd.beginRendering(renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        cmd.endRendering();
    });
}

void Renderer::run() {
    SDL_Event e;

    while (true) {
        auto start = std::chrono::system_clock::now();

        if (mInfrastructure.mProgramEndFrameNumber.has_value() && (mInfrastructure.mFrameNumber < mInfrastructure.mProgramEndFrameNumber.value())) {
            mCore.mDevice.waitIdle();
            break;
        }

        while (SDL_PollEvent(&e) != 0) {
            mEventHandler.executeEventCallbacks(e);
        }

        if (mStopRendering) {
            // Do not draw if minimized
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Throttle the speed to avoid endless spinning
            continue;
        }

        SDL_SetRelativeMouseMode(mCamera.mRelativeMode);
        if (mInfrastructure.mResizeRequested) {
            mCore.mDevice.waitIdle();

            mInfrastructure.resizeSwapchain();
            mScene.mCuller.resizeCuller();
            mScene.mPicker.resizePicker();
            mScene.mTransparency.resizeTransparency();

            mInfrastructure.mResizeRequested = false;

            mCore.mDevice.waitIdle();
        }

        mGui.updateFrame();
        mStats.reset();
        perFrameUpdate();

        draw();

        mInfrastructure.mFrameNumber++;

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.mFrameTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
    }
}

void Renderer::perFrameUpdate() {
    const auto start = std::chrono::system_clock::now();

    mScene.mPerspective.update();
    mCamera.uploadFrameFrustum();

    mScene.deleteModels();
    mScene.deleteInstances();

    for (auto& model : mScene.mModelsCache | std::views::values) {
        if (model.mReloadInstances) {
            model.reloadInstances();
            mScene.mFlags.reloadMainInstancesBuffer = true;
        }
    }

    if (mScene.mFlags.modelAddedFlag || mScene.mFlags.modelDestroyedFlag) {
        mScene.realignOffsets();
        mScene.reloadMainBuffers();
        mScene.regenerateRenderItemsInstances();
    } else if (mScene.mFlags.instanceAddedFlag || mScene.mFlags.instanceDestroyedFlag) {
        mScene.realignInstancesOffset();
        mScene.reloadMainInstancesBuffer();
        mScene.regenerateRenderItemsInstances();
    } else if (mScene.mFlags.reloadMainInstancesBuffer) {
        mScene.reloadMainInstancesBuffer();
    }

    mScene.resetFlags();

    mImmSubmit.queuedSubmit();
    mImmSubmit.mCallbacks.clear();

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mSceneUpdateTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::draw() {
    auto start = std::chrono::system_clock::now();

    auto _ = mCore.mDevice.waitForFences(*mInfrastructure.getCurrentFrame().mRenderFence, true, 1e9);
    mCore.mDevice.resetFences(*mInfrastructure.getCurrentFrame().mRenderFence);
    try {
        mInfrastructure.mSwapchainIndex =
            mInfrastructure.mSwapchainBundle.mSwapchain.acquireNextImage(1e9, *mInfrastructure.getCurrentFrame().mAvailableSemaphore, nullptr).value;
    } catch (vk::OutOfDateKHRError e) {
        mInfrastructure.mResizeRequested = true;
        return;
    }

    vk::CommandBuffer cmd = *mInfrastructure.getCurrentFrame().mCommandBuffer;
    cmd.reset();
    vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmdBeginInfo);

    mInfrastructure.mDepthImage.transition(
        cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead
    );

    mPasses.at(PassType::Cull).execute(cmd);

    mInfrastructure.mDepthImage.transition(
        cmd,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
    );

    if (MSAA_ENABLE) {
        mInfrastructure.mIntermediateImage.transition(
            cmd, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        );
    } else {
        mInfrastructure.mDrawImage.transition(
            cmd, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
        );
    }

    mPasses.at(PassType::ClearScreen).execute(cmd);

    mPasses.at(PassType::Pick).execute(cmd);

    mPasses.at(PassType::Skybox).execute(cmd);
    mPasses.at(PassType::Opaque).execute(cmd);
    mPasses.at(PassType::Transparent).execute(cmd);

    mScene.mTransparency.mAccumImage.transition(
        cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
    );
    mScene.mTransparency.mRevealageImage.transition(
        cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead
    );

    mPasses.at(PassType::Composite).execute(cmd);

    mScene.mTransparency.mAccumImage.transition(
        cmd, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
    );
    mScene.mTransparency.mRevealageImage.transition(
        cmd, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite
    );

    if (MSAA_ENABLE) mPasses.at(PassType::ResolveMSAA).execute(cmd);

    if (MSAA_ENABLE) {
        mInfrastructure.mIntermediateImage.transition(
            cmd, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead
        );
    } else {
        mInfrastructure.mDrawImage.transition(
            cmd, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead
        );
    }
    vkhelper::transitionImage(
        cmd,
        mInfrastructure.getCurrentSwapchainImage().image,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite
    );

    mPasses.at(PassType::FinalColorToSwapchain).execute(cmd);

    vkhelper::transitionImage(
        cmd,
        mInfrastructure.getCurrentSwapchainImage().image,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite
    );

    mPasses.at(PassType::ImGui).execute(cmd);

    vkhelper::transitionImage(
        cmd,
        mInfrastructure.getCurrentSwapchainImage().image,
        vk::ImageAspectFlagBits::eColor,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone
    );

    cmd.end();

    vk::CommandBufferSubmitInfo cmdinfo = vkhelper::commandBufferSubmitInfo(cmd);
    vk::SemaphoreSubmitInfo waitInfo =
        vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput, *mInfrastructure.getCurrentFrame().mAvailableSemaphore);
    vk::SemaphoreSubmitInfo signalInfo =
        vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlagBits2::eColorAttachmentOutput, *mInfrastructure.getCurrentSwapchainImage().renderedSemaphore);
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

    try {
        auto _ = mCore.mGraphicsQueue.presentKHR(presentInfo);
    } catch (vk::OutOfDateKHRError e) {
        mInfrastructure.mResizeRequested = true;
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.mDrawTime = static_cast<float>(elapsed.count()) / ONE_SECOND_IN_MS;
}

void Renderer::cleanup() {
    PbrMaterial::cleanup(this);

    mCamera.cleanup();
    mGui.cleanup();
    mStats.cleanup();
    mScene.cleanup();
    mImmSubmit.cleanup();
    mResources.cleanup();
    mInfrastructure.cleanup();
    mCore.cleanup();

    LOG_INFO(mLogger, "Rendering Ended");
}
