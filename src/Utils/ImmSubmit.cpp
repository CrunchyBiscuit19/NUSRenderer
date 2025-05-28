#include <Renderer/Renderer.h>
#include <Utils/ImmSubmit.h>
#include <Utils/Helper.h>

ImmSubmit::ImmSubmit(Renderer* renderer) :
	mRenderer(renderer),
	mCommandPool(nullptr),
	mCommandBuffer(nullptr),
	mFence(nullptr)
{
}

void ImmSubmit::init()
{
	vk::CommandPoolCreateInfo commandPoolInfo = vkhelper::commandPoolCreateInfo(mRenderer->mRendererCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	mCommandPool = mRenderer->mRendererCore.mDevice.createCommandPool(commandPoolInfo);

	vk::CommandBufferAllocateInfo cmdAllocInfo = vkhelper::commandBufferAllocateInfo(*mCommandPool, 1);
	mCommandBuffer = std::move(mRenderer->mRendererCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);

	vk::FenceCreateInfo fenceCreateInfo = vkhelper::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
	mFence = mRenderer->mRendererCore.mDevice.createFence(fenceCreateInfo);
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
	mFence.clear();
	mCommandBuffer.clear();
	mCommandPool.clear();
}
