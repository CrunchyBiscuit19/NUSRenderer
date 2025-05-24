#include <vk_images.h>
#include <vk_initializers.h>
#include <renderer.h>

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
    void generateMipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D imageSize, bool cubemap)
    {
        int numFaces = cubemap ? NUMBER_OF_CUBEMAP_FACES : 1;
        const int mipLevels = static_cast<int>(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;

        for (int mip = 0; mip < mipLevels; mip++) {
            

            // Transition current mipmap level from eTransferDstOptimal to eTransferSrcOptimal
            vk::Extent2D halfSize = imageSize;
            halfSize.width /= 2;
            halfSize.height /= 2;
            vk::ImageMemoryBarrier2 imageBarrier{};
            imageBarrier.pNext = nullptr;
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
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

            for (int face = 0; face < numFaces; face++) {
                // Copy the image from previous level into next level at half resolution (except if at last level).
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
                    blitRegion.srcSubresource.baseArrayLayer = face;
                    blitRegion.srcSubresource.layerCount = 1;
                    blitRegion.srcSubresource.mipLevel = mip;
                    blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                    blitRegion.dstSubresource.baseArrayLayer = face;
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
        }

        // Transition all mip levels into the final read_only layout
        transitionImage(cmd, image,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    int getFormatTexelSize(vk::Format format) 
    {
        int bytesPerTexel = 0;
        switch (format) {
        case vk::Format::eR8G8B8A8Unorm:
            bytesPerTexel = 4;
            break;
        default:
            break;
        }

        return bytesPerTexel;
    }

}