#pragma once

#include <renderer_core.h>
#include <renderer_infrastructure.h>
#include <resource_manager.h>
#include <scene_manager.h>
#include <imm_submit.h>
#include <gui.h>
#include <camera.h>

constexpr unsigned int EXPECTED_FRAME_RATE = 60;

constexpr unsigned int ONE_SECOND_IN_MS = 1000;

constexpr unsigned int ONE_MEBIBYTE_IN_BYTES = 1048576;
constexpr unsigned int MAX_IMAGE_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int DEFAULT_VERTEX_BUFFER_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;
constexpr unsigned int DEFAULT_INDEX_BUFFER_SIZE = 100 * ONE_MEBIBYTE_IN_BYTES;

constexpr unsigned int MAX_INSTANCES = 1000;
constexpr unsigned int MAX_MATERIALS = 1000;

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
    ResourceManager mResourceManager;
    SceneManager mSceneManager;
    ImmSubmit mImmSubmit;
    GUI mGUI;
    Camera mCamera;

    Renderer();

    void init();
    void run();
    void cleanup();

    void draw();
    void drawClearScreen(vk::CommandBuffer cmd);
    void drawGeometry(vk::CommandBuffer cmd);
    void drawSkybox(vk::CommandBuffer cmd);
    void drawGui(vk::CommandBuffer cmd, vk::ImageView targetImageView);

    void drawCleanup();
    void drawUpdate();
};