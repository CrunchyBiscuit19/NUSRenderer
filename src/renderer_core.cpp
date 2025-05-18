#include <renderer_core.h>
#include <vk_initializers.h>

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
    mRenderer(renderer),
    mContext(),
    mInstance(nullptr),
    mDebugMessenger(nullptr),
    mChosenGPU(nullptr),
    mDevice(nullptr),
    mSurface(nullptr),
    mComputeQueue(nullptr),
    mGraphicsQueue(nullptr)
{}

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
    VkValidationFeatureEnableEXT validationFeaturesEnables[] = { static_cast<VkValidationFeatureEnableEXT>(vk::ValidationFeatureEnableEXT::eDebugPrintf) };
    auto instResult = builder.set_app_name("Vulkan renderer")
        .request_validation_layers(USE_VALIDATION_LAYERS)
        .set_debug_messenger_severity(static_cast<VkDebugUtilsMessageSeverityFlagsEXT>(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning))
        .set_debug_callback(debugMessageFunc)
        .require_api_version(1, 3, 0)
        .add_validation_feature_enable(validationFeaturesEnables[0])
        .build();
    const vkb::Instance vkbInst = instResult.value();
    mInstance = vk::raii::Instance(mContext, vkbInst.instance);
    vk::raii::DebugUtilsMessengerEXT debugMessenger(mInstance, vkbInst.debug_messenger);
    mDebugMessenger = std::move(debugMessenger);

    VkSurfaceKHR tempSurface = nullptr;
    SDL_Vulkan_CreateSurface(mWindow, *mInstance, &tempSurface);

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
    vk::raii::PhysicalDevice chosenGPU(mInstance, vkbDevice.physical_device);
    vk::raii::Device device(chosenGPU, vkbDevice.device);
    mChosenGPU = std::move(chosenGPU);
    mDevice = std::move(device);

    mSurface = vk::raii::SurfaceKHR(mInstance, tempSurface);

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
