#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class Picker
{
	Renderer* mRenderer;

public:
	AllocatedImage mObjectImage;
	AllocatedImage mDepthImage;

	PipelineBundle mPipelineBundle;
	vk::raii::PipelineLayout mPipelineLayout;

	std::pair<int,int> mMouseClickLocation;

	Picker(Renderer* renderer);

	void init();
	void initImage();
	void initPipelineLayout();
	void initPipeline();
	void initCallback();

	void cleanup();
};
