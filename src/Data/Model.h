#pragma once

#include <Data/Mesh.h>
#include <Renderer/RendererScene.h>
#include <Data/Instance.h>
#include <Utils/Descriptor.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <filesystem>
#include <unordered_map>

class Renderer;

class GLTFModel {
private:
	Renderer* mRenderer;

public:
	std::string mName;
	int mLatestId{ 0 };
	std::optional<uint64_t> mDeleteSignal{ std::nullopt };
	bool mReloadInstancesBuffer{ true };

	fastgltf::Asset mAsset;
	std::vector<std::shared_ptr<Node>> mTopNodes;
	std::vector<std::shared_ptr<Node>> mNodes;
	std::vector<std::shared_ptr<Mesh>> mMeshes;
	std::vector<vk::raii::Sampler> mSamplers;
	std::vector<AllocatedImage> mImages;

	DescriptorAllocatorGrowable mModelDescriptorAllocator;

	std::vector<std::shared_ptr<PbrMaterial>> mMaterials;
	AllocatedBuffer mMaterialConstantsBuffer;

	std::vector<GLTFInstance> mInstances;
	AllocatedBuffer mInstancesBuffer;

private:
	vk::Filter extractFilter(fastgltf::Filter filter);
	vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
	AllocatedImage loadImage(fastgltf::Image& image);
	void assignTexture(MaterialImage& materialImage, fastgltf::Optional<fastgltf::TextureInfo>& textureInfo);
	void assignTexture(MaterialImage& materialImage, fastgltf::Optional<fastgltf::NormalTextureInfo>& textureInfo);
	void assignTexture(MaterialImage& materialImage, fastgltf::Optional<fastgltf::OcclusionTextureInfo>& textureInfo);

	void initDescriptors();
	void initBuffers();
	void loadSamplers();
	void loadImages();
	void loadMaterials();
	void loadMeshes();
	void loadNodes();

	void loadMaterialsConstantsBuffer(std::vector<MaterialConstants>& materialConstantsVector);
	void loadInstancesBuffer(std::vector<InstanceData>& instanceDataVector);
	void loadMeshBuffers(Mesh* mesh, std::vector<uint32_t>& srcIndexVector, std::vector<Vertex>& srcVertexVector);

public:
	GLTFModel(Renderer* renderer, std::filesystem::path modelPath);
	~GLTFModel();

	GLTFModel(GLTFModel&& other) noexcept;
	GLTFModel& operator=(GLTFModel&& other) noexcept;

	void load();

	void generateRenderItems();

	void createInstance();
	void updateInstances();

	void markDelete();
};