#pragma once

#include <Data/Mesh.h>
#include <Data/Skybox.h>

#include <glm/fwd.hpp>

#include <vector>
#include <map>
#include <filesystem>

class Renderer;

struct PerspectiveData { // Will move global lights away in the future
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

class Perspective {
	Renderer* mRenderer;

public:
	PerspectiveData mPerspectiveData;
	AllocatedBuffer mPerspectiveBuffer;
	vk::raii::DescriptorSet mPerspectiveDescriptorSet;
	vk::raii::DescriptorSetLayout mPerspectiveDescriptorSetLayout;

	Perspective(Renderer* renderer);

	void init();
	void initData();
	void initBuffer();
	void initDescriptor();

	void update();

	void cleanup();
};

struct ScenePushConstants {
	vk::DeviceAddress vertexBuffer;
	vk::DeviceAddress materialConstantsBuffer;
	vk::DeviceAddress nodeTransformsBuffer;
	vk::DeviceAddress instancesBuffer;
	vk::DeviceAddress visibleRenderItemsBuffer;
};

struct CullPushConstants {
	vk::DeviceAddress renderItemsBuffer;
	vk::DeviceAddress visibleRenderItemsBuffer;
	// vk::DeviceAddress boundsBuffer;
	// Frustum as a GPU_ONLY uniform buffer, passed as DS? (vec4 planes[6]; vec4 corners[8];) -> 224 bytes
	vk::DeviceAddress countBuffer;
};

struct MainBuffer : AllocatedBuffer {
	vk::DeviceAddress address{};

	MainBuffer() = default;

	MainBuffer(AllocatedBuffer&& other) noexcept
		: AllocatedBuffer(std::move(other)) 
	{}
	MainBuffer& operator=(AllocatedBuffer&& other) noexcept {
		static_cast<AllocatedBuffer&>(*this) = std::move(other);
		return *this;
	}

	MainBuffer(const MainBuffer&) = delete;
	MainBuffer& operator=(const MainBuffer&) = delete;
};

struct Batch {
	PipelineBundle* pipeline;
	std::vector<RenderItem> renderItems;
	MainBuffer renderItemsBuffer;
	MainBuffer visibleRenderItemsBuffer;
	MainBuffer countBuffer;

	~Batch() {
		countBuffer.cleanup();
		visibleRenderItemsBuffer.cleanup();
		renderItemsBuffer.cleanup();
		renderItems.clear();
	}
};

struct Flags {
	bool mModelAddedFlag;
	bool mModelDestroyedFlag;
	bool mInstanceAddedFlag;
	bool mInstanceDestroyedFlag;
	bool mInstanceUpdatedFlag;
};

class SceneManager {
	Renderer* mRenderer;

public:
	Flags mFlags;

	std::unordered_map<std::string, GLTFModel> mModels;

	std::unordered_map<int, Batch> mBatches;
	ScenePushConstants mScenePushConstants;
	CullPushConstants mCullPushConstants;

	MainBuffer mMainVertexBuffer;
	AllocatedBuffer mMainIndexBuffer;

	MainBuffer mMainMaterialConstantsBuffer;
	vk::raii::DescriptorSet mMainMaterialResourcesDescriptorSet;
	vk::raii::DescriptorSetLayout mMainMaterialResourcesDescriptorSetLayout;

	MainBuffer mMainNodeTransformsBuffer;
	MainBuffer mMainInstancesBuffer;

	SceneManager(Renderer* renderer);

	void init();
	void initBuffers();
	void initDescriptor();
	void initPushConstants();

	void loadModels(const std::vector<std::filesystem::path>& files);
	void deleteModels();
	void deleteInstances();

	void regenerateRenderItems();

	void realignVertexIndexOffset();
	void realignMaterialOffset();
	void realignNodeTransformsOffset();
	void realignInstancesOffset();

	void realignOffsets();

	void reloadMainVertexBuffer();
	void reloadMainIndexBuffer();
	void reloadMainMaterialConstantsBuffer();
	void reloadMainNodeTransformsBuffer();
	void reloadMainInstancesBuffer();
	void reloadMainMaterialResourcesArray();

	void reloadMainBuffers();

	void resetFlags();

	void cleanup();
};

class RendererScene {
private:
	Renderer* mRenderer;

public:
	int mLatestInstanceId{ 0 };
	int mLatestMeshId{ 0 };
	int mLatestModelId{ 0 };

	Perspective mPerspective;
	Skybox mSkybox;
	SceneManager mSceneManager;

	RendererScene(Renderer* renderer);

	void init();

	void cleanup();
};

