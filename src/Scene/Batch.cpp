#include <Scene/Batch.h>
#include <Renderer/Renderer.h>

#include <fmt/core.h>
#include <quill/LogMacros.h>

Batch::Batch(Renderer* renderer, Primitive& primitive, int pipelineId)
{
	pipelineBundle = primitive.mMaterial->mPipelineBundle;

	renderItemsBuffer = renderer->mResources.createAddressedBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(renderItemsBuffer.buffer,
		fmt::format("RenderItemsBuffer{}", pipelineId).c_str());
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Buffer Created", pipelineId);

	renderItemsStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);

	renderInstancesBuffer = renderer->mResources.createAddressedBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderInstance),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(renderInstancesBuffer.buffer,
		fmt::format("RenderInstancesBuffer{}", pipelineId).c_str());
	LOG_INFO(renderer->mLogger, "Batch {} Render Instances Buffer Created", pipelineId);

	renderInstancesStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderInstance));
	LOG_INFO(renderer->mLogger, "Batch {} Render Instances Staging Buffer Created", pipelineId);
}

Batch::~Batch() {
	renderInstancesStagingBuffer.cleanup();
	renderInstancesBuffer.cleanup();
	renderItemsStagingBuffer.cleanup();
	renderItemsBuffer.cleanup();
	renderItems.clear();
}
