#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class Picker {
private:
	Renderer* mRenderer;

public:
	AllocatedImage mPickerDrawImage;

	PipelineBundle mPickerPipelineBundle;
	vk::raii::PipelineLayout mPickerPipelineLayout;

	Picker(Renderer* renderer);

	void init();
	void initImage();
	void initPickerPipelineLayout();
	void initPickerPipeline();

	void cleanup();
};