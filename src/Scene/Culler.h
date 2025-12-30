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
    vk::DeviceAddress boundsBuffer;
    vk::DeviceAddress frustumBuffer;
    vk::DeviceAddress nodeTransformsBuffer;
    vk::DeviceAddress instancesBuffer;
    vk::DeviceAddress visibleRenderInstancesInstanceIndexBuffer;
    u32 renderInstancesLimit;
};

struct CullerCompactPushConstants {
    vk::DeviceAddress preCullRenderItemsBuffer;
    vk::DeviceAddress postCullRenderItemsBuffer;
    vk::DeviceAddress postCullRenderItemsCountBuffer;
    u32 preCullRenderItemsLimit;
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

    Culler(Renderer* renderer);

    void init();
    void initResetPipeline();
    void initCullPipeline();
    void initCompactPipeline();

    void cleanup();
};
