#pragma once

#include <Data/Mesh.h>
#include <Data/Instance.h>
#include <Utils/Descriptor.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <filesystem>
#include <unordered_map>

class Renderer;

class GLTFModel
{
	Renderer* mRenderer;

public:
	std::string mName;
	int mId{0};
	std::optional<uint64_t> mDeleteSignal{std::nullopt};
	bool mReloadLocalInstancesBuffer{true};

	DescriptorAllocatorGrowable mModelDescriptorAllocator;

	fastgltf::Asset mAsset;
	std::vector<Mesh> mMeshes;
	std::vector<vk::SamplerCreateInfo> mSamplerCreateInfos;
	std::vector<AllocatedImage> mImages;

	std::vector<PbrMaterial> mMaterials;
	AllocatedBuffer mMaterialConstantsBuffer;
	uint32_t mMainFirstMaterial{0};

	std::vector<std::shared_ptr<Node>> mTopNodes;
	std::vector<std::shared_ptr<Node>> mNodes;
	AllocatedBuffer mNodeTransformsBuffer;
	uint32_t mMainFirstNodeTransform{0};

	std::vector<GLTFInstance> mInstances;
	AllocatedBuffer mInstancesBuffer;
	uint32_t mMainFirstInstance{0};

private:
	vk::Filter extractFilter(fastgltf::Filter filter);
	vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
	vk::SamplerAddressMode extractAddressMode(fastgltf::Wrap wrap);

	AllocatedImage loadImage(fastgltf::Image& image);

	void assignBase(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material);
	void assignMetallicRoughness(MaterialConstants& constants, MaterialResources& resources,
	                             fastgltf::Material& material);
	void assignEmissive(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material);
	void assignNormal(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material);
	void assignOcclusion(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material);

	void initBuffers();
	void loadSamplerCreateInfos();
	void loadImages();
	void loadMaterials();
	void loadMeshes();
	void loadNodes();

	void loadMeshBuffers(Mesh& mesh, std::span<uint32_t> srcIndexVector, std::span<Vertex> srcVertexVector);
	void loadMaterialsConstantsBuffer(std::span<MaterialConstants> materialConstantsVector);
	void loadNodeTransformsBuffer(std::span<std::shared_ptr<Node>> nodesVector);
	void loadInstancesBuffer(std::span<InstanceData> instanceDataVector);

public:
	GLTFModel(Renderer* renderer, std::filesystem::path modelPath);

	void load();

	void generateRenderItems();

	void createInstance(TransformData initialTransform = TransformData());
	void updateInstances();

	void markDelete();
};
