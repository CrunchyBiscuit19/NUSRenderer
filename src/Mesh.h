#pragma once

#include <Material.h>
#include <RendererResources.h>

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct Primitive {
    uint32_t indexStart;
    uint32_t indexCount;
    std::shared_ptr<PbrMaterial> material;
    Bounds bounds;
};

struct Mesh {
    std::string mName;
    std::vector<Primitive> mPrimitives;
    AllocatedBuffer mVertexBuffer;
    AllocatedBuffer mIndexBuffer;
};

struct Node {
    std::string mName;
    std::weak_ptr<Node> mParent;
    std::vector<std::shared_ptr<Node>> mChildren;
    glm::mat4 mLocalTransform;
    glm::mat4 mWorldTransform;

    void refreshTransform(const glm::mat4& parentTransform);
    virtual void generateRenderItems(Renderer* renderer, GLTFModel* model, const glm::mat4& topMatrix);

    virtual ~Node() = default;
};

struct MeshNode : Node {
    std::shared_ptr<Mesh> mMesh;
     
    void generateRenderItems(Renderer* renderer, GLTFModel* model, const glm::mat4& topMatrix) override;
};

struct RenderItem {
    Primitive* primitive;
    Mesh* mesh;
    GLTFModel* model;

    glm::mat4 transform;

    vk::DeviceAddress vertexBufferAddress;
    vk::DeviceAddress instancesBufferAddress;
    vk::DeviceAddress materialConstantsBufferAddress;
};