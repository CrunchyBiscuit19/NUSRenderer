#pragma once

#include <Data/Material.h>
#include <Renderer/RendererResources.h>

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct AABB {
    glm::vec4 min;
    glm::vec4 max;
};

struct Primitive {
    u32 mRelativeFirstIndex;
    u32 mIndexCount;
    u32 mRelativeVertexOffset;
    PbrMaterial* mMaterial;
};

class Mesh {
   public:
    std::string mName;
    u32 mId;
    std::vector<Primitive> mPrimitives;

    AABB mBounds;
    u32 mRelativeFirstBounds;

    AllocatedBuffer mVertexBuffer;
    u32 mNumVertices{0};
    u32 mMainVertexOffset{0};

    AllocatedBuffer mIndexBuffer;
    u32 mNumIndices{0};
    u32 mMainFirstIndex{0};
};

class Node {
   public:
    std::string mName;
    u32 mRelativeNodeIndex;
    std::weak_ptr<Node> mParent;
    std::vector<std::shared_ptr<Node>> mChildren;
    glm::mat4 mLocalTransform;
    glm::mat4 mWorldTransform;

    void refreshTransform(const glm::mat4& parentTransform);
    virtual void generateRenderItemsInstances(Renderer* renderer, GLTFModel* model);

    virtual ~Node() = default;
};

class MeshNode : public Node {
   public:
    Mesh* mMesh;

    void generateRenderItemsInstances(Renderer* renderer, GLTFModel* model) override;
};
