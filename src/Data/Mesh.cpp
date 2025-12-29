#include <Data/Mesh.h>
#include <Renderer/Renderer.h>
#include <Utils/Types.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

void Node::refreshTransform(const glm::mat4& parentTransform) {
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& child : mChildren) child->refreshTransform(mWorldTransform);
}

void Node::generateRenderItemsInstances(Renderer* renderer, GLTFModel* model) {
    for (const auto& child : mChildren) child->generateRenderItemsInstances(renderer, model);
}

void MeshNode::generateRenderItemsInstances(Renderer* renderer, GLTFModel* model) {
    for (auto& primitive : mMesh->mPrimitives) {
        u32 pipelineId = primitive.mMaterial->mPipelineBundle->id;

        u32 batchType = static_cast<u32>(BatchType::Opaque);
        switch (primitive.mMaterial->mPbrData.alphaMode) {
            case fastgltf::AlphaMode::Opaque:
                batchType = static_cast<u32>(BatchType::Opaque);
                break;
            case fastgltf::AlphaMode::Mask:
                batchType = static_cast<u32>(BatchType::Mask);
                break;
            case fastgltf::AlphaMode::Blend:
                batchType = static_cast<u32>(BatchType::Transparent);
                break;
        }

        renderer->mScene.mBatchTypes[batchType]->try_emplace(pipelineId, renderer, primitive, pipelineId);
        renderer->mScene.mBatchTypes[batchType]
            ->at(pipelineId)
            .renderItems.emplace_back(
                primitive.mIndexCount,
                0,  // Instance count set to 0, incremented inside culling compute shader
                mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
                mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
                model->mMainFirstInstance,
                model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
                model->mMainFirstNodeTransform + this->mRelativeNodeIndex,
                model->mId,
                model->mMainFirstBounds + mMesh->mRelativeFirstBounds
            );

        RenderItem& currRenderItem = renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderItems.back();
        u32 renderItemIndex = static_cast<u32>(renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderItems.size() - 1);
        u32 instanceIndex = model->mMainFirstInstance;
        for (u32 i = 0; i < model->mInstances.size(); i++) {
            renderer->mScene.mBatchTypes[batchType]
                ->at(pipelineId)
                .renderInstances.emplace_back(
                    renderItemIndex,
                    instanceIndex + i,
                    currRenderItem.firstInstance,
                    currRenderItem.nodeTransformIndex,
                    currRenderItem.boundsIndex
                );
        }
    }

    Node::generateRenderItemsInstances(renderer, model);
}
