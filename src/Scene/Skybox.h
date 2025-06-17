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

	AllocatedImage mImage;

	std::vector<float> mVertices;
	AllocatedBuffer mVertexBuffer;

public:
	bool mActive{ true };

	PipelineBundle mPipelineBundle;
	vk::raii::PipelineLayout mPipelineLayout;

	vk::raii::DescriptorSet mDescriptorSet;
	vk::raii::DescriptorSetLayout mDescriptorSetLayout;

	SkyBoxPushConstants mPushConstants;

	Skybox(Renderer* renderer);

	void init(std::optional<std::filesystem::path> skyboxImageDir = std::nullopt);
	void initDescriptor();
	void initPipelineLayout();
	void initPipeline();
	void initBuffer();

	void setBindings();
	void loadImage(std::filesystem::path skyboxImageDir);
	void updateImage(std::filesystem::path skyboxImageDir);

	void cleanup();
};