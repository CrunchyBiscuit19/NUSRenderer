#pragma once

#include <Data/Mesh.h>
#include <Data/Skybox.h>

#include <glm/fwd.hpp>

#include <vector>
#include <map>
#include <filesystem>

class Renderer;

struct PushConstants {
	vk::DeviceAddress vertexBuffer;
	vk::DeviceAddress instanceBuffer;
	vk::DeviceAddress materialBuffer;
	uint32_t materialIndex;
	uint32_t _pad;
	glm::mat4 worldMatrix;
};

struct SceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

class SceneResources {
	Renderer* mRenderer;

public:
	SceneData mSceneData;
	AllocatedBuffer mSceneBuffer;
	vk::raii::DescriptorSet mSceneDescriptorSet;
	vk::raii::DescriptorSetLayout mSceneDescriptorSetLayout;

	SceneResources(Renderer* renderer);

	void initSceneResourcesData();
	void initSceneResourcesBuffer();
	void initSceneResourcesDescriptor();
	void init();

	void updateResources();

	void cleanup();
};

class RendererScene {
private:
	Renderer* mRenderer;

public:
	std::vector<RenderItem> mRenderItems;

	std::unordered_map<std::string, GLTFModel> mModels;

	Skybox mSkybox;
	bool mSkyboxActive{ true };

	PushConstants mPushConstants;
	SceneResources mSceneResources;

	AllocatedBuffer mVertexBuffer;

	RendererScene(Renderer* renderer);

	void init();

	void loadModels(const std::vector<std::filesystem::path>& files);
	void deleteModels();
	void deleteInstances();

	void reloadGlobalVertexBuffer();

	void generateRenderItems();
	void updateScene();

	void cleanup();
};

