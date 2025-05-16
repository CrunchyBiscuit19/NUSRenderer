#pragma once

#include <resource_manager.h>
#include <materials.h>

#include <glm/fwd.hpp>

#include <vector>
#include <filesystem>

class Renderer;

struct PushConstants {
    vk::DeviceAddress vertexBuffer;
    vk::DeviceAddress materialBuffer;
    glm::mat4 worldMatrix;
    uint32_t materialIndex;
};

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

class SceneEncapsulation {
    Renderer* mRenderer;
    DescriptorAllocatorGrowable* mDescriptorAllocator;

public:
    SceneData mSceneData;
    AllocatedBuffer mSceneBuffer;
    vk::raii::DescriptorSet mSceneDescriptorSet;
    static vk::raii::DescriptorSetLayout mSceneDescriptorSetLayout;

    SceneEncapsulation(Renderer* renderer);

    void init();

    void cleanup();
};

class SceneManager {
private:
    Renderer* mRenderer;

public:
    SceneManager(Renderer* renderer);

    void init();

	void loadModels(const std::vector<std::filesystem::path>& files);
    void deleteModels();
    void deleteInstances();

    void generateRenderItems();
	void updateScene();   

    void cleanup();
};

