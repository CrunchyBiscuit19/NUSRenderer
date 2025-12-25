#include <Renderer/Renderer.h>
#include <Renderer/RendererStats.h>

#include <quill/LogMacros.h>

RendererStats::RendererStats(Renderer* renderer) :
	mRenderer(renderer),
	mFrameTime(0.0f),
	mDrawTime(0.0f),
	mDrawCallCount(0),
	mPreCullRenderInstancesCount(0),
	mSceneUpdateTime(0.0f) {
}

void RendererStats::initBuffers() {
	mPostCullRenderInstancesCountBuffer = mRenderer->mResources.createAddressedBuffer(sizeof(uint32_t), vk::BufferUsageFlagBits::eStorageBuffer | 
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);
	mRenderer->mCore.labelResourceDebug(mPostCullRenderInstancesCountBuffer.buffer, "PostCullRenderInstancesCountBuffer");
	LOG_INFO(mRenderer->mLogger, "Post Cull Render Instances Count Buffer Created");
}

void RendererStats::reset() {
	mDrawCallCount = 0;
	mPreCullRenderInstancesCount = 0;
}

void RendererStats::cleanup() {
	mPostCullRenderInstancesCountBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Post Cull Render Instances Count Buffer Destroyed");
}