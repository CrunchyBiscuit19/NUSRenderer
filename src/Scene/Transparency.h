#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class Transparency {
	Renderer* mRenderer;

public:	
	AllocatedImage mAccumImage;
	AllocatedImage mRevealageImage;

	PipelineBundle mPipelineBundle;
    vk::raii::PipelineLayout mPipelineLayout;

	vk::raii::DescriptorSet mDescriptorSet;
    vk::raii::DescriptorSetLayout mDescriptorSetLayout;

	Transparency(Renderer* renderer);
	
	void init();
	void initImages();
    void initDescriptors();
    void writeDescriptors();
    void initPipeline();
    void resizeImages();

	void cleanup();
};
;