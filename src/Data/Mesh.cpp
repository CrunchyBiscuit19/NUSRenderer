#include <Data/Mesh.h>
#include <Renderer/Renderer.h>

#include <fmt/core.h>
#include <quill/LogMacros.h>

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
		renderer->mScene.mBatchTypes[batchType]->at(pipelineId).preCullRenderItems.emplace_back(
			primitive.mIndexCount,
			model->mInstances.size(),
			mMesh->mMainFirstIndex + primitive.mRelativeFirstIndex,
			mMesh->mMainVertexOffset + primitive.mRelativeVertexOffset,
			model->mMainFirstInstance,
			model->mMainFirstMaterial + primitive.mMaterial->mRelativeMaterialIndex,
			model->mMainFirstNodeTransform + this->mRelativeNodeIndex,
			model->mId,
			model->mMainFirstBounds + mMesh->mRelativeFirstBounds
		);
	}

	Node::generateRenderItems(renderer, model);
}
