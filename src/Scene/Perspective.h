#pragma once

#include <Renderer/RendererInfrastructure.h>

#include <glm/glm.hpp>

struct PerspectiveData {
    // Will move global lights away in the future
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 ambientColor;
    glm::vec4 sunlightDirection;  // w for sun power
    glm::vec3 sunlightColor;
};

class Perspective {
    Renderer* mRenderer;

   public:
    PerspectiveData mData;

    vk::raii::DescriptorSetLayout mDescriptorSetLayout;

    Perspective(Renderer* renderer);

    void init();
    void initData();
    void initDescriptor();

    void update();

    void cleanup();
};
