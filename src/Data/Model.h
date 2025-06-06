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
	int mId{ 0 };
	std::optional<uint64_t> mDeleteSignal{ std::nullopt };
	bool mReloadInstancesBuffer{ true };

	DescriptorAllocatorGrowable mModelDescriptorAllocator;

	fastgltf::Asset mAsset;
	std::vector<Mesh> mMeshes;
	std::vector<vk::raii::Sampler> mSamplers;
	std::vector<AllocatedImage> mImages;

	std::vector<PbrMaterial> mMaterials;
	AllocatedBuffer mMaterialConstantsBuffer;
	uint32_t mMainFirstMaterial;

	std::vector<std::shared_ptr<Node>> mTopNodes;
	std::vector<std::shared_ptr<Node>> mNodes;
	AllocatedBuffer mNodeTransformsBuffer;
	uint32_t mMainFirstNodeTransform;

	std::vector<GLTFInstance> mInstances;
	AllocatedBuffer mInstancesBuffer;

private:
	vk::Filter extractFilter(fastgltf::Filter filter);
	vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter);
	AllocatedImage loadImage(fastgltf::Image& image);
	void assignTexture(MaterialTexture& materialImage, fastgltf::Optional<fastgltf::TextureInfo>& textureInfo);
	void assignTexture(MaterialTexture& materialImage, fastgltf::Optional<fastgltf::NormalTextureInfo>& textureInfo);
	void assignTexture(MaterialTexture& materialImage, fastgltf::Optional<fastgltf::OcclusionTextureInfo>& textureInfo);

	void initBuffers();
	void loadSamplers();
	void loadImages();
	void loadMaterials();
	void loadMeshes();
	void loadNodes();

	void loadMeshBuffers(Mesh* mesh, std::span<uint32_t> srcIndexVector, std::span<Vertex> srcVertexVector);
	void loadMaterialsConstantsBuffer(std::span<MaterialConstants> materialConstantsVector);
	void loadNodeTransformsBuffer(std::span<std::shared_ptr<Node>> nodesVector);
	void loadInstancesBuffer(std::span<InstanceData> instanceDataVector);

public:
	GLTFModel(Renderer* renderer, std::filesystem::path modelPath);

	void load();

	void generateRenderItems();

	void createInstance();
	void updateInstances();

	void markDelete();
};