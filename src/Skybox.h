#pragma once

#include <RendererResources.h>

#include <filesystem>

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

	void loadSkyboxImage(std::filesystem::path skyboxImageDir);
	void initSkyboxDescriptor();
	void initSkyboxPipeline();
	void initSkyboxBuffer();

	void setSkyboxBindings();
	void updateSkyboxImage(std::filesystem::path skyboxImageDir);

	void init(std::filesystem::path skyboxImageDir);

	void cleanup();
};