#pragma once

#include <Utils/Constexprs.h>
#include <Data/Model.h>
#include <Utils/Descriptor.h>
#include <Utils/Pipeline.h>

class Renderer;

struct Frame {
	vk::raii::CommandPool mCommandPool;
	vk::raii::CommandBuffer mCommandBuffer;
	vk::raii::Fence mRenderFence;
	vk::raii::Semaphore mAvailableSemaphore;

	Frame();

	void cleanup();
};

class SwapchainBundle {
public:
	struct SwapchainImage {
		vk::Image image;
		vk::raii::ImageView imageView;
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
	std::optional<uint64_t> mProgramEndFrameNumber{ std::nullopt };
	std::vector<Frame> mFrames;
	Frame& getCurrentFrame() { return mFrames[mFrameNumber % FRAME_OVERLAP]; }
	Frame& getPreviousFrame() { return mFrames[(mFrameNumber - 1) % FRAME_OVERLAP]; }
	SwapchainBundle::SwapchainImage& getCurrentSwapchainImage() { return mSwapchainBundle.mImages[mSwapchainIndex]; }

	bool mResizeRequested{ false };
	SwapchainBundle mSwapchainBundle;
	uint32_t mSwapchainIndex;

	DescriptorAllocatorGrowable mMainDescriptorAllocator;

	AllocatedImage mDrawImage;
	AllocatedImage mDepthImage;
	AllocatedImage mIntermediateImage;

	int mLatestPipelineId{ 0 };

	std::unordered_map<PipelineOptions, vk::raii::Pipeline> mMaterialPipelinesCache;
	vk::raii::PipelineLayout mMaterialPipelineLayout;

	vk::raii::Pipeline mCullPipeline;
	vk::raii::PipelineLayout mCullPipelineLayout;

	RendererInfrastructure(Renderer* renderer);

	void init();

	void initDescriptors();
	void initFrames();
	void initCullPipeline();
	void initPipelineLayouts();
	void initSwapchain();
	
	void destroySwapchain();
	void resizeSwapchain();

	PipelineBundle getMaterialPipeline(PipelineOptions materialPipelineOptions);
	void createMaterialPipeline(PipelineOptions materialPipelineOptions);
	void createCullPipeline();
	void createSkyboxPipeline();

	void cleanup();
};