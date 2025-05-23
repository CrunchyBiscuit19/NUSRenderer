#include <vk_initializers.h>

vk::CommandPoolCreateInfo vkinit::commandPoolCreateInfo(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags)
{
    vk::CommandPoolCreateInfo info = {};
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

vk::CommandBufferAllocateInfo vkinit::commandBufferAllocateInfo(vk::CommandPool pool, uint32_t count)
{
    vk::CommandBufferAllocateInfo info = {};
    info.pNext = nullptr;
    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = vk::CommandBufferLevel::ePrimary;
    return info;
}

vk::CommandBufferBeginInfo vkinit::commandBufferBeginInfo(vk::CommandBufferUsageFlags flags)
{
    vk::CommandBufferBeginInfo info = {};
    info.pNext = nullptr;
    info.pInheritanceInfo = nullptr;
    info.flags = flags;
    return info;
}

vk::FenceCreateInfo vkinit::fenceCreateInfo(vk::FenceCreateFlags flags)
{
    vk::FenceCreateInfo info = {};
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

vk::SemaphoreCreateInfo vkinit::semaphoreCreateInfo()
{
    vk::SemaphoreCreateInfo info = {};
    info.pNext = nullptr;
    return info;
}

vk::SemaphoreSubmitInfo vkinit::semaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore)
{
	vk::SemaphoreSubmitInfo submitInfo{};
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = semaphore;
	submitInfo.stageMask = stageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;
	return submitInfo;
}

vk::CommandBufferSubmitInfo vkinit::commandBufferSubmitInfo(vk::CommandBuffer cmd)
{
	vk::CommandBufferSubmitInfo info{};
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;
	return info;
}

vk::SubmitInfo2 vkinit::submitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo,
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

vk::PresentInfoKHR vkinit::presentInfo()
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

vk::RenderingAttachmentInfo vkinit::colorAttachmentInfo(vk::ImageView view, std::optional<vk::ClearValue> clear,vk::ImageLayout layout, std::optional<vk::ImageView> resolveImageView)
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

vk::RenderingAttachmentInfo vkinit::depthAttachmentInfo(vk::ImageView view, std::optional<vk::ClearValue> clear, vk::ImageLayout layout)
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

vk::RenderingInfo vkinit::renderingInfo(vk::Extent2D renderExtent, vk::RenderingAttachmentInfo* colorAttachment,
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

vk::ImageSubresourceRange vkinit::imageSubresourceRange(vk::ImageAspectFlags aspectMask)
{
    vk::ImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return subImage;
}

vk::DescriptorSetLayoutBinding vkinit::descriptorSetLayoutBinding(vk::DescriptorType type, vk::ShaderStageFlags stageFlags,
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

vk::DescriptorSetLayoutCreateInfo vkinit::descriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutBinding* bindings,
    uint32_t bindingCount)
{
    vk::DescriptorSetLayoutCreateInfo info = {};
    info.pNext = nullptr;
    info.pBindings = bindings;
    info.bindingCount = bindingCount;
    //info.flags = vk::DescriptorSetLayoutCreateFlags::eDescriptorBufferEXT;
    return info;
}

vk::WriteDescriptorSet vkinit::writeDescriptorImage(vk::DescriptorType type, vk::DescriptorSet dstSet,
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

vk::WriteDescriptorSet vkinit::writeDescriptorBuffer(vk::DescriptorType type, vk::DescriptorSet dstSet,
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

vk::DescriptorBufferInfo vkinit::bufferInfo(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range)
{
    vk::DescriptorBufferInfo binfo {};
    binfo.buffer = buffer;
    binfo.offset = offset;
    binfo.range = range;
    return binfo;
}

vk::ImageCreateInfo vkinit::imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, bool useMultisampling, vk::Extent3D extent)
{
    vk::ImageCreateInfo info = {};
    info.pNext = nullptr;
    info.imageType = vk::ImageType::e2D;
    info.format = format;
    info.extent = extent;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    useMultisampling ? (info.samples = vk::SampleCountFlagBits::e8) : (info.samples = vk::SampleCountFlagBits::e1);
    info.tiling = vk::ImageTiling::eOptimal; // Image is stored on the best gpu format
    info.usage = usageFlags;

    return info;
}

vk::ImageViewCreateInfo vkinit::imageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags)
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

vk::PipelineLayoutCreateInfo vkinit::pipelineLayoutCreateInfo()
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

vk::PipelineShaderStageCreateInfo vkinit::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule,
    const char* entry)
{
    vk::PipelineShaderStageCreateInfo info {};
    info.pNext = nullptr;
    info.stage = stage;
    info.module = shaderModule;
    info.pName = entry;
    return info;
}