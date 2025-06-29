#include <Renderer/Renderer.h>
#include <Utils/ImmSubmit.h>
#include <Utils/Helper.h>

#include <quill/LogMacros.h>

ImmSubmit::ImmSubmit(Renderer* renderer) :
	mRenderer(renderer),
	mCommandPool(nullptr),
	mCommandBuffer(nullptr),
	mFence(nullptr)
{
}

void ImmSubmit::init()
{
	vk::CommandPoolCreateInfo commandPoolInfo = vkhelper::commandPoolCreateInfo(
		mRenderer->mCore.mGraphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	mCommandPool = mRenderer->mCore.mDevice.createCommandPool(commandPoolInfo);

	LOG_INFO(mRenderer->mLogger, "ImmSubmit Command Pool Created");

	vk::CommandBufferAllocateInfo cmdAllocInfo = vkhelper::commandBufferAllocateInfo(*mCommandPool, 1);
	mCommandBuffer = std::move(mRenderer->mCore.mDevice.allocateCommandBuffers(cmdAllocInfo)[0]);

	LOG_INFO(mRenderer->mLogger, "ImmSubmit Command Buffer Created");

	vk::FenceCreateInfo fenceCreateInfo = vkhelper::fenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
	mFence = mRenderer->mCore.mDevice.createFence(fenceCreateInfo);

	LOG_INFO(mRenderer->mLogger, "ImmSubmit Fence Created");
}

void ImmSubmit::individualSubmit(std::function<void(Renderer* renderer, vk::CommandBuffer cmd)>&& function)
{
	mRenderer->mCore.mDevice.resetFences(*mFence);

	mCommandBuffer.reset();

	vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	mCommandBuffer.begin(cmdBeginInfo);
	function(mRenderer, *mCommandBuffer);
	mCommandBuffer.end();

	vk::CommandBufferSubmitInfo cmdSubmitInfo = vkhelper::commandBufferSubmitInfo(*mCommandBuffer);
	vk::SubmitInfo2 submitInfo = vkhelper::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

	mRenderer->mCore.mGraphicsQueue.submit2(submitInfo, *mFence);
	mRenderer->mCore.mDevice.waitForFences(*mFence, true, 1e9); // DO NOT MOVE THIS TO THE TOP
}

void ImmSubmit::queuedSubmit()
{
	mRenderer->mCore.mDevice.resetFences(*mFence);

	mCommandBuffer.reset();

	vk::CommandBufferBeginInfo cmdBeginInfo = vkhelper::commandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	mCommandBuffer.begin(cmdBeginInfo);
	for (auto& callback : mCallbacks)
	{
		callback(mRenderer, *mCommandBuffer);
	}
	mCommandBuffer.end();

	vk::CommandBufferSubmitInfo cmdSubmitInfo = vkhelper::commandBufferSubmitInfo(*mCommandBuffer);
	vk::SubmitInfo2 submitInfo = vkhelper::submitInfo(&cmdSubmitInfo, nullptr, nullptr);

	mRenderer->mCore.mGraphicsQueue.submit2(submitInfo, *mFence);
	mRenderer->mCore.mDevice.waitForFences(*mFence, true, 1e9); // DO NOT MOVE THIS TO THE TOP
}

void ImmSubmit::cleanup()
{
	mFence.clear();
	LOG_INFO(mRenderer->mLogger, "ImmSubmit Fence Destroyed");
	mCommandBuffer.clear();
	LOG_INFO(mRenderer->mLogger, "ImmSubmit Command Buffer Destroyed");
	mCommandPool.clear();
	LOG_INFO(mRenderer->mLogger, "ImmSubmit Command Pool Destroyed");
}
