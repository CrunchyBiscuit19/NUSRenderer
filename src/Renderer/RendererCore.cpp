#include <Renderer/Renderer.h>
#include <Renderer/RendererCore.h>
#include <Utils/Helper.h>

#include <fmt/core.h>
#include <Vkbootstrap.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>


VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
	VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
	VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
	void* pUserData
)
{
	std::string severity;
	switch (messageSeverity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		severity = "ERROR";
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		severity = "WARNING";
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		severity = "INFO: ";
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		severity = "VERBOSE: ";
		break;
	}

	std::string message;
	message += fmt::format("{} <{}> Frame {}\n\n", severity, std::string(pCallbackData->pMessageIdName), static_cast<Renderer*>(pUserData)->mRendererInfrastructure.mFrameNumber);
	message += fmt::format("{}\n\n", std::string(pCallbackData->pMessage));

	message += fmt::format("Queue Labels:\n");
	for (int i = 0; i < pCallbackData->queueLabelCount; i++)
		message += fmt::format("LabelName = <{}>\n", pCallbackData->pQueueLabels[i].pLabelName);
	message += fmt::format("CommandBuffer Labels:\n");
	for (int i = 0; i < pCallbackData->cmdBufLabelCount; i++)
		message += fmt::format("LabelName = <{}>\n", pCallbackData->pCmdBufLabels[i].pLabelName);

	message += fmt::format("\n");

	for (int i = 0; i < pCallbackData->objectCount; i++)
	{
		message += fmt::format("Resource {} -> [ ResourceType = {}, ResourceHandle = {}]\n", std::to_string(i), vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)), std::to_string(pCallbackData->pObjects[i].objectHandle));
		if (pCallbackData->pObjects[i].pObjectName)
			message += fmt::format("ResourceName   = <{}>\n", pCallbackData->pObjects[i].pObjectName);
	}

	fmt::println("{}", message);
	return false;
}

RendererCore::RendererCore(Renderer* renderer) :
	mRenderer(renderer),
	mContext(),
	mInstance(nullptr),
	mDebugMessenger(nullptr),
	mChosenGPU(nullptr),
	mDevice(nullptr),
	mSurface(nullptr),
	mComputeQueue(nullptr),
	mGraphicsQueue(nullptr)
{
}

void RendererCore::init()
{
	SDL_Init(SDL_INIT_VIDEO);
	mWindow = SDL_CreateWindow(
		"NUSRenderer",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		static_cast<int>(mWindowExtent.width),
		static_cast<int>(mWindowExtent.height),
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	mContext = vk::raii::Context();

	vkb::InstanceBuilder builder;
	auto instResult = builder
		.set_app_name("Vulkan renderer")
		.request_validation_layers(USE_VALIDATION_LAYERS)
        .add_validation_feature_enable(static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eDebugPrintf))
		.set_debug_messenger_severity(static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError))
		.set_debug_messenger_type(static_cast<VkDebugUtilsMessageTypeFlagsEXT>(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance))
		.set_debug_callback(debugMessageFunc)
		.set_debug_callback_user_data_pointer(mRenderer)
		.require_api_version(1, 3, 0)
		.build();
	const vkb::Instance vkbInst = instResult.value();
	mInstance = vk::raii::Instance(mContext, vkbInst.instance);
	vk::raii::DebugUtilsMessengerEXT debugMessenger(mInstance, vkbInst.debug_messenger);
	mDebugMessenger = std::move(debugMessenger);

	VkSurfaceKHR tempSurface = nullptr;
	SDL_Vulkan_CreateSurface(mWindow, *mInstance, &tempSurface);
	mSurface = vk::raii::SurfaceKHR(mInstance, tempSurface);

	vk::PhysicalDeviceVulkan13Features features13{};
	features13.dynamicRendering = true;
	features13.synchronization2 = true;
	vk::PhysicalDeviceVulkan12Features features12{};
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;
	features12.drawIndirectCount = true;
	features12.runtimeDescriptorArray = true;
	features12.descriptorBindingVariableDescriptorCount = true;
	features12.descriptorBindingPartiallyBound = true;
	features12.descriptorBindingSampledImageUpdateAfterBind = true;
	vk::PhysicalDeviceVulkan11Features features11{};
	features11.shaderDrawParameters = true;
	vk::PhysicalDeviceFeatures features{};
	features.multiDrawIndirect = true;
	features.samplerAnisotropy = true;
	features.sampleRateShading = true;

	vkb::PhysicalDeviceSelector selector{ vkbInst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features13)
		.set_required_features_12(features12)
		.set_required_features_11(features11)
		.set_required_features(features)
		.set_surface(*mSurface)
		.select()
		.value();
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	vkb::Device vkbDevice = deviceBuilder.build().value();
	vk::raii::PhysicalDevice chosenGPU(mInstance, vkbDevice.physical_device);
	vk::raii::Device device(chosenGPU, vkbDevice.device);
	mChosenGPU = std::move(chosenGPU);
	mDevice = std::move(device);
	mChosenGPUProperties = mChosenGPU.getProperties();

	vk::raii::Queue computeQueue(mDevice, vkbDevice.get_queue(vkb::QueueType::compute).value());
	vk::raii::Queue graphicsQueue(mDevice, vkbDevice.get_queue(vkb::QueueType::graphics).value());
	mComputeQueue = std::move(computeQueue);
	mGraphicsQueue = std::move(graphicsQueue);
	mComputeQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
	mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = *mChosenGPU;
	allocatorInfo.device = *mDevice;
	allocatorInfo.instance = *mInstance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &mVmaAllocator);

	mRenderer->mRendererEvent.addEventCallback([this](SDL_Event& e) -> void {
		const SDL_Keymod modState = SDL_GetModState();
		const Uint8* keyState = SDL_GetKeyboardState(nullptr);

		if (keyState[SDL_SCANCODE_F10] && e.type == SDL_KEYDOWN && !e.key.repeat) {
			mWindowFullScreen = !mWindowFullScreen;
			SDL_SetWindowFullscreen(mWindow, mWindowFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
			SDL_SetWindowBordered(mWindow, mWindowFullScreen ? SDL_FALSE : SDL_TRUE);
		}
	});
}

void RendererCore::cleanup()
{
	mGraphicsQueue.clear();
	mComputeQueue.clear();
	mSurface.clear();
	mDebugMessenger.clear();
	vmaDestroyAllocator(mVmaAllocator);
	mDevice.clear();
	mChosenGPU.clear();
	mInstance.clear();
	SDL_DestroyWindow(mWindow);
}
