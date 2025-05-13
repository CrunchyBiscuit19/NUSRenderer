#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace vkutil {
    void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout, vk::ImageLayout newLayout);
    void copyImage(vk::CommandBuffer cmd, vk::Image source, vk::Image destination, vk::Extent2D srcSize, vk::Extent2D dstSize);
    void generateMipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D imageSize);
};