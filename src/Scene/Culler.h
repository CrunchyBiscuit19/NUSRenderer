#pragma once

#include <Utils/Pipeline.h>

class Renderer;

struct CullPushConstants {
	vk::DeviceAddress renderItemsBuffer;
	vk::DeviceAddress visibleRenderItemsBuffer;
	// vk::DeviceAddress boundsBuffer;
	// Frustum as a GPU_ONLY uniform buffer, passed as DS? (vec4 planes[6]; vec4 corners[8];) -> 224 bytes
	vk::DeviceAddress countBuffer;
};

class Culler {
	Renderer* mRenderer;

public:
	PipelineBundle mPipelineBundle;
	vk::raii::PipelineLayout mPipelineLayout;
	CullPushConstants mPushConstants;

	Culler(Renderer* renderer);

	void init();
	void initPipelineLayout();
	void initPipeline();

	void cleanup();
};