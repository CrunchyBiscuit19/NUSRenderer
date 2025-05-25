#pragma once

#include <RendererCore.h>
#include <RendererInfrastructure.h>
#include <RendererResources.h>
#include <RendererScene.h>
#include <ImmSubmit.h>
#include <Gui.h>
#include <Camera.h>

struct RendererStats {
    float mFrametime;
    float mDrawTime;
    int mDrawCallCount;
    float mSceneUpdateTime;
};

class Renderer {
public:
    bool mIsInitialized { false };
    bool mStopRendering { false };

    RendererStats mStats;
    bool mRegenRenderItems { false };

    RendererCore mRendererCore;
    RendererInfrastructure mRendererInfrastructure;
    RendererResources mRendererResources;
    RendererScene mRendererScene;
    ImmSubmit mImmSubmit;
    GUI mGUI;
    Camera mCamera;

    Renderer();

    void init();
    void run();
    void cleanup();

    void drawUpdate();

    void draw();
    void drawClearScreen(vk::CommandBuffer cmd);
    void drawGeometry(vk::CommandBuffer cmd);
    void drawSkybox(vk::CommandBuffer cmd);
    void drawGui(vk::CommandBuffer cmd, vk::ImageView targetImageView);

    void resolveMsaa(vk::CommandBuffer cmd);
};