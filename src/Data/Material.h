#pragma once

#include <Utils/Pipeline.h>
#include <Utils/Descriptor.h>

#include <fastgltf/types.hpp>

class Renderer;
class AllocatedImage;

struct MaterialImage { // One AllocatedImage images can be shared across multiple MaterialImage images
	AllocatedImage* image;
	vk::Sampler sampler;

	MaterialImage() :
		image(nullptr),
		sampler(nullptr)
	{
	}

	MaterialImage(AllocatedImage* image, vk::Sampler sampler) :
		image(image),
		sampler(sampler)
	{
	}
};

struct MaterialConstants {
	glm::vec4 baseFactor;
	glm::vec4 emissiveFactor;
	glm::vec4 metallicRoughnessFactor; // Combine for alignment
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
	DescriptorAllocatorGrowable* mModelDescriptorAllocator;

public:
	std::string mName;
	uint32_t mMaterialIndex;
	PipelineBundle* mPipeline;
	PbrData mPbrData;
	vk::Buffer mConstantsBuffer;
	uint32_t mConstantsBufferOffset;
	vk::raii::DescriptorSet mResourcesDescriptorSet;
	static vk::raii::DescriptorSetLayout mResourcesDescriptorSetLayout;

	PbrMaterial(Renderer* renderer, DescriptorAllocatorGrowable* descriptorAllocator);

	static void createResourcesDescriptorSetLayout(Renderer* renderer);

	void getMaterialPipeline();
	void writeMaterialResources();
};