#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace vkinit {
vk::CommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags);
vk::CommandBufferAllocateInfo commandBufferAllocateInfo(vk::CommandPool pool, uint32_t count = 1);

vk::CommandBufferBeginInfo commandBufferBeginInfo(vk::CommandBufferUsageFlags flags);
vk::CommandBufferSubmitInfo commandBufferSubmitInfo(vk::CommandBuffer cmd);

vk::FenceCreateInfo fenceCreateInfo(vk::FenceCreateFlags flags);

vk::SemaphoreCreateInfo semaphoreCreateInfo();

vk::SubmitInfo2 submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo);
vk::PresentInfoKHR presentInfo();

vk::RenderingAttachmentInfo colorAttachmentInfo(vk::ImageView view, std::optional<vk::ClearValue> clear, vk::ImageLayout layout, std::optional<vk::ImageView> swapchainResolveView = std::nullopt);

vk::RenderingAttachmentInfo depthAttachmentInfo(vk::ImageView view, std::optional<vk::ClearValue> clear, vk::ImageLayout layout);

vk::RenderingInfo renderingInfo(vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachment,
    vk::RenderingAttachmentInfo* depthAttachment);

vk::ImageSubresourceRange imageSubresourceRange(vk::ImageAspectFlags aspectMask);

vk::SemaphoreSubmitInfo semaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore);
vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags,
    uint32_t binding);
vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutBinding* bindings,
    uint32_t bindingCount);
vk::WriteDescriptorSet writeDescriptorImage(vk::DescriptorType type, vk::DescriptorSet dstSet,
    vk::DescriptorImageInfo* imageInfo, uint32_t binding);
vk::WriteDescriptorSet writeDescriptorBuffer(vk::DescriptorType type, vk::DescriptorSet dstSet,
    vk::DescriptorBufferInfo* bufferInfo, uint32_t binding);
vk::DescriptorBufferInfo bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range);

vk::ImageCreateInfo imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, bool multisampling, vk::Extent3D extent);
vk::ImageViewCreateInfo imageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags);
vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo();
vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule,
    const char * entry = "main");
}
