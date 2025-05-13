#pragma once

#include <vk_pipelines.h>

#include <fastgltf/types.hpp>

class Renderer;
class AllocatedImage;   

struct MaterialImage { // One AllocatedImage images can be shared across multiple MaterialImage images
    AllocatedImage* image;
    vk::Sampler sampler;

	MaterialImage() :
		image(nullptr),
		sampler(nullptr)
    {}   

	MaterialImage(AllocatedImage* image, vk::Sampler sampler) :
		image(image),
		sampler(sampler)
	{}
};

struct MaterialConstants {
    glm::vec4 baseFactor;
    glm::vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    glm::vec2 padding;
};

struct MaterialResources {
    MaterialImage base;
    MaterialImage metallicRoughness;
    MaterialImage normal;
    MaterialImage occlusion;
    MaterialImage emissive;
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
    PipelineBundle* mPipeline;
    PbrData mPbrData;
    vk::Buffer buffer;
    uint32_t bufferOffset;

    PbrMaterial(Renderer* renderer);

    void getMaterialPipeline();
};