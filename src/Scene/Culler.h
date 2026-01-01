#pragma once

#include <Utils/Pipeline.h>

class Renderer;

struct Plane {
    glm::vec3 normal;
    float d;

    Plane() : normal(glm::vec3(0.f)), d(0.f) {}
    Plane(glm::vec3 n, glm::vec3 p) : normal(glm::normalize(n)), d(glm::dot(glm::normalize(n), p)) {}
};

struct CullerResetPushConstants {
    vk::DeviceAddress preCullRenderItemsBuffer;
    u32 preCullRenderItemsLimit;
};

struct CullerCullPushConstants {
    vk::DeviceAddress preCullRenderItemsBuffer;
    vk::DeviceAddress renderInstancesBuffer;
    vk::DeviceAddress renderInstancesCountBuffer;
    vk::DeviceAddress frustumBuffer;
    vk::DeviceAddress mainBoundsBuffer;
    vk::DeviceAddress mainNodeTransformsBuffer;
    vk::DeviceAddress mainInstancesBuffer;
    vk::DeviceAddress mainVisibleRenderInstancesInstanceIndexBuffer;
    u32 renderInstancesLimit;
};

struct CullerCompactPushConstants {
    vk::DeviceAddress preCullRenderItemsBuffer;
    vk::DeviceAddress postCullRenderItemsBuffer;
    vk::DeviceAddress postCullRenderItemsCountBuffer;
    u32 preCullRenderItemsLimit;
};

struct CullerDepthPyramidPushConstants {
    glm::uvec2 levelExtent;
    u32 level;
};

class Culler {
    Renderer* mRenderer;

   public:
    PipelineBundle mResetPipelineBundle;
    vk::raii::PipelineLayout mResetPipelineLayout;
    CullerResetPushConstants mResetPushConstants;

    PipelineBundle mCullPipelineBundle;
    vk::raii::PipelineLayout mCullPipelineLayout;
    CullerCullPushConstants mCullPushConstants;

    PipelineBundle mCompactPipelineBundle;
    vk::raii::PipelineLayout mCompactPipelineLayout;
    CullerCompactPushConstants mCompactPushConstants;

    PipelineBundle mDepthPyramidPipelineBundle;
    vk::raii::PipelineLayout mDepthPyramidPipelineLayout;
    vk::raii::DescriptorSet mDepthPyramidDescriptorSet;
    vk::raii::DescriptorSetLayout mDepthPyramidDescriptorSetLayout;
    AllocatedImage mDepthPyramidImage;
    vk::Sampler mDepthPyramidSampler;
    std::vector<vk::raii::ImageView> mDepthPyramidMipViews;
    u32 mDepthPyramidLevels{0};
    vk::Extent3D mDepthPyramidExtent;
    CullerDepthPyramidPushConstants mDepthPyramidPushConstants;

    bool mFreezeCulling{false};

    Culler(Renderer* renderer);

    void init();
    void initDepthPyramidDescriptor();
    void initDepthPyramidImage();
    void initDepthPyramidPipeline();
    void initResetPipeline();
    void initCullPipeline();
    void initCompactPipeline();

    void reconstructDepthPyramid();

    void bindDepthPyramidDescriptor(u32 depthPyramidLevel);

    void cleanup();
};
