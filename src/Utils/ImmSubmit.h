#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <functional>

class Renderer;

class ImmSubmit
{
	Renderer* mRenderer;

public:
	vk::raii::CommandPool mCommandPool;
	vk::raii::CommandBuffer mCommandBuffer;
	vk::raii::Fence mFence;
	std::vector<std::function<void(Renderer* renderer, vk::CommandBuffer)>> mCallbacks;

	ImmSubmit(Renderer* renderer);

	ImmSubmit(ImmSubmit&& other) noexcept :
		mRenderer(other.mRenderer),
		mCommandPool(std::move(other.mCommandPool)),
		mCommandBuffer(std::move(other.mCommandBuffer)),
		mFence(std::move(other.mFence))
	{
		other.mRenderer = nullptr;
	}

	ImmSubmit& operator=(ImmSubmit&& other) noexcept
	{
		if (this != &other)
		{
			mRenderer = other.mRenderer;
			mCommandPool = std::move(other.mCommandPool);
			mCommandBuffer = std::move(other.mCommandBuffer);
			mFence = std::move(other.mFence);
			other.mRenderer = nullptr;
		}
		return *this;
	}

	ImmSubmit(const ImmSubmit&) = delete;
	ImmSubmit& operator=(const ImmSubmit&) = delete;

	void init();

	void individualSubmit(std::function<void(Renderer* renderer, vk::CommandBuffer cmd)>&& function);
	void queuedSubmit();

	void cleanup();
};
