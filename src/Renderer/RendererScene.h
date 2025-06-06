#pragma once

#include <Data/Mesh.h>
#include <Data/Skybox.h>

#include <glm/fwd.hpp>

#include <vector>
#include <map>
#include <filesystem>

class Renderer;

struct ScenePushConstants {
	vk::DeviceAddress vertexBuffer;
	vk::DeviceAddress instanceBuffer;
	vk::DeviceAddress materialBuffer;
	uint32_t materialIndex;
	uint32_t _pad;
	glm::mat4 worldMatrix;
};

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

class SceneManager {
	Renderer* mRenderer;

public:
	std::unordered_map<std::string, GLTFModel> mModels;

	std::vector<RenderItem> mRenderItems;
	ScenePushConstants mScenePushConstants;

	MainBuffer mMainVertexBuffer;
	AllocatedBuffer mMainIndexBuffer;

	MainBuffer mMainMaterialConstantsBuffer;
	vk::raii::DescriptorSet mMainMaterialResourcesDescriptorSet;
	vk::raii::DescriptorSetLayout mMainMaterialResourcesDescriptorSetLayout;

	MainBuffer mMainNodeTransformsBuffer;
	MainBuffer mMainInstancesBuffer;
	
	MainBuffer mDrawCommandsBuffer;
	MainBuffer mCountBuffer;

	SceneManager(Renderer* renderer);

	void init();
	void initBuffers();
	void initDescriptor();

	void loadModels(const std::vector<std::filesystem::path>& files);
	void deleteModels();
	void deleteInstances();

	void alignOffsets();
	void generateRenderItems();

	void reloadMainVertexBuffer();
	void reloadMainIndexBuffer();
	void reloadMainMaterialConstantsBuffer();
	void reloadMainNodeTransformsBuffer();
	void reloadMainInstancesBuffer();
	void reloadMainMaterialResourcesArray();

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

	void updateScene();

	void cleanup();
};

