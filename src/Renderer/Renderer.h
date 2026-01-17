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

using PassFn = void (*)(Renderer*, vk::CommandBuffer);

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

namespace PassFns {
void cull(Renderer* r, vk::CommandBuffer cmd);
void cullReset(Renderer* r, vk::CommandBuffer cmd);
void cullDepthPyramid(Renderer* r, vk::CommandBuffer cmd);
void cullCull(Renderer* r, vk::CommandBuffer cmd);
void cullCompact(Renderer* r, vk::CommandBuffer cmd);
void cullSyncBeforeDraw(Renderer* r, vk::CommandBuffer cmd);
void clearScreen(Renderer* r, vk::CommandBuffer cmd);
void pick(Renderer* r, vk::CommandBuffer cmd);
void pickClear(Renderer* r, vk::CommandBuffer cmd);
void pickDraw(Renderer* r, vk::CommandBuffer cmd);
void pickPick(Renderer* r, vk::CommandBuffer cmd);
void opaque(Renderer* r, vk::CommandBuffer cmd);
void skybox(Renderer* r, vk::CommandBuffer cmd);
void transparent(Renderer* r, vk::CommandBuffer cmd);
void composite(Renderer* r, vk::CommandBuffer cmd);
void resolveMSAA(Renderer* r, vk::CommandBuffer cmd);
void transferSwapchain(Renderer* r, vk::CommandBuffer cmd);
void imgui(Renderer* r, vk::CommandBuffer cmd);
void presentSwapchain(Renderer* r, vk::CommandBuffer cmd);
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

    std::array<PassFn, static_cast<size_t>(PassType::PresentSwapchain) + 1> mPassTable;

    Renderer();

    void init();
    void initLogger();
    void initComponents();
    void initPasses();

    void executePass(PassType type, vk::CommandBuffer cmd);

    void run();
    void perFrameUpdate();
    void draw();

    void cleanup();
};