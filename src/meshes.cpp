#include <renderer.h>
#include <meshes.h>

void Node::refreshTransform(const glm::mat4& parentTransform)
{
    mWorldTransform = parentTransform * mLocalTransform;
    for (const auto& c : mChildren)
        c->refreshTransform(mWorldTransform);
}

void Node::generateRenderItems(Renderer* renderer, const glm::mat4& topMatrix)
{
    for (const auto& c : mChildren)
        c->generateRenderItems(renderer, topMatrix);
}

void MeshNode::generateRenderItems(Renderer* renderer, const glm::mat4& topMatrix)
{
    const glm::mat4 nodeMatrix = topMatrix * mWorldTransform;

    for (Primitive& primitive : mMesh->mPrimitives) {
        vk::BufferDeviceAddressInfo deviceAddressInfo;
        deviceAddressInfo.buffer = *mMesh->mVertexBuffer.buffer;

        renderer->mRenderItems.emplace_back(
            primitive.indexStart,
            primitive.indexCount,
            *mMesh->mIndexBuffer.buffer,
            renderer->mDevice.getBufferAddress(deviceAddressInfo),
            primitive.bounds,
            primitive.material,
            nodeMatrix
        );
    }

    Node::generateRenderItems(renderer, topMatrix);
}
