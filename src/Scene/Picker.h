#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

struct PickerData
{
	glm::vec2 uvData;
	glm::uvec2 readData;
};

class Picker
{
	Renderer* mRenderer;

public:
	AllocatedBuffer mBuffer;

	AllocatedImage mImage;
	AllocatedImage mDepthImage;

	vk::raii::DescriptorSet mDescriptorSet;
	vk::raii::DescriptorSetLayout mDescriptorSetLayout;

	PipelineBundle mDrawPipelineBundle;
	vk::raii::PipelineLayout mDrawPipelineLayout;
	PipelineBundle mPickPipelineBundle;
	vk::raii::PipelineLayout mPickPipelineLayout;

	Picker(Renderer* renderer);

	void init();
	void initBuffer();
	void initImage();
	void initDescriptor();
	void initDrawPipeline();
	void initPickPipeline();
	void initCallback();

	void cleanup();
};
