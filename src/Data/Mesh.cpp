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
    for (auto& primitive : mMesh->mPrimitives) {
        int pipelineId = primitive.mMaterial->mPipelineBundle->id;

        renderer->mRendererScene.mBatches.try_emplace(pipelineId, renderer, primitive, pipelineId);

        renderer->mRendererScene.mBatches.at(pipelineId).renderItems.emplace_back(
            primitive.mIndexCount, 
            model->mInstances.size(),
            mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
            mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
            model->mMainFirstInstance,
            model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
            model->mMainFirstNodeTransform + this->mRelativeNodeIndex,
            model->mId
        );
    }

    Node::generateRenderItems(renderer, model);
}
