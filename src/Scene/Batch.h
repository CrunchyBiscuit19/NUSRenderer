#pragma once

#include <Data/Mesh.h>

struct PipelineBundle;

struct RenderItem
{
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
	uint32_t firstInstance;
	uint32_t materialIndex;
	uint32_t nodeTransformIndex;
	// uint32_t boundsIndex;
	uint32_t modelId;
};

struct Batch
{
	PipelineBundle* pipelineBundle;
	std::vector<RenderItem> renderItems;
	AddressedBuffer renderItemsBuffer;
	AddressedBuffer visibleRenderItemsBuffer;
	AddressedBuffer countBuffer;
	AllocatedBuffer renderItemsStagingBuffer;

	Batch(Renderer* renderer, Primitive& primitive, int pipelineId);
	~Batch();

	Batch(Batch&&) noexcept = default;
	Batch& operator=(Batch&&) noexcept = default;

	Batch(const Batch&) = delete;
	Batch& operator=(const Batch&) = delete;
};
