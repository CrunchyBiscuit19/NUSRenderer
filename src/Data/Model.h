#pragma once

#include <Data/Camera.h>
#include <Data/Instance.h>
#include <Data/Mesh.h>
#include <Utils/Descriptor.h>
#include <Utils/Types.h>

#include <fastgltf/parser.hpp>
#include <filesystem>

class Renderer;

class GLTFModel {
    Renderer* mRenderer;

   public:
    std::string mName;
    u32 mId{0};
    bool mToDelete{false};
    bool mReloadInstances{true};

    DescriptorAllocatorGrowable mModelDescriptorAllocator;

    fastgltf::Asset mAsset;
    std::vector<Mesh> mMeshes;
    std::vector<vk::SamplerCreateInfo> mSamplerCreateInfos;
    std::vector<AllocatedImage> mImages;

    std::vector<PbrMaterial> mMaterials;
    AllocatedBuffer mMaterialConstantsBuffer;
    u32 mMainFirstMaterial{0};

    std::vector<std::shared_ptr<Node>> mTopNodes;
    std::vector<std::shared_ptr<Node>> mNodes;
    AllocatedBuffer mNodeTransformsBuffer;
    u32 mMainFirstNodeTransform{0};

    std::vector<GLTFInstance> mInstances;
    AllocatedBuffer mInstancesBuffer;
    u32 mMainFirstInstance{0};

    AllocatedBuffer mBoundsBuffer;
    u32 mMainFirstBounds{0};

   private:
    vk::Filter extractFilter(fastgltf::Filter filter);
    vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
    vk::SamplerAddressMode extractAddressMode(fastgltf::Wrap wrap);

    AllocatedImage loadImage(fastgltf::Image& image);

    void assignBase(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material);
    void assignMetallicRoughness(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material);
    void assignEmissive(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material);
    void assignNormal(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material);
    void assignOcclusion(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material);

    void initBuffers();
    void loadSamplerCreateInfos();
    void loadImages();
    void loadMaterials();
    void loadMeshes();
    void loadNodes();
    void loadBoundsBuffer();

    void loadMeshBuffers(Mesh& mesh, std::span<u32> srcIndexVector, std::span<Vertex> srcVertexVector);
    void loadMaterialsConstantsBuffer(std::span<MaterialConstants> materialConstantsVector);
    void loadNodeTransformsBuffer(std::span<std::shared_ptr<Node>> nodesVector);

   public:
    GLTFModel(Renderer* renderer, const std::filesystem::path& modelPath);

    void load();

    Renderer* getRenderer() const;

    void generateRenderItemsInstances();

    void createInstance(InstanceData initialTransform = InstanceData());
    void createInstanceAtCamera(Camera& camera);
    void reloadInstances();

    void markDelete();
};
