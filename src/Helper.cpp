#include <Helper.h>
#include <RendererInfrastructure.h>

vk::CommandPoolCreateInfo vkhelper::commandPoolCreateInfo(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags)
{
    vk::CommandPoolCreateInfo info = {};
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

vk::CommandBufferAllocateInfo vkhelper::commandBufferAllocateInfo(vk::CommandPool pool, uint32_t count)
{
    vk::CommandBufferAllocateInfo info = {};
    info.pNext = nullptr;
    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = vk::CommandBufferLevel::ePrimary;
    return info;
}

vk::CommandBufferBeginInfo vkhelper::commandBufferBeginInfo(vk::CommandBufferUsageFlags flags)
{
    vk::CommandBufferBeginInfo info = {};
    info.pNext = nullptr;
    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

vk::FenceCreateInfo vkhelper::fenceCreateInfo(vk::FenceCreateFlags flags)
{
    vk::FenceCreateInfo info = {};
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

vk::SemaphoreCreateInfo vkhelper::semaphoreCreateInfo()
{
    vk::SemaphoreCreateInfo info = {};
    info.pNext = nullptr;
    return info;
}

vk::SemaphoreSubmitInfo vkhelper::semaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore)
{
	vk::SemaphoreSubmitInfo submitInfo{};
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = semaphore;
	submitInfo.stageMask = stageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;
	return submitInfo;
}

vk::CommandBufferSubmitInfo vkhelper::commandBufferSubmitInfo(vk::CommandBuffer cmd)
{
	vk::CommandBufferSubmitInfo info{};
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;
	return info;
}

vk::SubmitInfo2 vkhelper::submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo,
    vk::SemaphoreSubmitInfo* waitSemaphoreInfo)
{
    vk::SubmitInfo2 info = {};
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

vk::PresentInfoKHR vkhelper::presentInfo()
{
    vk::PresentInfoKHR info = {};
    info.pNext = 0;
    info.swapchainCount = 0;
    info.pSwapchains = nullptr;
    info.pWaitSemaphores = nullptr;
    info.waitSemaphoreCount = 0;
    info.pImageIndices = nullptr;
    return info;
}

vk::RenderingAttachmentInfo vkhelper::colorAttachmentInfo(vk::ImageView view, std::optional<vk::ClearValue> clear,vk::ImageLayout layout, std::optional<vk::ImageView> resolveImageView)
{
    vk::RenderingAttachmentInfo colorAttachment {};
    colorAttachment.pNext = nullptr;
    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = clear.has_value() ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    clear.has_value() ? colorAttachment.clearValue = clear.value() : colorAttachment.clearValue = {};
    if (resolveImageView.has_value()) {
        colorAttachment.resolveImageView = resolveImageView.value();
        colorAttachment.resolveMode = vk::ResolveModeFlagBits::eAverage;
        colorAttachment.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    }
    
    return colorAttachment;
}

vk::RenderingAttachmentInfo vkhelper::depthAttachmentInfo(vk::ImageView view, std::optional<vk::ClearValue> clear, vk::ImageLayout layout)
{
    vk::RenderingAttachmentInfo depthAttachment {};
    depthAttachment.pNext = nullptr;
    depthAttachment.imageView = view;
    depthAttachment.imageLayout = layout;
    depthAttachment.loadOp = clear.has_value() ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    if (clear.has_value()) {
        depthAttachment.clearValue = clear.value();
    }
    depthAttachment.clearValue.depthStencil.depth = 0.f;
    return depthAttachment;
}

vk::RenderingInfo vkhelper::renderingInfo(vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachment,
    vk::RenderingAttachmentInfo* depthAttachment)
{
    vk::RenderingInfo renderInfo {};
    renderInfo.pNext = nullptr;
    renderInfo.renderArea = vk::Rect2D { vk::Offset2D { 0, 0 }, renderExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = colorAttachment;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = nullptr;
    return renderInfo;
}

vk::ImageSubresourceRange vkhelper::imageSubresourceRange(vk::ImageAspectFlags aspectMask)
{
    vk::ImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return subImage;
}

vk::DescriptorSetLayoutBinding vkhelper::descriptorSetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags,
    uint32_t binding)
{
    vk::DescriptorSetLayoutBinding setbind = {};
    setbind.binding = binding;
    setbind.descriptorCount = 1;
    setbind.descriptorType = type;
    setbind.pImmutableSamplers = nullptr;
    setbind.stageFlags = stageFlags;

    return setbind;
}

vk::DescriptorSetLayoutCreateInfo vkhelper::descriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutBinding* bindings,
    uint32_t bindingCount)
{
    vk::DescriptorSetLayoutCreateInfo info = {};
    info.pNext = nullptr;
    info.pBindings = bindings;
    info.bindingCount = bindingCount;
    //info.flags = vk::DescriptorSetLayoutCreateFlags::eDescriptorBufferEXT;
    return info;
}

vk::WriteDescriptorSet vkhelper::writeDescriptorImage(vk::DescriptorType type, vk::DescriptorSet dstSet,
    vk::DescriptorImageInfo* imageInfo, uint32_t binding)
{
    vk::WriteDescriptorSet write = {};
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = imageInfo;
    return write;
}

vk::WriteDescriptorSet vkhelper::writeDescriptorBuffer(vk::DescriptorType type, vk::DescriptorSet dstSet,
    vk::DescriptorBufferInfo* bufferInfo, uint32_t binding)
{
    vk::WriteDescriptorSet write = {};
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = bufferInfo;
    return write;
}

vk::DescriptorBufferInfo vkhelper::bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range)
{
    vk::DescriptorBufferInfo binfo {};
    binfo.buffer = buffer;
    binfo.offset = offset;
    binfo.range = range;
    return binfo;
}

