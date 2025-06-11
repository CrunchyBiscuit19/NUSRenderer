#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace vkhelper {
	vk::CommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags);
	vk::CommandBufferAllocateInfo commandBufferAllocateInfo(vk::CommandPool pool, uint32_t count = 1);
	vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlags flags);
	vk::CommandBufferSubmitInfo commandBufferSubmitInfo(vk::CommandBuffer cmd);

	vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags flags);
	vk::SemaphoreCreateInfo semaphoreCreateInfo();
	vk::SemaphoreSubmitInfo semaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore);

	vk::SubmitInfo2 submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo);
	vk::PresentInfoKHR presentInfo();

	vk::RenderingAttachmentInfo colorAttachmentInfo(vk::ImageView view, vk::ImageLayout layout, vk::AttachmentLoadOp load = vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp store = vk::AttachmentStoreOp::eStore, std::optional<vk::ImageView> swapchainResolveView = std::nullopt);
	vk::RenderingAttachmentInfo depthAttachmentInfo(vk::ImageView view, vk::ImageLayout layout, vk::AttachmentLoadOp load = vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp store = vk::AttachmentStoreOp::eStore);
	vk::RenderingInfo renderingInfo(vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachment,
		vk::RenderingAttachmentInfo* depthAttachment);

	vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags,
		uint32_t binding);
	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutBinding* bindings,
		uint32_t bindingCount);
	vk::WriteDescriptorSet writeDescriptorImage(vk::DescriptorType type, vk::DescriptorSet dstSet,
		vk::DescriptorImageInfo* imageInfo, uint32_t binding);
	vk::WriteDescriptorSet writeDescriptorBuffer(vk::DescriptorType type, vk::DescriptorSet dstSet,
		vk::DescriptorBufferInfo* bufferInfo, uint32_t binding);
	vk::DescriptorBufferInfo bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range);

	vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlags aspectMask);
	vk::ImageCreateInfo imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, bool multisampling, vk::Extent3D extent);
	vk::ImageViewCreateInfo imageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags);

	void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout, vk::ImageLayout newLayout);
	void copyImage(vk::CommandBuffer cmd, vk::Image source, vk::Image destination, vk::Extent2D srcSize, vk::Extent2D dstSize);
	void generateMipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D imageSize, bool cubemap = false);
	int getFormatTexelSize(vk::Format format);

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo();
	vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule, const char* entry = "main");

	void createBufferPipelineBarrier(vk::CommandBuffer cmd, vk::Buffer buffer, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask);
}
