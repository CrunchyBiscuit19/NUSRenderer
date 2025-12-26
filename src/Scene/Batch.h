#pragma once

#include <Data/Mesh.h>

struct PipelineBundle;

struct RenderItem {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t vertexOffset;
    uint32_t firstInstance;
    uint32_t materialIndex;
    uint32_t nodeTransformIndex;
    uint32_t modelIndex;
    uint32_t boundsIndex;
};

struct RenderInstance {
    uint32_t renderItemIndex;  // Per batch render items index
    uint32_t instanceIndex;    // Global main instances buffer index
};

enum class BatchType { Opaque = 0, Mask = 1, Transparent = 2 };

struct Batch {
    PipelineBundle* pipelineBundle;

    std::vector<RenderItem> renderItems;
    AddressedBuffer renderItemsBuffer;
    AllocatedBuffer renderItemsStagingBuffer;

    std::vector<RenderInstance> renderInstances;
    AddressedBuffer renderInstancesBuffer;
    AllocatedBuffer renderInstancesStagingBuffer;

    Batch(Renderer* renderer, Primitive& primitive, int pipelineId);
    ~Batch();

    Batch(Batch&&) noexcept = default;
    Batch& operator=(Batch&&) noexcept = default;

    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
};
