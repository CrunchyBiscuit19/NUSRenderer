#pragma once

#include <Utils/Pipeline.h>

class Renderer;

struct Plane {
	glm::vec3 normal;
	float d;
};

struct CullPushConstants {
	vk::DeviceAddress preCullRenderItemsBuffer;
	vk::DeviceAddress postCullRenderItemsBuffer;
	vk::DeviceAddress preCullRenderInstancesBuffer;
	vk::DeviceAddress postCullRenderInstancesCountBuffer;
	vk::DeviceAddress totalPostCullRenderInstancesCountBuffer;
	vk::DeviceAddress boundsBuffer;
	vk::DeviceAddress frustumBuffer;
	vk::DeviceAddress nodeTransformsBuffer;
	vk::DeviceAddress instancesBuffer;
	vk::DeviceAddress perspectiveBuffer; 
	uint32_t preCullRenderInstancesCount;
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
