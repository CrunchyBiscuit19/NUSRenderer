#pragma once

#include <Renderer/RendererResources.h>

#include <imgui.h>    
#include <ImGuizmo.h>

class Renderer;
class GLTFInstance;

struct PickerDrawPushConstants
{
	vk::DeviceAddress vertexBuffer;
	vk::DeviceAddress nodeTransformsBuffer;
	vk::DeviceAddress instancesBuffer;
	vk::DeviceAddress visibleRenderItemsBuffer;
};

struct PickerPickPushConstants
{
	vk::DeviceAddress pickerBuffer;
};

struct PickerData
{
	glm::ivec2 coords;
	glm::uvec2 read;
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

	PickerDrawPushConstants mDrawPushConstants;
	PickerPickPushConstants mPickPushConstants;

	ImGuizmo::OPERATION mImguizmoOperation;
	GLTFInstance* mClickedInstance;

	Picker(Renderer* renderer);

	void init();
	void initBuffer();
	void initImage();
	void initDescriptor();
	void initDrawPipeline();
	void initPickPipeline();
	void initDrawPushConstants();
	void initPickPushConstants();

	void changeImguizmoOperation();

	void imguizmoFrame() const;

	void cleanup();
};