vk::ImageCreateInfo vkhelper::imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, bool useMultisampling, vk::Extent3D extent)
{
    vk::ImageCreateInfo info = {};
    info.pNext = nullptr;
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    useMultisampling ? (info.samples = MSAA_LEVEL) : (info.samples = vk::SampleCountFlagBits::e1);
    info.tiling = vk::ImageTiling::eOptimal; // Image is stored on the best gpu format
    info.usage = usageFlags;

    return info;
}

vk::ImageViewCreateInfo vkhelper::imageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo info = {};
    info.pNext = nullptr;
    info.viewType = vk::ImageViewType::e2D;
    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectFlags;
    return info;
}

void vkhelper::transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask, vk::ImageLayout currentLayout, vk::ImageLayout newLayout)
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
    imageBarrier.subresourceRange = vkhelper::imageSubresourceRange(aspectMask);
    imageBarrier.image = image;

    vk::DependencyInfo depInfo{};
    depInfo.pNext = nullptr;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    cmd.pipelineBarrier2(depInfo);
}

void vkhelper::copyImage(vk::CommandBuffer cmd, vk::Image source, vk::Image destination, vk::Extent2D srcSize, vk::Extent2D dstSize)
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

void vkhelper::generateMipmaps(vk::CommandBuffer cmd, vk::Image image, vk::Extent2D imageSize, bool cubemap)
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
        imageBarrier.subresourceRange = vkhelper::imageSubresourceRange(aspectMask);
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.baseMipLevel = mip;
        imageBarrier.image = image;
        vk::DependencyInfo depInfo{};
        depInfo.pNext = nullptr;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        // Add another barrier to all the mipmap levels to transition the image into VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
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

int vkhelper::getFormatTexelSize(vk::Format format)
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

vk::PipelineLayoutCreateInfo vkhelper::pipelineLayoutCreateInfo()
{
    vk::PipelineLayoutCreateInfo info {};
    info.pNext = nullptr;
    //info.flags = vk::PipelineLayoutCreateFlags::eIndependentSetsEXT;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

vk::PipelineShaderStageCreateInfo vkhelper::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule,
    const char* entry)
{
    vk::PipelineShaderStageCreateInfo info {};
    info.pNext = nullptr;
    info.stage = stage;
    info.module = shaderModule;
    info.pName = entry;
    return info;
}