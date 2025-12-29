#pragma once

#include <Data/Mesh.h>

struct PipelineBundle;

struct RenderItem {
    u32 indexCount;
    u32 instanceCount;
    u32 firstIndex;
    u32 vertexOffset;
    u32 firstInstance;
    u32 materialIndex;
    u32 nodeTransformIndex;
    u32 modelIndex;
    u32 boundsIndex;
};

struct RenderInstance {
    u32 renderItemIndex;  // Per batch render items index
    u32 instanceIndex;    // Global main instances buffer index
    u32 firstInstance;
    u32 nodeTransformIndex;
    u32 boundsIndex;
};

enum class BatchType { Opaque = 0, Mask = 1, Transparent = 2 };

struct Batch {
    PipelineBundle* pipelineBundle;

    std::vector<RenderItem> renderItems;
    AddressedBuffer preCullRenderItemsBuffer;
    AllocatedBuffer renderItemsStagingBuffer;
    AddressedBuffer postCullRenderItemsBuffer;
    AddressedBuffer postCullRenderItemsCountBuffer;

    std::vector<RenderInstance> renderInstances;
    AddressedBuffer renderInstancesBuffer;
    AllocatedBuffer renderInstancesStagingBuffer;

    Batch(Renderer* renderer, Primitive& primitive, u32 pipelineId);
    ~Batch();

    Batch(Batch&&) noexcept = default;
    Batch& operator=(Batch&&) noexcept = default;

    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
};
