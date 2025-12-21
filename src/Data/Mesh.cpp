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

void MeshNode::generateRenderItems(Renderer* renderer, GLTFModel* model) {
	for (auto& primitive : mMesh->mPrimitives) {
		int pipelineId = primitive.mMaterial->mPipelineBundle->id;

		if (primitive.mMaterial->mPbrData.alphaMode == fastgltf::AlphaMode::Opaque) {
			renderer->mScene.mOpaqueBatches.try_emplace(pipelineId, renderer, primitive, pipelineId);
			renderer->mScene.mOpaqueBatches.at(pipelineId).preCullRenderItems.emplace_back(
				primitive.mIndexCount,
				model->mInstances.size(),
				mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
				mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
				model->mMainFirstInstance,
				model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
				model->mMainFirstNodeTransform + this->mRelativeNodeIndex,
				model->mId,
				0
			);
		} else {
			renderer->mScene.mTransparentBatches.try_emplace(pipelineId, renderer, primitive, pipelineId);
			renderer->mScene.mTransparentBatches.at(pipelineId).preCullRenderItems.emplace_back(
				primitive.mIndexCount,
				model->mInstances.size(),
				mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
				mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
				model->mMainFirstInstance,
				model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
				model->mMainFirstNodeTransform + this->mRelativeNodeIndex,
				model->mId,
				0
			);
		}
	}

	Node::generateRenderItems(renderer, model);
}
