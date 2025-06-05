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

struct Bounds {
	glm::vec3 origin;
	float sphereRadius;
	glm::vec3 extents;
};

struct Primitive {
	uint32_t mRelativeIndexStart;
	uint32_t mIndexCount;
	uint32_t mRelativeVertexOffset;
	PbrMaterial* mMaterial;
	Bounds mBounds;
};

struct Mesh {
	std::string mName;
	int mId;
	std::vector<Primitive> mPrimitives;

	AllocatedBuffer mVertexBuffer;
	uint32_t mNumVertices;
	uint32_t mMainVertexOffset;
	
	AllocatedBuffer mIndexBuffer;
	uint32_t mNumIndices;
	uint32_t mMainIndexStart;
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

	/*uint32_t mFirstIndex;
	uint32_t mIndexCount;
	uint32_t mVertexOffset;*/

	glm::mat4 transform;

	vk::DeviceAddress vertexBufferAddress;
	vk::DeviceAddress instancesBufferAddress;
	vk::DeviceAddress materialConstantsBufferAddress;
};