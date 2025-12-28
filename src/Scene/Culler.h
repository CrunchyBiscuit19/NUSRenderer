#pragma once

#include <Utils/Pipeline.h>

class Renderer;

struct Plane {
    glm::vec3 normal;
    float d;
};

struct CullerCullPushConstants {
    vk::DeviceAddress renderItemsBuffer;
    vk::DeviceAddress renderInstancesBuffer;
    vk::DeviceAddress postCullRenderInstancesCountBuffer;
    vk::DeviceAddress boundsBuffer;
    vk::DeviceAddress frustumBuffer;
    vk::DeviceAddress nodeTransformsBuffer;
    vk::DeviceAddress preCullnstancesBuffer;
    vk::DeviceAddress postCullnstancesBuffer;
    vk::DeviceAddress perspectiveBuffer;
    u32 renderInstancesCount;
};

struct CullerResetPushConstants {
    vk::DeviceAddress renderItemsBuffer;
    u32 renderItemsCount;
};

struct CullerCompactPushConstants {
    vk::DeviceAddress preCullRenderItemsBuffer;
    vk::DeviceAddress postCullRenderItemsBuffer;
    u32 renderItemsCount;
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
