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
	uint32_t mRelativeFirstIndex;
	uint32_t mIndexCount;
	uint32_t mRelativeVertexOffset;
	PbrMaterial* mMaterial;
};

struct Mesh {
	std::string mName;
	int mId;
	std::vector<Primitive> mPrimitives;

	AABB mBounds;

	AllocatedBuffer mVertexBuffer;
	uint32_t mNumVertices;
	uint32_t mMainVertexOffset;
	
	AllocatedBuffer mIndexBuffer;
	uint32_t mNumIndices;
	uint32_t mMainFirstIndex;
};

struct Node {
	std::string mName;
	int mRelativeNodeIndex;
	std::weak_ptr<Node> mParent;
	std::vector<std::shared_ptr<Node>> mChildren;
	glm::mat4 mLocalTransform;
	glm::mat4 mWorldTransform;

	void refreshTransform(const glm::mat4& parentTransform);
	virtual void generateRenderItems(Renderer* renderer, GLTFModel* model);

	virtual ~Node() = default;
};

struct MeshNode : Node {
	Mesh* mMesh;

	void generateRenderItems(Renderer* renderer, GLTFModel* model) override;
};

struct RenderItem {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
	uint32_t firstInstance;
	uint32_t materialIndex;
	uint32_t nodeTransformIndex;
	// uint32_t boundsIndex;
};