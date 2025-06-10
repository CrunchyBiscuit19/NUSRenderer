#include <Data/Mesh.h>
#include <Renderer/Renderer.h>
#include <fmt/core.h>

void Node::refreshTransform(const glm::mat4& parentTransform)
{
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& child : mChildren)
        child->refreshTransform(mWorldTransform);
}

void Node::generateRenderItems(Renderer* renderer, GLTFModel* model)
{
    for (const auto& child : mChildren)
        child->generateRenderItems(renderer, model);
}

void MeshNode::generateRenderItems(Renderer* renderer, GLTFModel* model)
{
    for (Primitive& primitive : mMesh->mPrimitives) {
        int pipelineId = primitive.mMaterial->mPipeline->id;

        if (!renderer->mRendererScene.mSceneManager.mBatches.contains(pipelineId)) {
            Batch& currentBatch = renderer->mRendererScene.mSceneManager.mBatches[pipelineId];

            currentBatch.pipeline = primitive.mMaterial->mPipeline;
            currentBatch.renderItemsBuffer = renderer->mRendererResources.createBuffer(
                MAX_RENDER_ITEMS * sizeof(RenderItem),
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                VMA_MEMORY_USAGE_GPU_ONLY);
            renderer->mRendererCore.labelResourceDebug(
                currentBatch.renderItemsBuffer.buffer,
                fmt::format("RenderItemsBuffer{}", pipelineId).c_str());
            currentBatch.renderItemsBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(
                vk::BufferDeviceAddressInfo(
                    *currentBatch.renderItemsBuffer.buffer));

            currentBatch.visibleRenderItemsBuffer = renderer->mRendererResources.createBuffer(
                MAX_RENDER_ITEMS * sizeof(RenderItem),
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                VMA_MEMORY_USAGE_GPU_ONLY);
            renderer->mRendererCore.labelResourceDebug(
                currentBatch.visibleRenderItemsBuffer.buffer, fmt::format("VisibleRenderItemsBuffer{}", pipelineId).c_str());
            currentBatch.visibleRenderItemsBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(
                vk::BufferDeviceAddressInfo(*currentBatch.visibleRenderItemsBuffer.buffer));

            currentBatch.countBuffer = renderer->mRendererResources.createBuffer(
                sizeof(uint32_t),
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                VMA_MEMORY_USAGE_GPU_ONLY);
            renderer->mRendererCore.labelResourceDebug(currentBatch.countBuffer.buffer,
                fmt::format("CountBuffer{}", pipelineId).c_str());
            currentBatch.countBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(
                vk::BufferDeviceAddressInfo(*currentBatch.countBuffer.buffer));

            currentBatch.renderItemsStagingBuffer = renderer->mRendererResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
        }

        renderer->mRendererScene.mSceneManager.mBatches[pipelineId].renderItems.emplace_back(
            primitive.mIndexCount, 
            model->mInstances.size(),
            mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
            mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
            model->mMainFirstInstance,
            model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
            model->mMainFirstNodeTransform + this->mRelativeNodeIndex
        );
    }

    Node::generateRenderItems(renderer, model);
}
