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

class GLTFInstance {
public:
    int id;
    bool toDelete;
    InstanceData data;

    GLTFInstance();
};

class GLTFModel {
private:
    Renderer* mRenderer;

public:
    std::string mName;
    bool mToDelete { false };
    bool mLoaded { false };
    int mLatestId{ 0 };

    fastgltf::Asset mAsset;
    std::vector<std::shared_ptr<Node>> mTopNodes;
    std::vector<std::shared_ptr<Node>> mNodes;
    std::vector<std::shared_ptr<Mesh>> mMeshes;
    std::vector<vk::raii::Sampler> mSamplers;
	std::vector<AllocatedImage> mImages;
    std::vector<std::shared_ptr<PbrMaterial>> mMaterials;
    std::vector<GLTFInstance> mInstances;

    DescriptorAllocatorGrowable mDescriptorAllocator;
    AllocatedBuffer mMaterialConstantsBuffer;
    AllocatedBuffer mInstanceBuffer;

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

    void updateInstances();

public:
    GLTFModel(Renderer* renderer, std::filesystem::path modelPath);

    void load();
    void generateRenderItems();
};