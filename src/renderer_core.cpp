#include <renderer.h>
#include <renderer_core.h>
#include <vk_initializers.h>

#include <Vkbootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <fmt/core.h>

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(
    VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    void* pUserData
)
{
    std::string message;
    message += vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) + ": " +
        vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) + ":\n";
    message += std::string("\t") + "messageIDName   = <" + pCallbackData->pMessageIdName + ">\n";
    message += std::string("\t") + "messageIdNumber = " + std::to_string(pCallbackData->messageIdNumber) + "\n";
    message += std::string("\t") + "message         = <" + pCallbackData->pMessage + ">\n";
    if (0 < pCallbackData->queueLabelCount)
    {
        message += std::string("\t") + "Queue Labels:\n";
        for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++)
            message += std::string("\t\t") + "labelName = <" + pCallbackData->pQueueLabels[i].pLabelName + ">\n";
    }
    if (0 < pCallbackData->cmdBufLabelCount)
    {
        message += std::string("\t") + "CommandBuffer Labels:\n";
        for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
            message += std::string("\t\t") + "labelName = <" + pCallbackData->pCmdBufLabels[i].pLabelName + ">\n";

    }
    if (0 < pCallbackData->objectCount)
    {
        for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
        {
            message += std::string("\t") + "Object " + std::to_string(i) + "\n";
            message += std::string("\t\t") + "objectType   = " + vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) + "\n";
            message += std::string("\t\t") + "objectHandle = " + std::to_string(pCallbackData->pObjects[i].objectHandle) + "\n";
            if (pCallbackData->pObjects[i].pObjectName)
                message += std::string("\t\t") + "objectName   = <" + pCallbackData->pObjects[i].pObjectName + ">\n";
        }
    }
    fmt::println("{}", message);
    return false;
}

RendererCore::RendererCore(Renderer* renderer) :
    mRenderer(renderer)
{}

void RendererCore::init()
{
    SDL_Init(SDL_INIT_VIDEO);
    mRenderer->mWindow = SDL_CreateWindow(
        "Renderer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        static_cast<int>(mRenderer->mWindowExtent.width),
        static_cast<int>(mRenderer->mWindowExtent.height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    mRenderer->mContext = vk::raii::Context();

    vkb::InstanceBuilder builder;
    VkValidationFeatureEnableEXT validationFeaturesEnables[] = { static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eDebugPrintf) };
    auto instResult = builder.set_app_name("Vulkan renderer")
        .request_validation_layers(useValidationLayers)
        .set_debug_messenger_severity(static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning))
        .set_debug_callback(debugMessageFunc)
        .require_api_version(1, 3, 0)
        .add_validation_feature_enable(validationFeaturesEnables[0])
        .build();
    const vkb::Instance vkbInst = instResult.value();
    mRenderer->mInstance = vk::raii::Instance(mRenderer->mContext, vkbInst.instance);
    vk::raii::DebugUtilsMessengerEXT debugMessenger(mRenderer->mInstance, vkbInst.debug_messenger);
    mRenderer->mDebugMessenger = std::move(debugMessenger);

    VkSurfaceKHR tempSurface = nullptr;
    SDL_Vulkan_CreateSurface(mRenderer->mWindow, *mRenderer->mInstance, &tempSurface);

    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    vk::PhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.drawIndirectCount = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    vk::PhysicalDeviceVulkan11Features features11{};
    features11.shaderDrawParameters = true;
    vk::PhysicalDeviceFeatures features{};
    features.multiDrawIndirect = true;

    vkb::PhysicalDeviceSelector selector{ vkbInst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_required_features_11(features11)
        .set_required_features(features)
        .set_surface(tempSurface)
        .select()
        .value();
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();
    vk::raii::PhysicalDevice chosenGPU(mRenderer->mInstance, vkbDevice.physical_device);
    vk::raii::Device device(chosenGPU, vkbDevice.device);
    mRenderer->mChosenGPU = std::move(chosenGPU);
    mRenderer->mDevice = std::move(device);

    mRenderer->mSurface = vk::raii::SurfaceKHR(mRenderer->mInstance, tempSurface);

    vk::raii::Queue computeQueue(mRenderer->mDevice, vkbDevice.get_queue(vkb::QueueType::compute).value());
    vk::raii::Queue graphicsQueue(mRenderer->mDevice, vkbDevice.get_queue(vkb::QueueType::graphics).value());
    mRenderer->mComputeQueue = std::move(computeQueue);
    mRenderer->mGraphicsQueue = std::move(graphicsQueue);
    mRenderer->mComputeQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
    mRenderer->mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = *mRenderer->mChosenGPU;
    allocatorInfo.device = *mRenderer->mDevice;
    allocatorInfo.instance = *mRenderer->mInstance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &mRenderer->mAllocator);
}

void RendererCore::cleanup()
{
    mRenderer->mGraphicsQueue.clear();
    mRenderer->mComputeQueue.clear();
    mRenderer->mSurface.clear();
    mRenderer->mDebugMessenger.clear();
    vmaDestroyAllocator(mRenderer->mAllocator);
    mRenderer->mDevice.clear();
    mRenderer->mChosenGPU.clear();
    mRenderer->mInstance.clear();
    SDL_DestroyWindow(mRenderer->mWindow);
}
