#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

struct PickerPushConstants {
	vk::DeviceAddress vertexBuffer;
};

class Picker {
private:
	Renderer* mRenderer;

	AllocatedImage mPickerImage;

public:
	PipelineBundle mPickerPipeline;

	vk::raii::DescriptorSetLayout mPickerDescriptorSetLayout;
	vk::raii::DescriptorSet mPickerDescriptorSet;

	PickerPushConstants mPickerPushConstants;

	Picker(Renderer* renderer);

	void initSkyboxDescriptor();
	void initSkyboxPipeline();
	void init();

	void cleanup();
};