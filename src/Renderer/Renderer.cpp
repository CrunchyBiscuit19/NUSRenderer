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
    initTransitions();

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
    mScene.initBuffers();
    mScene.initDescriptor();
    mScene.initPushConstants();
    mScene.initKeyBinding();
    mScene.initComponents();
    mInfrastructure.initFrames();
    mGui.initDescriptors();
    mGui.initBackend();
    mGui.initLinearColors();
    mGui.initFileBrowsers();
    mGui.initComponents();
    mGui.initKeyBinding();
    mStats.initBuffers();
    mCamera.initControls();
    mCamera.initBuffers();

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

        vkhelper::createBufferPipelineBarrier(  // Wait for stats total count buffer to be reset to zero
            cmd,
            *mStats.mRenderInstancesCountBuffer.buffer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        vkhelper::createBufferPipelineBarrier(  // Wait for visible instances indices buffer to be used finish by the indirect draw commands
            cmd,
            *mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.buffer,
            vk::PipelineStageFlagBits2::eVertexShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite
        );

        cmd.fillBuffer(*mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.buffer, 0, vk::WholeSize, UINT32_MAX);

        vkhelper::createBufferPipelineBarrier(  // Zero out visisble instances indices buffer before writing into it in CullCompact
            cmd,
            *mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.buffer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                vkhelper::createBufferPipelineBarrier(  // Wait for post cull render items count buffer to be used finish by the indirect draw commands
                    cmd,
                    *batch.postCullRenderItemsCountBuffer.buffer,
                    vk::PipelineStageFlagBits2::eDrawIndirect,
                    vk::AccessFlagBits2::eIndirectCommandRead,
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite
                );

                cmd.fillBuffer(*batch.postCullRenderItemsCountBuffer.buffer, 0, vk::WholeSize, 0);

                vkhelper::createBufferPipelineBarrier(  // Zero out render items buffer before writing into it in CullCompact
                    cmd,
                    *batch.postCullRenderItemsCountBuffer.buffer,
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                vkhelper::createBufferPipelineBarrier(  // Wait for post cull render items buffer to be used finish by the indirect draw commands
                    cmd,
                    *batch.postCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eDrawIndirect,
                    vk::AccessFlagBits2::eIndirectCommandRead,
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite
                );

                cmd.fillBuffer(*batch.postCullRenderItemsBuffer.buffer, 0, vk::WholeSize, 0);

                vkhelper::createBufferPipelineBarrier(  // Zero out render items buffer before writing into it in CullCompact
                    cmd,
                    *batch.postCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eTransfer,
                    vk::AccessFlagBits2::eTransferWrite,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                vkhelper::createBufferPipelineBarrier(  // Wait for preCullRenderItemsBuffer to be copied into postCullRenderItemsBuffer in CullCompact
                    cmd,
                    *batch.preCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                mScene.mCuller.mResetPushConstants.preCullRenderItemsBuffer = batch.preCullRenderItemsBuffer.address;
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

        vk::Extent3D depthPyramidExtent = mScene.mCuller.mDepthPyramidImage.imageExtent;
        vk::Extent3D resolvedDepthExtent = mScene.mCuller.mResolvedDepthImage.imageExtent;

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mScene.mCuller.mDepthPyramidPipelineBundle.layout, 0, *mScene.mCuller.mDepthPyramidDescriptorSet, nullptr
        );

        mScene.mCuller.mDepthPyramidPushConstants.depthPyramidExtent = glm::uvec2(depthPyramidExtent.width, depthPyramidExtent.height);
        mScene.mCuller.mDepthPyramidPushConstants.resolvedDepthExtent = glm::uvec2(resolvedDepthExtent.width, resolvedDepthExtent.height);
        mScene.mCuller.mDepthPyramidPushConstants.resolvedDepthRatio = glm::vec2(
            depthPyramidExtent.width / static_cast<float>(resolvedDepthExtent.width), depthPyramidExtent.height / static_cast<float>(resolvedDepthExtent.height)
        );
        mScene.mCuller.mDepthPyramidPushConstants.readFromResolved = true;
        mScene.mCuller.mDepthPyramidPushConstants.level = 0;
        cmd.pushConstants<CullerDepthPyramidPushConstants>(
            mScene.mCuller.mDepthPyramidPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mDepthPyramidPushConstants
        );

        vkhelper::transitionImage(
            cmd,
            *mScene.mCuller.mDepthPyramidImage.image,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        cmd.dispatch(
            vkhelper::fastCeil(mScene.mCuller.mResolvedDepthImage.imageExtent.width, MAX_2D_WORKGROUP_THREADS),
            vkhelper::fastCeil(mScene.mCuller.mResolvedDepthImage.imageExtent.height, MAX_2D_WORKGROUP_THREADS),
            1
        );

        mScene.mCuller.mDepthPyramidPushConstants.readFromResolved = false;

        for (u32 i = 0; i < mScene.mCuller.mDepthPyramidLevels; i++) {
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute, mScene.mCuller.mDepthPyramidPipelineBundle.layout, 0, *mScene.mCuller.mDepthPyramidDescriptorSet, nullptr
            );

            mScene.mCuller.mDepthPyramidPushConstants.level = i;
            cmd.pushConstants<CullerDepthPyramidPushConstants>(
                mScene.mCuller.mDepthPyramidPipelineBundle.layout, vk::ShaderStageFlagBits::eCompute, 0, mScene.mCuller.mDepthPyramidPushConstants
            );

            vkhelper::createImagePipelineBarrier(
                cmd,
                *mScene.mCuller.mDepthPyramidImage.image,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
                vk::ImageLayout::eGeneral
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

        mScene.mCuller.mCullPushConstants.renderInstancesCountBuffer = mStats.mRenderInstancesCountBuffer.address;
        mScene.mCuller.mCullPushConstants.mainBoundsBuffer = mScene.mMainBoundsBuffer.address;
        mScene.mCuller.mCullPushConstants.frustumBuffer = mCamera.mFrustumBuffer.address;
        mScene.mCuller.mCullPushConstants.perspectiveBuffer = mInfrastructure.getCurrentFrame().mPerspectiveBuffer.address;
        mScene.mCuller.mCullPushConstants.mainNodeTransformsBuffer = mScene.mMainNodeTransformsBuffer.address;
        mScene.mCuller.mCullPushConstants.mainInstancesBuffer = mScene.mMainInstancesBuffer.address;
        mScene.mCuller.mCullPushConstants.mainVisibleRenderInstancesInstanceIndexBuffer = mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.address;
        mScene.mCuller.mCullPushConstants.drawExtents = glm::vec2(mInfrastructure.mDrawImage.imageExtent.width, mInfrastructure.mDrawImage.imageExtent.height);

        vkhelper::transitionImage(
            cmd,
            *mScene.mCuller.mDepthPyramidImage.image,
            vk::ImageLayout::eGeneral,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead
        );

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                vkhelper::createBufferPipelineBarrier(  // Wait for all render items to have instance count reset in CullReset
                    cmd,
                    *batch.preCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eCompute, mScene.mCuller.mCullPipelineBundle.layout, 0, *mScene.mCuller.mCullDescriptorSet, nullptr
                );

                mScene.mCuller.mCullPushConstants.preCullRenderItemsBuffer = batch.preCullRenderItemsBuffer.address;
                mScene.mCuller.mCullPushConstants.renderInstancesLimit = batch.renderInstances.size();
                mScene.mCuller.mCullPushConstants.renderInstancesBuffer = batch.renderInstancesBuffer.address;
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

                vkhelper::createBufferPipelineBarrier(  // Wait for preCullRenderItemsBuffer to have instanceCount written in CullCull
                    cmd,
                    *batch.preCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead
                );

                vkhelper::createBufferPipelineBarrier(  // Wait for postCullRenderItemsBuffer to be used finish by the indirect draw commands
                    cmd,
                    *batch.postCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eDrawIndirect,
                    vk::AccessFlagBits2::eIndirectCommandRead,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderWrite
                );

                vkhelper::createBufferPipelineBarrier(  // Wait for postCullRenderItemsCountBuffer to be used finish by the indirect draw commands
                    cmd,
                    *batch.postCullRenderItemsCountBuffer.buffer,
                    vk::PipelineStageFlagBits2::eDrawIndirect,
                    vk::AccessFlagBits2::eIndirectCommandRead,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
                );

                mScene.mCuller.mCompactPushConstants.preCullRenderItemsBuffer = batch.preCullRenderItemsBuffer.address;
                mScene.mCuller.mCompactPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address;
                mScene.mCuller.mCompactPushConstants.postCullRenderItemsCountBuffer = batch.postCullRenderItemsCountBuffer.address;
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

                vkhelper::createBufferPipelineBarrier(  // Wait for visible instances indices buffer to be written to in CullCull
                    cmd,
                    *mScene.mMainVisibleRenderInstancesInstanceIndexBuffer.buffer,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderWrite,
                    vk::PipelineStageFlagBits2::eVertexShader,
                    vk::AccessFlagBits2::eShaderRead
                );

                vkhelper::createBufferPipelineBarrier(  // Wait for postCullRenderItemsBuffer to be written to in CullCompact
                    cmd,
                    *batch.postCullRenderItemsBuffer.buffer,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderWrite,
                    vk::PipelineStageFlagBits2::eDrawIndirect,
                    vk::AccessFlagBits2::eIndirectCommandRead
                );

                vkhelper::createBufferPipelineBarrier(  // Wait for postCullRenderItemsCountBuffer to be accurately counted in CullCompact
                    cmd,
                    *batch.postCullRenderItemsCountBuffer.buffer,
                    vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderWrite,
                    vk::PipelineStageFlagBits2::eDrawIndirect,
                    vk::AccessFlagBits2::eIndirectCommandRead
                );
            }
        }
    });

    mPasses.try_emplace(PassType::ClearScreen, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment =
            vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eClear);
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal, vk::AttachmentLoadOp::eClear);
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::Pick, [&](vk::CommandBuffer cmd) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || mCamera.mRelativeMode || ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        mPasses.at(PassType::PickClear).execute(cmd);

        mTransitions.at(TransitionType::PickerGeneralIntoColorAttachment).execute(cmd, *mScene.mPicker.mImage.image);

        mPasses.at(PassType::PickDraw).execute(cmd);

        mTransitions.at(TransitionType::PickerColorAttachmentIntoGeneral).execute(cmd, *mScene.mPicker.mImage.image);

        mPasses.at(PassType::PickPick).execute(cmd);
    });

    mPasses.try_emplace(PassType::PickClear, [&](vk::CommandBuffer cmd) {
        vk::ClearColorValue clearColor(0, 0, 0, 0);
        vk::ImageSubresourceRange range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        cmd.clearColorImage(*mScene.mPicker.mImage.image, vk::ImageLayout::eGeneral, clearColor, range);
    });

    mPasses.try_emplace(PassType::PickDraw, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(*mScene.mPicker.mImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mScene.mPicker.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(vkhelper::extent3dTo2d(mScene.mPicker.mImage.imageExtent), &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mPicker.mDrawPipelineBundle.pipeline);
        vkhelper::setViewportScissors(cmd, mScene.mPicker.mImage.imageExtent);
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

                mScene.mPicker.mDrawPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address;
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

        vkhelper::createBufferPipelineBarrier(
            cmd,
            *mScene.mPicker.mBuffer.buffer,
            vk::PipelineStageFlagBits2::eHost,
            vk::AccessFlagBits2::eHostWrite,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
        );

        vkhelper::createImagePipelineBarrier(
            cmd,
            *mScene.mPicker.mImage.image,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentReadNoncoherentEXT,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eGeneral
        );

        cmd.dispatch(1, 1, 1);

        vkhelper::createBufferPipelineBarrier(
            cmd,
            *mScene.mPicker.mBuffer.buffer,
            vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderWrite,
            vk::PipelineStageFlagBits2::eHost,
            vk::AccessFlagBits2::eHostRead
        );

        glm::uvec2 read(0);
        std::memcpy(glm::value_ptr(read), static_cast<char*>(mScene.mPicker.mBuffer.info.pMappedData) + sizeof(glm::ivec2), sizeof(glm::uvec2));

        u32 modelId = read.x;

        auto reverseIt = mScene.mModelsReverse.find(static_cast<u32>(modelId));
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

        u32 localInstanceIndex = read.y - clickedModel.mMainFirstInstance;
        mScene.mPicker.mClickedInstance = &clickedModel.mInstances[localInstanceIndex];
    });

    mPasses.try_emplace(PassType::Skybox, [&](vk::CommandBuffer cmd) {
        if (!mScene.mSkybox.mActive) {
            return;
        }

        vk::RenderingAttachmentInfo colorAttachment =
            vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *mScene.mSkybox.mPipelineBundle.pipeline);
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mScene.mSkybox.mPipelineBundle.layout,
            0,
            std::vector{*mInfrastructure.getCurrentFrame().mPerspectiveDescriptorSet, *mScene.mSkybox.mDescriptorSet},
            nullptr
        );
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
        cmd.pushConstants<SkyBoxPushConstants>(mScene.mSkybox.mPipelineBundle.layout, vk::ShaderStageFlagBits::eVertex, 0, mScene.mSkybox.mPushConstants);

        cmd.draw(NUMBER_OF_SKYBOX_VERTICES, 1, 0, 0);
        mStats.mDrawCallCount++;

        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::Geometry, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment =
            vkhelper::colorAttachmentInfo(*mInfrastructure.mDrawImage.imageView, vk::ImageLayout::eColorAttachmentOptimal);
        vk::RenderingAttachmentInfo depthAttachment =
            vkhelper::depthAttachmentInfo(*mInfrastructure.mDepthImage.imageView, vk::ImageLayout::eDepthAttachmentOptimal);
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(vkhelper::extent3dTo2d(mInfrastructure.mDrawImage.imageExtent), &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);

        for (auto batchType : mScene.mBatchTypes) {
            for (auto& batch : *batchType | std::views::values) {
                if (batch.renderItems.empty()) {
                    continue;
                }

                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *batch.pipelineBundle->pipeline);

                vkhelper::setViewportScissors(cmd, mInfrastructure.mDrawImage.imageExtent);

                cmd.bindIndexBuffer(*mScene.mMainIndexBuffer.buffer, 0, vk::IndexType::eUint32);

                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 0, *mInfrastructure.getCurrentFrame().mPerspectiveDescriptorSet, nullptr
                );
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, batch.pipelineBundle->layout, 1, *mScene.mMainMaterialResourcesDescriptorSet, nullptr);

                mScene.mGeometryPushConstants.postCullRenderItemsBuffer = batch.postCullRenderItemsBuffer.address;
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

    mPasses.try_emplace(PassType::ResolveMSAA, [&](vk::CommandBuffer cmd) {
        vk::RenderingAttachmentInfo colorAttachment = vkhelper::colorAttachmentInfo(
            *mInfrastructure.mDrawImage.imageView,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eDontCare,
            *mInfrastructure.mIntermediateImage.imageView
        );
        vk::RenderingAttachmentInfo depthAttachment = vkhelper::depthAttachmentInfo(
            *mInfrastructure.mDepthImage.imageView,
            vk::ImageLayout::eDepthAttachmentOptimal,
            vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eStore,
            *mScene.mCuller.mResolvedDepthImage.imageView
        );
        const vk::RenderingInfo renderInfo = vkhelper::renderingInfo(mInfrastructure.mSwapchainBundle.mExtent, &colorAttachment, &depthAttachment);

        cmd.beginRendering(renderInfo);
        cmd.endRendering();
    });

    mPasses.try_emplace(PassType::IntermediateToSwapchain, [&](vk::CommandBuffer cmd) {
        vkhelper::copyImage(
            cmd,
            *mInfrastructure.mIntermediateImage.image,
            mInfrastructure.getCurrentSwapchainImage().image,
            vkhelper::extent3dTo2d(mInfrastructure.mIntermediateImage.imageExtent),
            mInfrastructure.mSwapchainBundle.mExtent
        );
    });

    mPasses.try_emplace(PassType::ImGui, [&](vk::CommandBuffer cmd) {
        std::array<vk::RenderingAttachmentInfo, 2> colorAttachments = {
            vkhelper::colorAttachmentInfo(
                *mInfrastructure.getCurrentSwapchainImage().imageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eDontCare
            ),
            vkhelper::colorAttachmentInfo(
                *mInfrastructure.getCurrentSwapchainImage().uNormImageView, vk::ImageLayout::eColorAttachmentOptimal, vk::AttachmentLoadOp::eDontCare
            ),
        };
        const vk::RenderingInfo renderInfo =
            vkhelper::renderingInfo(mInfrastructure.mSwapchainBundle.mExtent, colorAttachments.data(), nullptr, colorAttachments.size());

        cmd.beginRendering(renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        cmd.endRendering();
    });
}

