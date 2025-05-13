#include <renderer.h>
#include <imm_submit.h>
#include <vk_initializers.h>

ImmSubmit::ImmSubmit(Renderer* renderer):
    mRenderer(renderer),
	mCommandPool(nullptr),
	mCommandBuffer(nullptr),
	mFence(nullptr),
	mDescriptorPool(nullptr)
{}   

void ImmSubmit::init()
{
    vk::CommandPoolCreateInfo commandPoolInfo = vkinit::commandPoolCreateInfo(mRenderer->mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandPool = mRenderer->mDevice.createCommandPool(commandPoolInfo);

    vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(*mCommandPool, 1);
    mCommandBuffer = std::move(mRenderer->mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);

    vk::FenceCreateInfo fenceCreateInfo = vkinit::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    mFence = mRenderer->mDevice.createFence(fenceCreateInfo);

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
    mDescriptorPool = mRenderer->mDevice.createDescriptorPool(poolInfo);
}

void ImmSubmit::submit(std::function<void(vk::raii::CommandBuffer& cmd) >&& function)
{
    mRenderer->mDevice.resetFences(*mFence);
    mCommandBuffer.reset();

    vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::commandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    mCommandBuffer.begin(cmdBeginInfo);
    function(mCommandBuffer);
    mCommandBuffer.end();

    vk::CommandBufferSubmitInfo cmdSubmitInfo = vkinit::commandBufferSubmitInfo(*mCommandBuffer);
    vk::SubmitInfo2 submitInfo = vkinit::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

    mRenderer->mGraphicsQueue.submit2(submitInfo, *mFence);
    mRenderer->mDevice.waitForFences(*mFence, true, 1e9);
}

void ImmSubmit::cleanup()
{
    mDescriptorPool.clear();
    mFence.clear();
    mCommandBuffer.clear();
    mCommandPool.clear();
}
