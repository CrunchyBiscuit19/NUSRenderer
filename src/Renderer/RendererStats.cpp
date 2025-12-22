#include <Renderer/Renderer.h>
#include <Renderer/RendererStats.h>

RendererStats::RendererStats(Renderer* renderer) :
	mRenderer(renderer),
	mFrameTime(0.0f),
	mDrawTime(0.0f),
	mDrawCallCount(0),
	mPreCullMeshesCount(0),
	mPostCullMeshesCount(0),
	mSceneUpdateTime(0.0f) {
}

void RendererStats::initTotalPostCullCountBuffer() {
	mTotalPostCullCountBuffer = mRenderer->mResources.createBuffer(sizeof(uint32_t), vk::BufferUsageFlagBits::eStorageBuffer | 
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);
	mTotalPostCullCountBuffer.address = mRenderer->mCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mTotalPostCullCountBuffer.buffer));
}

void RendererStats::reset() {
	mDrawCallCount = 0;
	mPreCullMeshesCount = 0;
	mPostCullMeshesCount = 0;
}

void RendererStats::cleanup() {
	mTotalPostCullCountBuffer.cleanup();
}