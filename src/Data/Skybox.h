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

	vk::raii::Pipeline mSkyboxPipeline;
	vk::raii::PipelineLayout mSkyboxPipelineLayout;

	vk::raii::DescriptorSet mSkyboxDescriptorSet;
	vk::raii::DescriptorSetLayout mSkyboxDescriptorSetLayout;

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