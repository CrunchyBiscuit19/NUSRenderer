#pragma once

#include <models.h>
#include <vk_descriptors.h>
#include <vk_pipelines.h>

constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int NUMBER_OF_SWAPCHAIN_IMAGES = 3;

class Renderer;

struct Frame {
    vk::raii::CommandPool mCommandPool;
    vk::raii::CommandBuffer mCommandBuffer;
    vk::raii::Fence mRenderFence;

    Frame();

    void cleanup();
};

class SwapchainBundle {
public:
    struct SwapchainImage {
        vk::Image image;
        vk::raii::ImageView imageView;
        vk::raii::Semaphore availableSemaphore;
        vk::raii::Semaphore renderedSemaphore;
    };

    vk::raii::SwapchainKHR mSwapchain;
    vk::Extent2D mExtent;
    vk::Format mFormat;
    std::vector<SwapchainImage> mImages;

};

class RendererInfrastructure {
private:
    Renderer* mRenderer;

public:
    uint64_t mFrameNumber{ 0 }; // Normal 32-bit should also be fine, but just to safeguard against overflow use 64 bit int
    std::vector<Frame> mFrames;
    Frame& getCurrentFrame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }
    Frame& getPreviousFrame() { return mFrames[(mFrameNumber - 1) % FRAME_OVERLAP]; }
    SwapchainBundle::SwapchainImage& getCurrentSwapchainImage() { return mSwapchainBundle.mImages[mFrameNumber % NUMBER_OF_SWAPCHAIN_IMAGES]; }

    bool mResizeRequested;
    SwapchainBundle mSwapchainBundle;

    DescriptorAllocatorGrowable mDescriptorAllocator;

    AllocatedImage mDrawImage;
    AllocatedImage mIntermediateImage;
    AllocatedImage mDepthImage;

    std::unordered_map<PipelineOptions, PipelineBundle> mMaterialPipelines;
    std::unordered_map<PipelineOptions, PipelineBundle> mComputePipelines;

    RendererInfrastructure(Renderer* renderer);

    void init();

    void initDescriptors();
    void initFrames();
    void initSwapchain();
    void destroySwapchain();
    void resizeSwapchain();
    
    PipelineBundle* getMaterialPipeline(PipelineOptions pipelineOptions);
    void createMaterialPipeline(PipelineOptions pipelineOptions);
    void createComputePipeline(PipelineOptions pipelineOptions);
    void createSkyboxPipeline();

    void cleanup();
};