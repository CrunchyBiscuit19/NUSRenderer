#pragma once

#include <Data/Camera.h>
#include <Renderer/RendererCore.h>
#include <Renderer/RendererEvent.h>
#include <Renderer/RendererInfrastructure.h>
#include <Renderer/RendererResources.h>
#include <Renderer/RendererScene.h>
#include <Renderer/RendererStats.h>
#include <User/Gui.h>
#include <Utils/Helper.h>
#include <Utils/ImmSubmit.h>
#include <quill/Logger.h>

enum class PassType {
    Cull,
    CullReset,
    CullDepthPyramid,
    CullCull,
    CullCompact,
    CullSyncBeforeDraw,
    ClearScreen,
    Pick,
    PickClear,
    PickDraw,
    PickPick,
    Geometry,
    Skybox,
    ResolveMSAA,
    IntermediateToSwapchain,
    ImGui
};

struct Pass {
    static Renderer* renderer;
    std::function<void(vk::CommandBuffer)> function;

    Pass(const std::function<void(vk::CommandBuffer)>& function) : function(function) {}

    void execute(vk::CommandBuffer cmd) const { function(cmd); }
};

enum class TransitionType {
    PickerGeneralIntoColorAttachment,
    PickerColorAttachmentIntoGeneral,
    IntermediateTransferSrcIntoColorAttachment,
    IntermediateColorAttachmentIntoTransferSrc,
    SwapchainPresentIntoTransferDst,
    SwapchainTransferDstIntoColorAttachment,
    SwapchainColorAttachmentIntoPresent,
};

struct Transition {
    vk::ImageLayout currentLayout;
    vk::PipelineStageFlags2 srcStageMask;
    vk::AccessFlags2 srcAccessMask;
    vk::ImageLayout newLayout;
    vk::PipelineStageFlags2 dstStageMask;
    vk::AccessFlags2 dstAccessMask;

    Transition(
        vk::ImageLayout currentLayout, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::ImageLayout newLayout,
        vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask
    )
        : currentLayout(currentLayout),
          srcStageMask(srcStageMask),
          srcAccessMask(srcAccessMask),
          newLayout(newLayout),
          dstStageMask(dstStageMask),
          dstAccessMask(dstAccessMask) {}

    void execute(vk::CommandBuffer cmd, vk::Image image) {
        vkhelper::transitionImage(cmd, image, currentLayout, srcStageMask, srcAccessMask, newLayout, dstStageMask, dstAccessMask);
    }
};

class Renderer {
   public:
    bool mIsInitialized{false};
    bool mStopRendering{false};

    RendererStats mStats;
    RendererCore mCore;
    RendererInfrastructure mInfrastructure;
    RendererResources mResources;
    RendererScene mScene;
    RendererEvent mEventHandler;
    ImmSubmit mImmSubmit;
    Gui mGui;
    Camera mCamera;
    quill::Logger* mLogger;

    std::unordered_map<PassType, Pass> mPasses;
    std::unordered_map<TransitionType, Transition> mTransitions;

    Renderer();

    void init();
    void initLogger();
    void initComponents();
    void initPasses();
    void initTransitions();

    void run();
    void perFrameUpdate();
    void draw();

    void cleanup();
};