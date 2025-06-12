#pragma once

#include <Utils/Pipeline.h>
#include <Utils/Descriptor.h>

#include <fastgltf/types.hpp>

class Renderer;
class AllocatedImage;

struct MaterialTexture {
	AllocatedImage* image;
	vk::Sampler sampler;

	MaterialTexture() :
		image(nullptr),
		sampler(nullptr)
	{
	}

	MaterialTexture(AllocatedImage* image, vk::Sampler sampler) :
		image(image),
		sampler(sampler)
	{
	}
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
	uint32_t mRelativeMaterialIndex;
	PipelineBundle* mPipeline;
	PbrData mPbrData;
	vk::Buffer mConstantsBuffer;
	uint32_t mConstantsBufferOffset;

	PbrMaterial(Renderer* renderer);

	void getMaterialPipeline();
};