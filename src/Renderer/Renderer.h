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
    Opaque,
    Skybox,
    Transparent,
    Composite,
    ResolveMSAA,
    TransferSwapchain,
    ImGui,
    PresentSwapchain
};

struct Pass {
    static Renderer* renderer;
    std::function<void(vk::CommandBuffer)> function;

    Pass(const std::function<void(vk::CommandBuffer)>& function) : function(function) {}

    void execute(vk::CommandBuffer cmd) const { function(cmd); }
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

    Renderer();

    void init();
    void initLogger();
    void initComponents();
    void initPasses();

    void run();
    void perFrameUpdate();
    void draw();

    void cleanup();
};