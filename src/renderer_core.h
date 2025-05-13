#pragma once

#include <vulkan/vulkan.hpp>

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
    VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    void* pUserData
);

class Renderer;

class RendererCore {
private:
    Renderer* mRenderer;

public:
    RendererCore(Renderer* renderer);

    void init();
    void cleanup();
};