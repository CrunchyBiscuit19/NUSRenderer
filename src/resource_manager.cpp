#include <resource_manager.h>
#include <renderer.h>
#include <vk_descriptors.h>
#include <vk_initializers.h>
#include <vk_images.h>

#include <bit>

ResourceManager::ResourceManager(Renderer* renderer):
    mRenderer(renderer)
{}   

void ResourceManager::init()
{
    mImageStagingBuffer = std::move(createStagingBuffer(MAX_IMAGE_SIZE));
	mMeshStagingBuffer = std::move(createStagingBuffer(static_cast<size_t>(DEFAULT_VERTEX_BUFFER_SIZE) + DEFAULT_INDEX_BUFFER_SIZE));
    mMaterialConstantsStagingBuffer = std::move(createStagingBuffer(MAX_MATERIALS * sizeof(MaterialConstants)));
    initDefault();
}

void ResourceManager::initDefault()
{
    // Colour data interpreted as little endian
    constexpr uint32_t white = std::byteswap(0xFFFFFFFF);
    mRenderer->mDefaultImages.emplace(DefaultImage::White, createImage(&white, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
    constexpr uint32_t grey = std::byteswap(0xAAAAAAFF);
    mRenderer->mDefaultImages.emplace(DefaultImage::Grey, createImage(&grey, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
    constexpr uint32_t black = std::byteswap(0x000000FF);
    mRenderer->mDefaultImages.emplace(DefaultImage::Black, createImage(&black, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
    constexpr uint32_t blue = std::byteswap(0x769DDBFF);
    mRenderer->mDefaultImages.emplace(DefaultImage::Blue, createImage(&blue, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            constexpr uint32_t magenta = std::byteswap(0xFF00FFFF);
            pixels[static_cast<std::array<uint32_t, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    mRenderer->mDefaultImages.emplace(DefaultImage::Checkerboard, createImage(pixels.data(), vk::Extent3D{ 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));

    vk::SamplerCreateInfo sampl;
    sampl.magFilter = vk::Filter::eLinear;
    sampl.minFilter = vk::Filter::eLinear;
    mRenderer->mDefaultSampler = mRenderer->mRendererCore.mDevice.createSampler(sampl);
}

AllocatedBuffer ResourceManager::createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;
    VkBufferCreateInfo bufferInfo1 = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VkBuffer tempBuffer;
    vmaCreateBuffer(mRenderer->mRendererCore.mVmaAllocator, &bufferInfo1, &vmaAllocInfo, &tempBuffer, &newBuffer.allocation, &newBuffer.info);
    newBuffer.buffer = vk::raii::Buffer(mRenderer->mRendererCore.mDevice, tempBuffer);
	newBuffer.allocator = &mRenderer->mRendererCore.mVmaAllocator;

    return newBuffer;
}

AllocatedImage ResourceManager::createImage(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped)
{
    vk::ImageCreateInfo newImageCreateInfo = vkinit::imageCreateInfo(format, usage, extent);
    if (mipmapped) { newImageCreateInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1; }
    VkImageCreateInfo newImageCreateInfo1 = static_cast<vk::ImageCreateInfo>(newImageCreateInfo);

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaAllocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    AllocatedImage newImage;
	newImage.imageFormat = format;
    newImage.imageExtent = extent;
    VkImage tempImage;
    vmaCreateImage(mRenderer->mRendererCore.mVmaAllocator, &newImageCreateInfo1, &vmaAllocInfo, &tempImage, &newImage.allocation, nullptr);
    newImage.image = vk::raii::Image(mRenderer->mRendererCore.mDevice, tempImage);
	newImage.allocator = &mRenderer->mRendererCore.mVmaAllocator;

    vk::ImageAspectFlagBits aspectFlag = vk::ImageAspectFlagBits::eColor;
    if (format == vk::Format::eD32Sfloat)
        aspectFlag = vk::ImageAspectFlagBits::eDepth;
    vk::ImageViewCreateInfo newImageViewCreateInfo = vkinit::imageViewCreateInfo(format, *newImage.image, aspectFlag);
    newImageViewCreateInfo.subresourceRange.levelCount = newImageCreateInfo.mipLevels;

    newImage.imageView = mRenderer->mRendererCore.mDevice.createImageView(newImageViewCreateInfo);

    return newImage;
}

AllocatedImage ResourceManager::createImage(const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped)
{
    const size_t dataSize = extent.depth * extent.width * extent.height * 4;
    std::memcpy(mImageStagingBuffer.info.pMappedData, data, dataSize);

    AllocatedImage newImage = createImage(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipmapped);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        vkutil::transitionImage(*cmd, *newImage.image, 
        vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, 
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentRead, 
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = extent;

        cmd.copyBufferToImage(*mImageStagingBuffer.buffer, *newImage.image, vk::ImageLayout::eTransferDstOptimal, copyRegion);

        if (mipmapped)
            vkutil::generateMipmaps(*cmd, *newImage.image, vk::Extent2D{ newImage.imageExtent.width, newImage.imageExtent.height });
        else
            vkutil::transitionImage(*cmd, *newImage.image,
                vk::PipelineStageFlagBits2KHR::eAllGraphics, vk::AccessFlagBits2::eMemoryRead,
                vk::PipelineStageFlagBits2KHR::eAllGraphics, vk::AccessFlagBits2::eMemoryRead,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    });

    return newImage;
}

AllocatedBuffer ResourceManager::createStagingBuffer(size_t allocSize)
{
    return createBuffer(allocSize,  vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void ResourceManager::loadMaterialsConstantsBuffer(GLTFModel* model, std::vector<MaterialConstants>& materialConstants)
{
    std::memcpy(static_cast<char*>(mMaterialConstantsStagingBuffer.info.pMappedData), materialConstants.data(), materialConstants.size() * sizeof(MaterialConstants));

    vk::BufferCopy materialConstantsCopy {};
    materialConstantsCopy.dstOffset = 0;
    materialConstantsCopy.srcOffset = 0;
    materialConstantsCopy.size = materialConstants.size() * sizeof(MaterialConstants);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        cmd.copyBuffer(*mMaterialConstantsStagingBuffer.buffer, *model->mMaterialConstantsBuffer.buffer, materialConstantsCopy);
    });
}

void ResourceManager::loadMeshBuffers(Mesh* mesh, std::vector<uint32_t>& srcIndexVector, std::vector<Vertex>& srcVertexVector)
{
    const vk::DeviceSize srcVertexVectorSize = srcVertexVector.size() * sizeof(Vertex);
    const vk::DeviceSize srcIndexVectorSize = srcIndexVector.size() * sizeof(uint32_t);

    mesh->mVertexBuffer = createBuffer(srcVertexVectorSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
    mesh->mIndexBuffer = createBuffer(srcIndexVectorSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

    std::memcpy(static_cast<char*>(mMeshStagingBuffer.info.pMappedData) + 0, srcVertexVector.data(), srcVertexVectorSize);
    std::memcpy(static_cast<char*>(mMeshStagingBuffer.info.pMappedData) + srcVertexVectorSize, srcIndexVector.data(), srcIndexVectorSize);

    vk::BufferCopy vertexCopy {};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = srcVertexVectorSize;
    vk::BufferCopy indexCopy {};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = srcVertexVectorSize;
    indexCopy.size = srcIndexVectorSize;

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        cmd.copyBuffer(*mMeshStagingBuffer.buffer, *mesh->mVertexBuffer.buffer, vertexCopy);
        cmd.copyBuffer(*mMeshStagingBuffer.buffer, *mesh->mIndexBuffer.buffer, indexCopy);
    });
}

void ResourceManager::cleanup()
{
    mImageStagingBuffer.cleanup();
	mMeshStagingBuffer.cleanup();
    mMaterialConstantsStagingBuffer.cleanup();
    mRenderer->mDefaultImages.clear();
	mRenderer->mDefaultSampler.clear();
}

ResourceManager::ResourceManager(ResourceManager&& other) noexcept : 
    mRenderer(std::exchange(other.mRenderer, nullptr)),
    mImageStagingBuffer(std::move(other.mImageStagingBuffer)),
    mMeshStagingBuffer(std::move(other.mMeshStagingBuffer))
{}

ResourceManager& ResourceManager::operator=(ResourceManager && other) noexcept {
    if (this != &other) {
        mRenderer = std::exchange(other.mRenderer, nullptr);
        mImageStagingBuffer = std::move(other.mImageStagingBuffer);
        mMeshStagingBuffer = std::move(other.mMeshStagingBuffer);
    }
    return *this;
}