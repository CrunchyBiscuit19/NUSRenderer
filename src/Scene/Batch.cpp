#include <Scene/Batch.h>
#include <Renderer/Renderer.h>

#include <fmt/core.h>
#include <quill/LogMacros.h>

Batch::Batch(Renderer* renderer, Primitive& primitive, int pipelineId)
{
	pipelineBundle = primitive.mMaterial->mPipelineBundle;
	preCullRenderItemsBuffer = renderer->mResources.createBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(preCullRenderItemsBuffer.buffer,
	                                           fmt::format("RenderItemsBuffer{}", pipelineId).c_str());
	preCullRenderItemsBuffer.address = renderer->mCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*preCullRenderItemsBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Buffer Created", pipelineId);

	postCullRenderItemsBuffer = renderer->mResources.createBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(postCullRenderItemsBuffer.buffer,
	                                           fmt::format("VisibleRenderItemsBuffer{}", pipelineId).c_str());
	postCullRenderItemsBuffer.address = renderer->mCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*postCullRenderItemsBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Visible Render Items Buffer Created", pipelineId);

	countBuffer = renderer->mResources.createBuffer(
		sizeof(uint32_t),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(countBuffer.buffer, fmt::format("CountBuffer{}", pipelineId).c_str());
	countBuffer.address = renderer->mCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*countBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Count Buffer Created", pipelineId);

	preCullRenderItemsStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);
}

Batch::~Batch()
{
	preCullRenderItemsStagingBuffer.cleanup();
	countBuffer.cleanup();
	postCullRenderItemsBuffer.cleanup();
	preCullRenderItemsBuffer.cleanup();
	preCullRenderItems.clear();
}
