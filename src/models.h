#pragma once

#include <meshes.h>
#include <scene_manager.h>
#include <vk_descriptors.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <uuid.h>

#include <filesystem>
#include <unordered_map>

class Renderer;

struct TransformData {
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::f32 scale;
};

struct InstanceData {
    glm::mat4 transformMatrix;
};

class GLTFInstance {
public:
    GLTFModel* mModel;
    int mId;
    bool mDeleteSignal;
    TransformData mTransformComponents;

    GLTFInstance(GLTFModel* model);
};

class GLTFModel {
    struct DeleteInfo {
        bool deleteSignal;
        uint64_t deleteFrame;
    };

private:
    Renderer* mRenderer;

public:
    std::string mName;
    int mLatestId { 0 };
    DeleteInfo mDeleteInfo { false, 0 };


    fastgltf::Asset mAsset;
    std::vector<std::shared_ptr<Node>> mTopNodes;
    std::vector<std::shared_ptr<Node>> mNodes;
    std::vector<std::shared_ptr<Mesh>> mMeshes;
    std::vector<vk::raii::Sampler> mSamplers;
	std::vector<AllocatedImage> mImages;

    DescriptorAllocatorGrowable mDescriptorAllocator;

    std::vector<std::shared_ptr<PbrMaterial>> mMaterials;
    AllocatedBuffer mMaterialConstantsBuffer;
    
    std::vector<GLTFInstance> mInstances;
    AllocatedBuffer mInstancesBuffer;
    static vk::raii::DescriptorSetLayout mInstancesDescriptorSetLayout;
    vk::raii::DescriptorSet mInstancesDescriptorSet;

private:
    vk::Filter extractFilter(fastgltf::Filter filter);
    vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
    AllocatedImage loadImage(fastgltf::Image& image);

    void initDescriptors();
    void initBuffers();
    void loadSamplers();
    void loadImages();
    void loadMaterials();
    void loadMeshes();
    void loadNodes();

public:
    GLTFModel(Renderer* renderer, std::filesystem::path modelPath);
    ~GLTFModel();

    GLTFModel(GLTFModel&& other) noexcept;
    GLTFModel& operator=(GLTFModel&& other) noexcept;

    static void createInstanceDescriptorSetLayout(Renderer* renderer);

    void load();

    void generateRenderItems();
    
    void createInstance();
    void updateInstances();

    void markDelete();
};