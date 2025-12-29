#include <Renderer/Renderer.h>
#include <Scene/Batch.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

Batch::Batch(Renderer* renderer, Primitive& primitive, u32 pipelineId) {
    pipelineBundle = primitive.mMaterial->mPipelineBundle;

    preCullRenderItemsBuffer =
        renderer->mResources.createAddressedBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem),
                                                   vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
                                                       vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                   VMA_MEMORY_USAGE_GPU_ONLY);
    renderer->mCore.labelResourceDebug(preCullRenderItemsBuffer.buffer, fmt::format("PreCullRenderItemsBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Pre-Cull Render Items Buffer Created", pipelineId);

    renderItemsStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
    LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);

    postCullRenderItemsBuffer =
        renderer->mResources.createAddressedBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem),
                                                   vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
                                                       vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                   VMA_MEMORY_USAGE_GPU_ONLY);
    renderer->mCore.labelResourceDebug(postCullRenderItemsBuffer.buffer, fmt::format("PostCullRenderItemsBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Post-Cull Render Items Buffer Created", pipelineId);

    postCullRenderItemsCountBuffer =
        renderer->mResources.createBuffer(sizeof(uint32_t),
                                          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
                                              vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                          VMA_MEMORY_USAGE_GPU_ONLY);
    renderer->mCore.labelResourceDebug(postCullRenderItemsCountBuffer.buffer, fmt::format("PostCullRenderItemsCountBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Post-Cull Render Items Count Buffer Created", pipelineId);

    renderInstancesBuffer = renderer->mResources.createAddressedBuffer(
        MAX_RENDER_ITEMS * sizeof(RenderInstance),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_MEMORY_USAGE_GPU_ONLY);
    renderer->mCore.labelResourceDebug(renderInstancesBuffer.buffer, fmt::format("RenderInstancesBuffer{}", pipelineId).c_str());
    LOG_INFO(renderer->mLogger, "Batch {} Render Instances Buffer Created", pipelineId);

    renderInstancesStagingBuffer = renderer->mResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderInstance));
    LOG_INFO(renderer->mLogger, "Batch {} Render Instances Staging Buffer Created", pipelineId);
}

Batch::~Batch() {
    renderInstancesStagingBuffer.cleanup();
    renderInstancesBuffer.cleanup();
    renderItemsStagingBuffer.cleanup();
    preCullRenderItemsBuffer.cleanup();
    renderItems.clear();
}
