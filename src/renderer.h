#pragma once

#define NOMINMAX // imfilebrowser.h contains windows.h

#include <renderer_core.h>
#include <renderer_infrastructure.h>
#include <resource_manager.h>
#include <scene_manager.h>
#include <imm_submit.h>
#include <gui.h>
#include <camera.h>
#include <models.h>
#include <vk_descriptors.h>

#include <imgui.h>
#include <imfilebrowser.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <map>

#ifdef NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

constexpr unsigned int FRAME_OVERLAP = 2;
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

struct RendererFlags {
    bool updateModels = false;
    bool updateInstances = false;
};

struct FrameResources {
    int mId;
    vk::raii::CommandPool mCommandPool;
    vk::raii::CommandBuffer mCommandBuffer;
    vk::raii::Semaphore mSwapchainSemaphore, mRenderSemaphore;
    vk::raii::Fence mRenderFence;

    FrameResources();

    void cleanup();
};

class Renderer {
public:
    bool mIsInitialized { false };
    bool mStopRendering { false };

    RendererStats mStats;
    RendererFlags mFlags;
    bool mRegenRenderItems { false };

    RendererCore mRendererCore;
    RendererInfrastructure mRendererInfrastructure;
    ResourceManager mResourceManager;
    SceneManager mSceneManager;
    ImmSubmit mImmSubmit;
    GUI mGUI;
    Camera mCamera;

    uint64_t mFrameNumber{ 0 }; // Normal 32-bit should also be fine, but just to safeguard against overflow use 64 bit int
    std::vector<FrameResources> mFrames;
    const FrameResources& getCurrentFrame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }
    const FrameResources& getPreviousFrame() { return mFrames[(mFrameNumber - 1) % FRAME_OVERLAP]; }

    vk::raii::Context mContext;
    vk::raii::Instance mInstance;
    vk::raii::DebugUtilsMessengerEXT mDebugMessenger;

    vk::raii::PhysicalDevice mChosenGPU;
    vk::raii::Device mDevice;
    vk::raii::SurfaceKHR mSurface;
    SDL_Window* mWindow{ nullptr };
    vk::Extent2D mWindowExtent{ 1700, 900 };

    vk::raii::Queue mComputeQueue;
    uint32_t mComputeQueueFamily;
    vk::raii::Queue mGraphicsQueue;
    uint32_t mGraphicsQueueFamily;

    VmaAllocator mAllocator;

    bool mResizeRequested;
    vk::raii::SwapchainKHR mSwapchain;
    vk::Format mSwapchainImageFormat;
    vk::Extent2D mSwapchainExtent;
    std::vector<vk::raii::ImageView> mSwapchainImageViews;

    DescriptorAllocatorGrowable mDescriptorAllocator;

    AllocatedImage mDrawImage;
    AllocatedImage mDepthImage;

    std::unordered_map<PipelineOptions, PipelineBundle> mMaterialPipelines;
    std::unordered_map<PipelineOptions, PipelineBundle> mComputePipelines;

    std::unordered_map<std::string, GLTFModel> mModels;
    std::vector<RenderItem> mRenderItems;

    PushConstants mPushConstants;
    SceneEncapsulation mSceneEncapsulation;

    std::unordered_map<DefaultImage, AllocatedImage> mDefaultImages;
    vk::raii::Sampler mDefaultSampler;

    ImGui::FileBrowser mSelectModelFileDialog;

    Renderer();

    void init();
    void run();
    void cleanup();

    void draw();
    void drawGui(vk::CommandBuffer cmd, vk::ImageView targetImageView);
    void drawGeometry(vk::CommandBuffer cmd);

    void drawCleanup();
    void drawUpdate();
};