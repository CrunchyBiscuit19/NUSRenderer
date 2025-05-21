#pragma once

//#include <resource_manager.h>
#include <meshes.h>
#include <skybox.h>

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
    DescriptorAllocatorGrowable* mDescriptorAllocator;

public:
    SceneData mSceneData;
    AllocatedBuffer mSceneBuffer;
    vk::raii::DescriptorSet mSceneDescriptorSet;
    vk::raii::DescriptorSetLayout mSceneDescriptorSetLayout;

    SceneResources(Renderer* renderer);

    void init();

    void cleanup();
};

class SceneManager {
private:
    Renderer* mRenderer;

public:
    std::vector<RenderItem> mRenderItems;

    std::unordered_map<std::string, GLTFModel> mModels;

    PushConstants mPushConstants;
    SceneResources mSceneResources;
    Skybox mSkybox;

    SceneManager(Renderer* renderer);

    void init();

	void loadModels(const std::vector<std::filesystem::path>& files);
    void deleteModels();
    void deleteInstances();

    void generateRenderItems();
	void updateScene();   

    void cleanup();
};

