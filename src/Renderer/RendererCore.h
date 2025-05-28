#pragma once

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_mem_alloc.h>

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

	vk::raii::Context mContext;
	vk::raii::DebugUtilsMessengerEXT mDebugMessenger;

	vk::PhysicalDeviceProperties mChosenGPUProperties;


public:
	vk::raii::Instance mInstance;

	vk::raii::Device mDevice;
	vk::raii::PhysicalDevice mChosenGPU;

	vk::raii::SurfaceKHR mSurface;

	SDL_Window* mWindow{ nullptr };
	vk::Extent2D mWindowExtent{ 1700, 900 };
	bool mWindowFullScreen{ false };

	vk::raii::Queue mComputeQueue;
	uint32_t mComputeQueueFamily;
	vk::raii::Queue mGraphicsQueue;
	uint32_t mGraphicsQueueFamily;

	VmaAllocator mVmaAllocator;

	RendererCore(Renderer* renderer);

	void init();

	void processSDLEvent(const SDL_Event& e);

	void cleanup();
};