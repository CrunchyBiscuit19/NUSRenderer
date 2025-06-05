#include <Renderer/Renderer.h>
#include <Data/Mesh.h>

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
		// Getting device address is relatively expensive, so do it here instead of at every draw loop
		vk::BufferDeviceAddressInfo vertexBufferDeviceAddressInfo;
		vertexBufferDeviceAddressInfo.buffer = *mMesh->mVertexBuffer.buffer;
		vk::BufferDeviceAddressInfo materialConstantBufferDeviceAddressInfo;
		materialConstantBufferDeviceAddressInfo.buffer = primitive.mMaterial->mConstantsBuffer;
		vk::BufferDeviceAddressInfo instancesBufferDeviceAddressInfo;
		instancesBufferDeviceAddressInfo.buffer = *model->mInstancesBuffer.buffer;

		renderer->mRendererScene.mRenderItems.emplace_back(
			&primitive,
			mMesh,
			model,
			mWorldTransform,
			renderer->mRendererCore.mDevice.getBufferAddress(vertexBufferDeviceAddressInfo),
			renderer->mRendererCore.mDevice.getBufferAddress(instancesBufferDeviceAddressInfo),
			renderer->mRendererCore.mDevice.getBufferAddress(materialConstantBufferDeviceAddressInfo)
		);
	}

	Node::generateRenderItems(renderer, model);
}
