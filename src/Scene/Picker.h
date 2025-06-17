#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class Picker {
private:
	Renderer* mRenderer;

public:
	AllocatedImage mObjectImage;
	AllocatedImage mDepthImage;

	PipelineBundle mPipelineBundle;
	vk::raii::PipelineLayout mPipelineLayout;

	Picker(Renderer* renderer);

	void init();
	void initImage();
	void initPipelineLayout();
	void initPipeline();

	void cleanup();
};