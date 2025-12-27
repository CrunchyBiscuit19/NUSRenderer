#pragma once

#include <Renderer/RendererResources.h>
#include <Utils/Constexprs.h>
#include <Utils/Descriptor.h>

class Renderer;

struct Frame {
    vk::raii::CommandPool mCommandPool;
    vk::raii::CommandBuffer mCommandBuffer;
    vk::raii::Fence mRenderFence;
    vk::raii::Semaphore mAvailableSemaphore;

    AddressedBuffer mPerspectiveBuffer;
    vk::raii::DescriptorSet mPerspectiveDescriptorSet;

    Frame();

    void cleanup();
};

class SwapchainBundle {
   public:
    struct SwapchainImage {
        vk::Image image;
        vk::raii::ImageView imageView;
        vk::raii::ImageView uNormImageView;
        vk::raii::Semaphore renderedSemaphore;
    };

    vk::raii::SwapchainKHR mSwapchain;
    vk::Extent2D mExtent;
    vk::Format mFormat;
    vk::Format mUnormFormat;
    std::vector<SwapchainImage> mImages;
};

class RendererInfrastructure {
    Renderer* mRenderer;

   public:
    u64 mFrameNumber{0};
    // u32 should also be fine, but just to safeguard against overflow use u64
    std::optional<u64> mProgramEndFrameNumber{std::nullopt};
    std::vector<Frame> mFrames;
    inline Frame& getCurrentFrame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }
    inline Frame& getPreviousFrame() { return mFrames[(mFrameNumber - 1) % FRAME_OVERLAP]; }
    inline SwapchainBundle::SwapchainImage& getCurrentSwapchainImage() { return mSwapchainBundle.mImages[mSwapchainIndex]; }

    bool mResizeRequested{false};
    SwapchainBundle mSwapchainBundle;
    u32 mSwapchainIndex;

    DescriptorAllocatorGrowable mMainDescriptorAllocator;

    AllocatedImage mDrawImage;
    AllocatedImage mDepthImage;
    AllocatedImage mIntermediateImage;

    u32 mLatestPipelineId{0};

    RendererInfrastructure(Renderer* renderer);

    void initDescriptors();
    void initFrames();
    void initSwapchain();

    void destroySwapchain();
    void resizeSwapchain();

    void cleanup();
};
