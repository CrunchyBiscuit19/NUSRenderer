#pragma once

#include <Data/Material.h>
#include <Renderer/RendererResources.h>

struct Vertex
{
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct AABB
{
	glm::vec4 min;
	glm::vec4 max;
};

struct Primitive
{
	uint32_t mRelativeFirstIndex;
	uint32_t mIndexCount;
	uint32_t mRelativeVertexOffset;
	PbrMaterial* mMaterial;
};

class Mesh
{
public:
	std::string mName;
	int mId;
	std::vector<Primitive> mPrimitives;

	AABB mBounds;

	AllocatedBuffer mVertexBuffer;
	uint32_t mNumVertices{0};
	uint32_t mMainVertexOffset{0};

	AllocatedBuffer mIndexBuffer;
	uint32_t mNumIndices{0};
	uint32_t mMainFirstIndex{0};
};

class Node
{
public:
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

class MeshNode : public Node
{
public:
	Mesh* mMesh;

	void generateRenderItems(Renderer* renderer, GLTFModel* model) override;
};
