#pragma once

#include <Renderer/RendererResources.h>

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
	bool mActive{ true };

	PipelineBundle mSkyboxPipelineBundle;
	vk::raii::PipelineLayout mSkyboxPipelineLayout;

	vk::raii::DescriptorSet mSkyboxDescriptorSet;
	vk::raii::DescriptorSetLayout mSkyboxDescriptorSetLayout;

	SkyBoxPushConstants mSkyboxPushConstants;

	Skybox(Renderer* renderer);

	void init(std::optional<std::filesystem::path> skyboxImageDir = std::nullopt);
	void initSkyboxDescriptor();
	void initSkyboxPipelineLayout();
	void initSkyboxPipeline();
	void initSkyboxBuffer();

	void setSkyboxBindings();
	void loadSkyboxImage(std::filesystem::path skyboxImageDir);
	void updateSkyboxImage(std::filesystem::path skyboxImageDir);

	void cleanup();
};