void Renderer::initTransitions() {
    mTransitions.try_emplace(
        TransitionType::PickerGeneralIntoColorAttachment,
        vk::ImageLayout::eGeneral,
        vk::PipelineStageFlagBits2::eClear,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentReadNoncoherentEXT
    );

    mTransitions.try_emplace(
        TransitionType::PickerColorAttachmentIntoGeneral,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentReadNoncoherentEXT,
        vk::ImageLayout::eGeneral,
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
    );

    mTransitions.try_emplace(
        TransitionType::IntermediateTransferSrcIntoColorAttachment,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite
    );

    mTransitions.try_emplace(
        TransitionType::IntermediateColorAttachmentIntoTransferSrc,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead
    );

    mTransitions.try_emplace(
        TransitionType::SwapchainColorPresentIntoTransferDst,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite
    );

    mTransitions.try_emplace(
        TransitionType::SwapchainColorTransferDstIntoColorAttachment,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite
    );

    mTransitions.try_emplace(
        TransitionType::SwapchainColorColorAttachmentIntoPresent,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eNone
    );
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
            mScene.mCuller.reconstructDepthPyramid();

            mInfrastructure.mResizeRequested = false;
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

    mPasses.at(PassType::Cull).execute(cmd);

    mPasses.at(PassType::ClearScreen).execute(cmd);

    mPasses.at(PassType::Pick).execute(cmd);

    mPasses.at(PassType::Skybox).execute(cmd);
    mPasses.at(PassType::Geometry).execute(cmd);

    mTransitions.at(TransitionType::IntermediateTransferSrcIntoColorAttachment).execute(cmd, *mInfrastructure.mIntermediateImage.image);

    mPasses.at(PassType::ResolveMSAA).execute(cmd);

    mTransitions.at(TransitionType::IntermediateColorAttachmentIntoTransferSrc).execute(cmd, *mInfrastructure.mIntermediateImage.image);
    mTransitions.at(TransitionType::SwapchainColorPresentIntoTransferDst).execute(cmd, mInfrastructure.getCurrentSwapchainImage().image);

    mPasses.at(PassType::IntermediateToSwapchain).execute(cmd);

    mTransitions.at(TransitionType::SwapchainColorTransferDstIntoColorAttachment).execute(cmd, mInfrastructure.getCurrentSwapchainImage().image);

    mPasses.at(PassType::ImGui).execute(cmd);

    mTransitions.at(TransitionType::SwapchainColorColorAttachmentIntoPresent).execute(cmd, mInfrastructure.getCurrentSwapchainImage().image);

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
