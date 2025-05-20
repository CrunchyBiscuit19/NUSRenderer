#pragma once

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>

#ifdef RELEASE
constexpr bool USE_VALIDATION_LAYERS = false;
#else
constexpr bool USE_VALIDATION_LAYERS = true;
#endif

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
    vk::raii::Context mContext;
    vk::raii::Instance mInstance;
    vk::raii::DebugUtilsMessengerEXT mDebugMessenger;

    vk::raii::PhysicalDevice mChosenGPU;
    vk::raii::Device mDevice;
    vk::PhysicalDeviceProperties mChosenGPUProperties;

    vk::raii::SurfaceKHR mSurface;
    SDL_Window* mWindow{ nullptr };
    vk::Extent2D mWindowExtent{ 1700, 900 };

    vk::raii::Queue mComputeQueue;
    uint32_t mComputeQueueFamily;
    vk::raii::Queue mGraphicsQueue;
    uint32_t mGraphicsQueueFamily;

    VmaAllocator mVmaAllocator;

    RendererCore(Renderer* renderer);

    void init();
    void cleanup();
};