#pragma once

#include <resource_manager.h>

#include <filesystem>
namespace fs = std::filesystem;

constexpr int NUMBER_OF_SKYBOX_VERTICES = 36;

class Renderer;

struct SkyBoxPushConstants {
	vk::DeviceAddress vertexBuffer;
};

class Skybox {
private:
	Renderer* mRenderer;

	AllocatedImage mSkyboxImage;

	std::vector<float> mSkyboxVertices;
	AllocatedBuffer mSkyboxVertexBuffer;

public:
	PipelineBundle mSkyboxPipeline;

	vk::raii::DescriptorSetLayout mSkyboxDescriptorSetLayout;
	vk::raii::DescriptorSet mSkyboxDescriptorSet;

	SkyBoxPushConstants mSkyboxPushConstants;

	Skybox(Renderer* renderer);

	void loadSkyboxImage(fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back);
	void initSkyboxDescriptor();
	void initSkyboxPipeline();
	void initSkyboxBuffer();

	void init(fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back);

	void cleanup();
};