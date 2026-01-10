#pragma once

#include <Data/Mesh.h>

struct PipelineBundle;

struct RenderItem {
    u32 indexCount;
    u32 renderInstanceCount;
    u32 firstIndex;
    u32 vertexOffset;
    u32 firstRenderInstance;
    u32 materialIndex;
    u32 nodeTransformIndex;
    u32 modelIndex;
    u32 firstInstance;
    u32 boundsIndex;
};

struct RenderInstance {
    u32 renderItemIndex;
    u32 mainInstanceIndex;
};

enum class BatchType { Opaque = 0, Mask = 1, Transparent = 2 };

struct Batch {
    static u32 firstRenderInstanceOffset;

    PipelineBundle* pipelineBundle;

    std::vector<RenderItem> renderItems;
    AllocatedBuffer preCullRenderItemsBuffer;
    AllocatedBuffer renderItemsStagingBuffer;
    AllocatedBuffer postCullRenderItemsBuffer;
    AllocatedBuffer postCullRenderItemsCountBuffer;

    std::vector<RenderInstance> renderInstances;
    AllocatedBuffer renderInstancesBuffer;
    AllocatedBuffer renderInstancesStagingBuffer;

    Batch(Renderer* renderer, Primitive& primitive, u32 pipelineId);
    ~Batch();

    Batch(Batch&&) noexcept = default;
    Batch& operator=(Batch&&) noexcept = default;

    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
};
