#include <Data/Mesh.h>
#include <Renderer/Renderer.h>
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
        int pipelineId = primitive.mMaterial->mPipelineBundle->id;

        int batchType = static_cast<uint32_t>(BatchType::Opaque);
        switch (primitive.mMaterial->mPbrData.alphaMode) {
            case fastgltf::AlphaMode::Opaque:
                batchType = static_cast<uint32_t>(BatchType::Opaque);
                break;
            case fastgltf::AlphaMode::Mask:
                batchType = static_cast<uint32_t>(BatchType::Mask);
                break;
            case fastgltf::AlphaMode::Blend:
                batchType = static_cast<uint32_t>(BatchType::Transparent);
                break;
        }

        renderer->mScene.mBatchTypes[batchType]->try_emplace(pipelineId, renderer, primitive, pipelineId);
        renderer->mScene.mBatchTypes[batchType]
            ->at(pipelineId)
            .renderItems.emplace_back(primitive.mIndexCount, model->mInstances.size(), mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
                                      mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset, model->mMainFirstInstance,
                                      model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
                                      model->mMainFirstNodeTransform + this->mRelativeNodeIndex, model->mId,
                                      model->mMainFirstBounds + mMesh->mRelativeFirstBounds);

        uint32_t renderItemIndex = static_cast<uint32_t>(renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderItems.size() - 1);
        uint32_t instanceIndex = model->mMainFirstInstance;
        for (int i = 0; i < model->mInstances.size(); i++) {
            renderer->mScene.mBatchTypes[batchType]->at(pipelineId).renderInstances.emplace_back(renderItemIndex, instanceIndex + i);
        }
    }

    Node::generateRenderItemsInstances(renderer, model);
}
