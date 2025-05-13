#include <vk_images.h>
#include <vk_initializers.h>

namespace vkutil {

    void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout, vk::ImageLayout newLayout)
    {
        vk::ImageMemoryBarrier2 imageBarrier{};
        imageBarrier.pNext = nullptr;
        imageBarrier.srcStageMask = srcStageMask;
        imageBarrier.srcAccessMask = srcAccessMask;
        imageBarrier.dstStageMask = dstStageMask;
        imageBarrier.dstAccessMask = dstAccessMask;
        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        const vk::ImageAspectFlagBits aspectMask = (newLayout == vk::ImageLayout::eDepthAttachmentOptimal) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
        imageBarrier.subresourceRange = vkinit::imageSubresourceRange(aspectMask);
        imageBarrier.image = image;

        vk::DependencyInfo depInfo{};
        depInfo.pNext = nullptr;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        cmd.pipelineBarrier2(depInfo);
    }

    void copyImage(vk::CommandBuffer cmd, vk::Image source, vk::Image destination, vk::Extent2D srcSize, vk::Extent2D dstSize)
    {
        vk::ImageBlit2 blitRegion{ };
        blitRegion.pNext = nullptr;
        blitRegion.srcOffsets[1].x = static_cast<int>(srcSize.width);
        blitRegion.srcOffsets[1].y = static_cast<int>(srcSize.height);
        blitRegion.srcOffsets[1].z = 1;
        blitRegion.dstOffsets[1].x = static_cast<int>(dstSize.width);
        blitRegion.dstOffsets[1].y = static_cast<int>(dstSize.height);
        blitRegion.dstOffsets[1].z = 1;
        blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;
        blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        vk::BlitImageInfo2 blitInfo{};
        blitInfo.pNext = nullptr;
        blitInfo.dstImage = destination;
        blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
        blitInfo.srcImage = source;
        blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        blitInfo.filter = vk::Filter::eLinear;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        cmd.blitImage2(blitInfo);
    }

    // Add another barrier to all the mipmap levels to transition the image into VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    void vkutil::generateMipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D imageSize)
    {
        // 2^(mipLevels) = max(width / height), rounded down to nearest 2 power.
        const int mipLevels = static_cast<int>(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;

        // For each level, copy the image from previous level into next level at half resolution.
        // On each copy, transition previous level mipmap layout to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. 
        for (int mip = 0; mip < mipLevels; mip++) {
            vk::Extent2D halfSize = imageSize;
            halfSize.width /= 2;
            halfSize.height /= 2;
            vk::ImageMemoryBarrier2 imageBarrier{};
            imageBarrier.pNext = nullptr;
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
            imageBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            imageBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            constexpr vk::ImageAspectFlagBits aspectMask = vk::ImageAspectFlagBits::eColor;
            imageBarrier.subresourceRange = vkinit::imageSubresourceRange(aspectMask);
            imageBarrier.subresourceRange.levelCount = 1;
            imageBarrier.subresourceRange.baseMipLevel = mip;
            imageBarrier.image = image;
            vk::DependencyInfo depInfo{};
            depInfo.pNext = nullptr;
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &imageBarrier;

            cmd.pipelineBarrier2(depInfo);

            // Not the last level
            if (mip < mipLevels - 1) {
                vk::ImageBlit2 blitRegion{};
                blitRegion.pNext = nullptr;
                blitRegion.srcOffsets[1].x = imageSize.width;
                blitRegion.srcOffsets[1].y = imageSize.height;
                blitRegion.srcOffsets[1].z = 1;
                blitRegion.dstOffsets[1].x = halfSize.width;
                blitRegion.dstOffsets[1].y = halfSize.height;
                blitRegion.dstOffsets[1].z = 1;
                blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcSubresource.mipLevel = mip;
                blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstSubresource.mipLevel = mip + 1;
                vk::BlitImageInfo2 blitInfo{};
                blitInfo.pNext = nullptr;
                blitInfo.dstImage = image;
                blitInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
                blitInfo.srcImage = image;
                blitInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
                blitInfo.filter = vk::Filter::eLinear;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &blitRegion;

                cmd.blitImage2(blitInfo);
                imageSize = halfSize;
            }
        }

        // Transition all mip levels into the final read_only layout
        transitionImage(cmd, image,
            vk::PipelineStageFlagBits2::eAllGraphics,
            vk::AccessFlagBits2::eMemoryRead,
            vk::PipelineStageFlagBits2::eAllGraphics,
            vk::AccessFlagBits2::eMemoryRead,
            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    }

}