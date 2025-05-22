#pragma once

#include <models.h>
#include <vk_descriptors.h>
#include <vk_pipelines.h>

constexpr unsigned int FRAME_OVERLAP = 2;

class Renderer;

struct Frame {
    vk::raii::CommandPool mCommandPool;
    vk::raii::CommandBuffer mCommandBuffer;
    vk::raii::Semaphore mSwapchainSemaphore, mRenderSemaphore;
    vk::raii::Fence mRenderFence;

    Frame();

    void cleanup();
};

class RendererInfrastructure {
private:
    Renderer* mRenderer;

public:
    uint64_t mFrameNumber{ 0 }; // Normal 32-bit should also be fine, but just to safeguard against overflow use 64 bit int
    std::vector<Frame> mFrames;
    const Frame& getCurrentFrame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }
    const Frame& getPreviousFrame() { return mFrames[(mFrameNumber - 1) % FRAME_OVERLAP]; }

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

    RendererInfrastructure(Renderer* renderer);

    void init();

    void initCommands();
    void initDescriptors();
    void initSyncStructures();

    void createSwapchain();
    void destroySwapchain();
    void resizeSwapchain();
    
    PipelineBundle* getMaterialPipeline(PipelineOptions pipelineOptions);
    void createMaterialPipeline(PipelineOptions pipelineOptions);
    void createComputePipeline(PipelineOptions pipelineOptions);
    void createSkyboxPipeline();
    void destroyPipelines();

    void cleanup();
};