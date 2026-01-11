#pragma once

#include <ImGuizmo.h>
#include <Renderer/RendererResources.h>
#include <imgui.h>

class Renderer;
class GLTFInstance;

struct PickerDrawPushConstants {
    vk::DeviceAddress mainVertexBuffer;
    vk::DeviceAddress mainNodeTransformsBuffer;
    vk::DeviceAddress mainInstancesBuffer;
    vk::DeviceAddress mainVisibleRenderInstancesInstanceIndexBuffer;
    vk::DeviceAddress postCullRenderItemsBuffer;
};

struct PickerPickPushConstants {
    vk::DeviceAddress pickerBuffer;
};

struct PickerData {
    glm::ivec2 coords;
    glm::uvec2 read;
};

class Picker {
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
    void writeDescriptor();
    void initDrawPipeline();
    void initPickPipeline();
    void initDrawPushConstants();
    void initPickPushConstants();

    void resizePicker();

    void changeImguizmoOperation();

    void imguizmoFrame() const;

    void cleanup();
};
