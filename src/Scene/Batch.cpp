#include <Scene/Batch.h>
#include <Renderer/Renderer.h>

#include <fmt/core.h>
#include <quill/LogMacros.h>

Batch::Batch(Renderer* renderer, Primitive& primitive, int pipelineId)
{
	pipelineBundle = primitive.mMaterial->mPipelineBundle;

	preCullRenderItemsBuffer = renderer->mResources.createAddressedBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(preCullRenderItemsBuffer.buffer,
		fmt::format("PreCullRenderItemsBuffer{}", pipelineId).c_str());
	LOG_INFO(renderer->mLogger, "Batch {} Pre-Cull Render Items Buffer Created", pipelineId);

	preCullRenderItemsStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);

	preCullRenderInstancesBuffer = renderer->mResources.createAddressedBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderInstance),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(preCullRenderInstancesBuffer.buffer,
		fmt::format("PreCullRenderInstancesBuffer{}", pipelineId).c_str());
	LOG_INFO(renderer->mLogger, "Batch {} Pre-Cull Render Instances Buffer Created", pipelineId);

	preCullRenderInstancesStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderInstance));
	LOG_INFO(renderer->mLogger, "Batch {} Render Instances Staging Buffer Created", pipelineId);

	postCullRenderItemsBuffer = renderer->mResources.createAddressedBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(postCullRenderItemsBuffer.buffer,
		fmt::format("PostCullRenderItemsBuffer{}", pipelineId).c_str());
	LOG_INFO(renderer->mLogger, "Batch {} Post-Cull Render Items Buffer Created", pipelineId);

	postCullRenderInstancesCountBuffer = renderer->mResources.createAddressedBuffer(
		sizeof(uint32_t),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mCore.labelResourceDebug(postCullRenderInstancesCountBuffer.buffer, fmt::format("CountBuffer{}", pipelineId).c_str());
	LOG_INFO(renderer->mLogger, "Batch {} Count Buffer Created", pipelineId);
}

Batch::~Batch() {
	postCullRenderInstancesCountBuffer.cleanup();
	postCullRenderItemsBuffer.cleanup();
	preCullRenderInstancesStagingBuffer.cleanup();
	preCullRenderInstancesBuffer.cleanup();
	preCullRenderItemsStagingBuffer.cleanup();
	preCullRenderItemsBuffer.cleanup();
	preCullRenderItems.clear();
}
