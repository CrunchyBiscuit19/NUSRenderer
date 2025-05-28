#include <Renderer/Renderer.h>
#include <Utils/ImmSubmit.h>
#include <Utils/Helper.h>

ImmSubmit::ImmSubmit(Renderer* renderer):
    mRenderer(renderer),
	mCommandPool(nullptr),
	mCommandBuffer(nullptr),
	mFence(nullptr),
	mDescriptorPool(nullptr)
{}   

void ImmSubmit::init()
{
    vk::CommandPoolCreateInfo commandPoolInfo = vkhelper::commandPoolCreateInfo(mRenderer->mRendererCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);

    vk::CommandBufferAllocateInfo cmdAllocInfo = vkhelper::commandBufferAllocateInfo(*mCommandPool, 1);
    mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);

    vk::FenceCreateInfo fenceCreateInfo = vkhelper::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    mFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);

    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 },
    };
    vk::DescriptorPoolCreateInfo poolInfo = {};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    mDescriptorPool = mRenderer->mRendererCore.mDevice.createDescriptorPool(poolInfo);
}

void ImmSubmit::submit(std::function<void(vk::raii::CommandBuffer& cmd) >&& function)
{
    mRenderer->mRendererCore.mDevice.resetFences(*mFence);
    mCommandBuffer.reset();

    vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    mCommandBuffer.begin(cmdBeginInfo);
    function(mCommandBuffer);
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo cmdSubmitInfo = vkhelper::commandBufferSubmitInfo(*mCommandBuffer);
    vk::SubmitInfo2 submitInfo = vkhelper::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

    mRenderer->mRendererCore.mGraphicsQueue.submit2(submitInfo, *mFence);
    mRenderer->mRendererCore.mDevice.waitForFences(*mFence, true, 1e9);
}

void ImmSubmit::cleanup()
{
    mDescriptorPool.clear();
    mFence.clear();
    mCommandBuffer.clear();
    mCommandPool.clear();
}
