#include <Renderer/Renderer.h>
#include <Scene/Batch.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>
#include <vma/vk_mem_alloc.h>

u32 Batch::firstRenderInstanceOffset = 0;

Batch::Batch(Renderer* renderer, Primitive& primitive, u32 pipelineId) {
    pipelineBundle = primitive.mMaterial->mPipelineBundle;

    preCullRenderItemsBuffer = renderer->mResources.createBuffer(
        BATCH_MAX_RENDER_ITEMS * sizeof(RenderItem),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    );
    renderer->mCore.labelResourceDebug(preCullRenderItemsBuffer.buffer, fmt::format("PreCullRenderItemsBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Pre-Cull Render Items Buffer Created", pipelineId);

    renderItemsStagingBuffer = renderer->mResources.createStagingBuffer(RENDER_ITEMS_STAGING_BUFFER_SIZE);
    LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);

    postCullRenderItemsBuffer = renderer->mResources.createBuffer(
        BATCH_MAX_RENDER_ITEMS * sizeof(RenderItem),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    );
    renderer->mCore.labelResourceDebug(postCullRenderItemsBuffer.buffer, fmt::format("PostCullRenderItemsBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Post-Cull Render Items Buffer Created", pipelineId);

    postCullRenderItemsCountBuffer = renderer->mResources.createBuffer(
        sizeof(uint32_t),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    );
    renderer->mCore.labelResourceDebug(postCullRenderItemsCountBuffer.buffer, fmt::format("PostCullRenderItemsCountBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Post-Cull Render Items Count Buffer Created", pipelineId);

    renderInstancesBuffer = renderer->mResources.createBuffer(
        BATCH_MAX_RENDER_INSTANCES * sizeof(RenderInstance),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    );
    renderer->mCore.labelResourceDebug(renderInstancesBuffer.buffer, fmt::format("RenderInstancesBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Render Instances Buffer Created", pipelineId);

    renderInstancesStagingBuffer = renderer->mResources.createStagingBuffer(RENDER_INSTANCES_STAGING_BUFFER_SIZE);
    LOG_INFO(renderer->mLogger, "Batch {} Render Instances Staging Buffer Created", pipelineId);
}

Batch::~Batch() {
    renderInstancesStagingBuffer.cleanup();
    renderInstancesBuffer.cleanup();
    postCullRenderItemsCountBuffer.cleanup();
    postCullRenderItemsBuffer.cleanup();
    renderItemsStagingBuffer.cleanup();
    preCullRenderItemsBuffer.cleanup();
    renderItems.clear();
}
