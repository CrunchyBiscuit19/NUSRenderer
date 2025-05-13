#pragma once

#include <resource_manager.h>
#include <materials.h>

#include <glm/fwd.hpp>

#include <vector>
#include <filesystem>

class Renderer;

struct SSBOAddresses {
    vk::DeviceAddress vertexBuffer;
    vk::DeviceAddress sceneBuffer;
    vk::DeviceAddress materialBuffer;
    vk::DeviceAddress instanceBuffer;
};

struct SceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct TransformationData {
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::f32 scale;
};

struct InstanceData {
    glm::mat4 transformation;
};

class SceneManager {
private:
    Renderer* mRenderer;
    AllocatedBuffer mSceneStagingBuffer;

public:
    SceneManager(Renderer* renderer);

    void init();

	void loadModels(const std::vector<std::filesystem::path>& files);
    void deleteModels();
    void deleteInstances();

    void generateRenderItems();
	void updateSceneBuffer();   
	void updateMaterialTextureArray(std::shared_ptr<PbrMaterial> material);

    void cleanup();
};

