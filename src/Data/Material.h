#pragma once

#include <Utils/Pipeline.h>

#include <fastgltf/types.hpp>

class Renderer;
class AllocatedImage;

struct MaterialTexture {
    AllocatedImage* image;
    vk::Sampler sampler;

    MaterialTexture() : image(nullptr), sampler(nullptr) {}

    MaterialTexture(AllocatedImage* image, vk::Sampler sampler) : image(image), sampler(sampler) {}
};

struct MaterialConstants {
    glm::vec4 baseFactor;
    glm::vec4 emissiveFactor;
    glm::vec2 metallicRoughnessFactor;
    float normalScale;
    float occlusionStrength;
};

struct MaterialResources {
    MaterialTexture base;
    MaterialTexture metallicRoughness;
    MaterialTexture normal;
    MaterialTexture occlusion;
    MaterialTexture emissive;
};

struct PbrData {
    bool doubleSided;
    fastgltf::AlphaMode alphaMode;
    MaterialConstants constants;
    MaterialResources resources;
};

class PbrMaterial {
    Renderer* mRenderer;

   public:
    std::string mName;
    u32 mRelativeMaterialIndex;
    PipelineBundle* mPipelineBundle;
    PbrData mPbrData;
    vk::Buffer mConstantsBuffer;
    u32 mConstantsBufferOffset;

    static std::unordered_map<PipelineOptions, PipelineBundle> mPipelinesCache;
    static vk::raii::PipelineLayout mOpaquePipelineLayout;
    static vk::raii::PipelineLayout mTransparentPipelineLayout;

    static vk::raii::DescriptorSetLayout mDrawImageDescriptorSetLayout;
    static vk::raii::DescriptorSet mDrawImageDescriptorSet;

    PbrMaterial(Renderer* renderer);

    static void init(Renderer* renderer);

    void getMaterialPipeline();
    void createMaterialPipeline(PipelineOptions materialPipelineOptions) const;
    
    static void resizeDrawImageView(Renderer* renderer);

    static void cleanup(Renderer* renderer);

   private:
    static void initMaterialPipelineLayout(Renderer* renderer);
    static void initMaterialDescriptor(Renderer* renderer);
};
