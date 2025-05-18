#include <renderer.h>
#include <meshes.h>

void Node::refreshTransform(const glm::mat4& parentTransform)
{
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& c : mChildren)
        c->refreshTransform(mWorldTransform);
}

void Node::generateRenderItems(Renderer* renderer, GLTFModel* model, const glm::mat4& topMatrix)
{
    for (const auto& c : mChildren)
        c->generateRenderItems(renderer, model, topMatrix);
}

void MeshNode::generateRenderItems(Renderer* renderer, GLTFModel* model, const glm::mat4& topMatrix)
{
    const glm::mat4 nodeMatrix = topMatrix * mWorldTransform;

    for (Primitive& primitive : mMesh->mPrimitives) {
        // Getting device address is relatively expensive, so do it here instead of at every draw loop
        vk::BufferDeviceAddressInfo vertexBufferDeviceAddressInfo;
        vertexBufferDeviceAddressInfo.buffer = *mMesh->mVertexBuffer.buffer;
        vk::BufferDeviceAddressInfo materialConstantBufferDeviceAddressInfo;
        materialConstantBufferDeviceAddressInfo.buffer = primitive.material->mConstantsBuffer;

        renderer->mRenderItems.emplace_back(
            &primitive,
            mMesh.get(),
            model,
            nodeMatrix,
            renderer->mRendererCore.mDevice.getBufferAddress(vertexBufferDeviceAddressInfo),
            renderer->mRendererCore.mDevice.getBufferAddress(materialConstantBufferDeviceAddressInfo)
        );
    }

    Node::generateRenderItems(renderer, model, topMatrix);
}
