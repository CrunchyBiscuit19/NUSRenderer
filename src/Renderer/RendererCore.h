#pragma once

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan.hpp>

template <typename T>
struct VulkanResourceInfo;

#define DEFINE_VULKAN_RESOURCE_INFO(HppType, VkType, resourceTypeEnum)              \
template<> struct VulkanResourceInfo<HppType> {                                   \
	static constexpr vk::ObjectType resourceType = resourceTypeEnum; \
	static uint64_t getHandle(const HppType& resource) {                             \
		return reinterpret_cast<uint64_t>(static_cast<VkType>(resource));         \
	}                                                                        \
};

#define DEFINE_VULKAN_RAII_RESOURCE_INFO(RaiiType, VkType, resourceTypeEnum)          \
template<> struct VulkanResourceInfo<RaiiType> {                          \
	static constexpr vk::ObjectType resourceType = resourceTypeEnum; \
	static uint64_t getHandle(const RaiiType& resource) {                   \
		return reinterpret_cast<uint64_t>(static_cast<VkType>(*resource));        \
	}                                                                        \
};

DEFINE_VULKAN_RESOURCE_INFO(vk::Buffer, VkBuffer, vk::ObjectType::eBuffer)

DEFINE_VULKAN_RESOURCE_INFO(vk::Image, VkImage, vk::ObjectType::eImage)

DEFINE_VULKAN_RESOURCE_INFO(vk::ImageView, VkImageView, vk::ObjectType::eImageView)

DEFINE_VULKAN_RESOURCE_INFO(vk::ShaderModule, VkShaderModule, vk::ObjectType::eShaderModule)

DEFINE_VULKAN_RESOURCE_INFO(vk::Pipeline, VkPipeline, vk::ObjectType::ePipeline)

DEFINE_VULKAN_RESOURCE_INFO(vk::PipelineLayout, VkPipelineLayout, vk::ObjectType::ePipelineLayout)

DEFINE_VULKAN_RESOURCE_INFO(vk::DescriptorSetLayout, VkDescriptorSetLayout, vk::ObjectType::eDescriptorSetLayout)

DEFINE_VULKAN_RESOURCE_INFO(vk::DescriptorSet, VkDescriptorSet, vk::ObjectType::eDescriptorSet)

DEFINE_VULKAN_RESOURCE_INFO(vk::CommandPool, VkCommandPool, vk::ObjectType::eCommandPool)

DEFINE_VULKAN_RESOURCE_INFO(vk::CommandBuffer, VkCommandBuffer, vk::ObjectType::eCommandBuffer)

DEFINE_VULKAN_RESOURCE_INFO(vk::Fence, VkFence, vk::ObjectType::eFence)

DEFINE_VULKAN_RESOURCE_INFO(vk::Semaphore, VkSemaphore, vk::ObjectType::eSemaphore)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Buffer, VkBuffer, vk::ObjectType::eBuffer)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Image, VkImage, vk::ObjectType::eImage)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::ImageView, VkImageView, vk::ObjectType::eImageView)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::ShaderModule, VkShaderModule, vk::ObjectType::eShaderModule)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Pipeline, VkPipeline, vk::ObjectType::ePipeline)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::PipelineLayout, VkPipelineLayout, vk::ObjectType::ePipelineLayout)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::DescriptorSetLayout, VkDescriptorSetLayout,
                                 vk::ObjectType::eDescriptorSetLayout)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::DescriptorSet, VkDescriptorSet, vk::ObjectType::eDescriptorSet)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::CommandPool, VkCommandPool, vk::ObjectType::eCommandPool)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::CommandBuffer, VkCommandBuffer, vk::ObjectType::eCommandBuffer)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Fence, VkFence, vk::ObjectType::eFence)

DEFINE_VULKAN_RAII_RESOURCE_INFO(vk::raii::Semaphore, VkSemaphore, vk::ObjectType::eSemaphore)


VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
);

class Renderer;

class RendererCore
{
	Renderer* mRenderer;

	vk::raii::Context mContext;
	vk::raii::DebugUtilsMessengerEXT mDebugMessenger;

	vk::PhysicalDeviceProperties mChosenGPUProperties;

public:
	vk::raii::Instance mInstance;

	vk::raii::Device mDevice;
	vk::raii::PhysicalDevice mChosenGPU;

	vk::raii::SurfaceKHR mSurface;

	SDL_Window* mWindow{nullptr};
	vk::Extent2D mWindowExtent{1700, 900};
	bool mWindowFullScreen{false};

	vk::raii::Queue mComputeQueue;
	uint32_t mComputeQueueFamily;
	vk::raii::Queue mGraphicsQueue;
	uint32_t mGraphicsQueueFamily;

	VmaAllocator mVmaAllocator;

	RendererCore(Renderer* renderer);

	void init();

	template <typename T>
	void labelResourceDebug(T& resource, const char* name)
	{
		vk::DebugUtilsObjectNameInfoEXT nameInfo{
			VulkanResourceInfo<T>::resourceType,
			VulkanResourceInfo<T>::getHandle(resource),
			name
		};
		mDevice.setDebugUtilsObjectNameEXT(nameInfo);
	};

	void cleanup();
};
