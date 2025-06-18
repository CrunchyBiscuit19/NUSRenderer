#include <Scene/Batch.h>
#include <Renderer/Renderer.h>

#include <fmt/core.h>

Batch::Batch(Renderer* renderer, Primitive& primitive, int pipelineId)
{
	pipelineBundle = primitive.mMaterial->mPipelineBundle;
	renderItemsBuffer = renderer->mRendererResources.createBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mRendererCore.labelResourceDebug(renderItemsBuffer.buffer,
	                                           fmt::format("RenderItemsBuffer{}", pipelineId).c_str());
	renderItemsBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*renderItemsBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Buffer Created", pipelineId);

	visibleRenderItemsBuffer = renderer->mRendererResources.createBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mRendererCore.labelResourceDebug(visibleRenderItemsBuffer.buffer,
	                                           fmt::format("VisibleRenderItemsBuffer{}", pipelineId).c_str());
	visibleRenderItemsBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*visibleRenderItemsBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Visible Render Items Buffer Created", pipelineId);

	countBuffer = renderer->mRendererResources.createBuffer(
		sizeof(uint32_t),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mRendererCore.labelResourceDebug(countBuffer.buffer, fmt::format("CountBuffer{}", pipelineId).c_str());
	countBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*countBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Count Buffer Created", pipelineId);

	renderItemsStagingBuffer = renderer->mRendererResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);
}

Batch::~Batch()
{
	renderItemsStagingBuffer.cleanup();
	countBuffer.cleanup();
	visibleRenderItemsBuffer.cleanup();
	renderItemsBuffer.cleanup();
	renderItems.clear();
}
