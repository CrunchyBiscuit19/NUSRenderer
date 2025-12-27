#pragma once

#include <Utils/Pipeline.h>

class Renderer;

struct Plane {
    glm::vec3 normal;
    float d;
};

struct CullPushConstants {
    vk::DeviceAddress renderItemsBuffer;
    vk::DeviceAddress renderInstancesBuffer;
    vk::DeviceAddress postCullRenderInstancesCountBuffer;
    vk::DeviceAddress boundsBuffer;
    vk::DeviceAddress frustumBuffer;
    vk::DeviceAddress nodeTransformsBuffer;
    vk::DeviceAddress instancesBuffer;
    vk::DeviceAddress perspectiveBuffer;
    u32 renderInstancesCount;
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
