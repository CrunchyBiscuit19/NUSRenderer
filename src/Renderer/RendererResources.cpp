#include <Renderer/Renderer.h>
#include <Renderer/RendererResources.h>
#include <Utils/Helper.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

#include <bit>
#include <fstream>

AllocatedImage::AllocatedImage()
    : image(nullptr),
      view(nullptr),
      format(vk::Format::eUndefined),
      extent({0, 0, 0}),
      aspect(vk::ImageAspectFlagBits::eNone),
      currentLayout(vk::ImageLayout::eUndefined),
      currentStage(vk::PipelineStageFlagBits2::eNone),
      currentAccess(vk::AccessFlagBits2::eNone),
      allocator(nullptr),
      allocation(nullptr) {}

AllocatedImage::AllocatedImage(
    vk::raii::Image image, vk::raii::ImageView view, vk::Format format, vk::Extent3D extent, vk::ImageAspectFlags aspect, VmaAllocator* allocator, VmaAllocation allocation
)
    : image(std::move(image)),
      view(std::move(view)),
      format(format),
      extent(extent),
      aspect(aspect),
      currentLayout(vk::ImageLayout::eUndefined),
      currentStage(vk::PipelineStageFlagBits2::eNone),
      currentAccess(vk::AccessFlagBits2::eNone),
      allocator(allocator),
      allocation(allocation) {}

AllocatedImage::AllocatedImage(AllocatedImage&& other) noexcept
    : image(std::move(other.image)),
      view(std::move(other.view)),
      format(other.format),
      extent(other.extent),
      aspect(other.aspect),
      currentLayout(other.currentLayout),
      currentStage(other.currentStage),
      currentAccess(other.currentAccess),
      allocator(other.allocator),
      allocation(other.allocation) {
    other.format = vk::Format::eUndefined;
    other.extent = vk::Extent3D{};
    other.currentLayout = vk::ImageLayout::eUndefined;
    other.currentStage = vk::PipelineStageFlagBits2::eNone;
    other.currentAccess = vk::AccessFlagBits2::eNone;
    other.allocator = nullptr;
    other.allocation = nullptr;
}

AllocatedImage& AllocatedImage::operator=(AllocatedImage&& other) noexcept {
    if (this != &other) {
        image = std::move(other.image);
        view = std::move(other.view);
        format = other.format;
        extent = other.extent;
        aspect = other.aspect;
        currentLayout = other.currentLayout;
        currentStage = other.currentStage;
        currentAccess = other.currentAccess;
        allocator = other.allocator;
        allocation = other.allocation;

        other.format = vk::Format::eUndefined;
        other.extent = vk::Extent3D{};
        other.aspect = vk::ImageAspectFlagBits::eNone;
        other.currentLayout = vk::ImageLayout::eUndefined;
        other.currentStage = vk::PipelineStageFlagBits2::eNone;
        other.currentAccess = vk::AccessFlagBits2::eNone;
        other.allocator = nullptr;
        other.allocation = nullptr;
    }
    return *this;
}

void AllocatedImage::barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) {
    vkhelper::createImagePipelineBarrier(cmd, *image, aspect, currentStage, currentAccess, nextStage, nextAccess, currentLayout);
    currentLayout = currentLayout;
    currentStage = nextStage;
    currentAccess = nextAccess;
}

void AllocatedImage::transition(vk::CommandBuffer cmd, vk::ImageLayout nextLayout, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) {
    vkhelper::transitionImage(cmd, *image, aspect, currentLayout, currentStage, currentAccess, nextLayout, nextStage, nextAccess);
    currentLayout = nextLayout;
    currentStage = nextStage;
    currentAccess = nextAccess;
}

void AllocatedImage::cleanup() {
    if (allocator == nullptr) {
        return;
    }  // If destroying a moved AllocatedImage
    image.clear();
    view.clear();
    vmaFreeMemory(*allocator, allocation);

    image = nullptr;
    view = nullptr;
    allocator = nullptr;
    allocation = nullptr;
    format = {};
    extent = vk::Extent3D{};
}

AllocatedImage::~AllocatedImage() { cleanup(); }

AllocatedBuffer::AllocatedBuffer()
    : buffer(nullptr),
      address(std::nullopt),
      allocator(nullptr),
      allocation(nullptr),
      info({}),
      currentStage(vk::PipelineStageFlagBits2::eNone),
      currentAccess(vk::AccessFlagBits2::eNone) {}

AllocatedBuffer::AllocatedBuffer(
    vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator* allocator, VmaAllocation allocation, VmaAllocationInfo info
)
    : buffer(std::move(buffer)),
      address(address),
      allocator(allocator),
      allocation(allocation),
      info(info),
      currentStage(vk::PipelineStageFlagBits2::eNone),
      currentAccess(vk::AccessFlagBits2::eNone) {}

AllocatedBuffer::AllocatedBuffer(AllocatedBuffer&& other) noexcept
    : buffer(std::move(other.buffer)),
      address(other.address),
      allocator(other.allocator),
      allocation(other.allocation),
      info(other.info),
      currentStage(other.currentStage),
      currentAccess(other.currentAccess) {
    other.address = std::nullopt;
    other.allocator = nullptr;
    other.allocation = nullptr;
    other.info = {};
    other.currentStage = vk::PipelineStageFlagBits2::eNone;
    other.currentAccess = vk::AccessFlagBits2::eNone;
}

AllocatedBuffer& AllocatedBuffer::operator=(AllocatedBuffer&& other) noexcept {
    if (this != &other) {
        buffer = std::move(other.buffer);
        address = other.address;
        allocator = other.allocator;
        allocation = other.allocation;
        info = other.info;
        currentStage = other.currentStage;
        currentAccess = other.currentAccess;

        other.address = std::nullopt;
        other.allocator = nullptr;
        other.allocation = nullptr;
        other.info = {};
        other.currentStage = vk::PipelineStageFlagBits2::eNone;
        other.currentAccess = vk::AccessFlagBits2::eNone;
    }
    return *this;
}

void AllocatedBuffer::barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess) {
    vkhelper::createBufferPipelineBarrier(cmd, buffer, currentStage, currentAccess, nextStage, nextAccess);
    currentStage = nextStage;
    currentAccess = nextAccess;
}

void AllocatedBuffer::cleanup() {
    if (allocator == nullptr) {
        return;
    }  // If destroying a moved AllocatedBuffer
    buffer.clear();
    vmaFreeMemory(*allocator, allocation);

    buffer = nullptr;
    allocator = nullptr;
    allocation = nullptr;
    info = {};
    currentStage = vk::PipelineStageFlagBits2::eNone;
    currentAccess = vk::AccessFlagBits2::eNone;
}

AllocatedBuffer::~AllocatedBuffer() { cleanup(); }

RendererResources::RendererResources(Renderer* renderer) : mRenderer(renderer), mColorClearValue(CLEAR_COLOR) {}

void RendererResources::initStaging() {
    mImageStagingBuffer = createStagingBuffer(MAX_IMAGE_SIZE);
    mRenderer->mCore.labelResourceDebug(mImageStagingBuffer.buffer, "ImageStagingBuffer");
    LOG_INFO(mRenderer->mLogger, "Image Staging Buffer Created");

    mMeshStagingBuffer = createStagingBuffer(MESH_VERTEX_BUFFER_SIZE + MESH_INDEX_BUFFER_SIZE);
    mRenderer->mCore.labelResourceDebug(mMeshStagingBuffer.buffer, "MeshStagingBuffer");
    LOG_INFO(mRenderer->mLogger, "Mesh Staging Buffer Created");

    mMaterialConstantsStagingBuffer = createStagingBuffer(MAX_MATERIALS * sizeof(MaterialConstants));
    mRenderer->mCore.labelResourceDebug(mMaterialConstantsStagingBuffer.buffer, "MaterialConstantsStagingBuffer");
    LOG_INFO(mRenderer->mLogger, "Material Constants Staging Buffer Created");

    mNodeTransformsStagingBuffer = createStagingBuffer(MAX_NODES * sizeof(glm::mat4));
    mRenderer->mCore.labelResourceDebug(mNodeTransformsStagingBuffer.buffer, "NodeTransformsStagingBuffer");
    LOG_INFO(mRenderer->mLogger, "Node Transforms Staging Buffer Created");

    mBoundsStagingBuffer = createStagingBuffer(MAX_MESHES * sizeof(AABB));
    mRenderer->mCore.labelResourceDebug(mBoundsStagingBuffer.buffer, "BoundsStagingBuffer");
    LOG_INFO(mRenderer->mLogger, "Bounds Staging Buffer Created");
}

void RendererResources::initDefaultImages() {
    // Colour data interpreted as little endian
    constexpr u32 white = std::byteswap(0xFFFFFFFF);
    mDefaultImages.try_emplace(DefaultImage::White, createImage(&white, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled));
    constexpr u32 grey = std::byteswap(0xAAAAAAFF);
    mDefaultImages.try_emplace(DefaultImage::Grey, createImage(&grey, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled));
    constexpr u32 black = std::byteswap(0x000000FF);
    mDefaultImages.try_emplace(DefaultImage::Black, createImage(&black, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled));
    constexpr u32 blue = std::byteswap(0x769DDBFF);
    mDefaultImages.try_emplace(DefaultImage::Blue, createImage(&blue, vk::Extent3D{1, 1, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled));
    std::array<u32, 16 * 16> pixels;
    for (u32 x = 0; x < 16; x++) {
        for (u32 y = 0; y < 16; y++) {
            constexpr u32 magenta = std::byteswap(0xFF00FFFF);
            pixels[static_cast<std::array<u32, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    mDefaultImages.try_emplace(
        DefaultImage::Checkerboard, createImage(pixels.data(), vk::Extent3D{16, 16, 1}, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled)
    );

    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::White).image, "DefaultWhiteImage");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::White).view, "DefaultWhiteImageView");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Grey).image, "DefaultGreyImage");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Grey).view, "DefaultGreyImageView");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Black).image, "DefaultBlackImage");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Black).view, "DefaultBlackImageView");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Blue).image, "DefaultBlueImage");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Blue).view, "DefaultBlueImageView");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Checkerboard).image, "DefaultCheckboardImage");
    mRenderer->mCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Checkerboard).view, "DefaultCheckboardImageView");

    LOG_INFO(mRenderer->mLogger, "Default Images Created");
}

void RendererResources::initDefaultSampler() {
    vk::SamplerCreateInfo defaultSamplerCreateInfo;
    defaultSamplerCreateInfo.magFilter = vk::Filter::eLinear;
    defaultSamplerCreateInfo.minFilter = vk::Filter::eLinear;
    defaultSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    defaultSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    defaultSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    defaultSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;

    SamplerOptions defaultSamplerOptions;
    mSamplersCache.try_emplace(defaultSamplerOptions, mRenderer->mCore.mDevice.createSampler(defaultSamplerCreateInfo));

    LOG_INFO(mRenderer->mLogger, "Default Sampler Created");
}

vk::Sampler RendererResources::getSampler(vk::SamplerCreateInfo samplerCreateInfo) {
    SamplerOptions samplerOptions(samplerCreateInfo);
    mSamplersCache.try_emplace(samplerOptions, mRenderer->mCore.mDevice, samplerCreateInfo);
    if (auto it = mSamplersCache.find(samplerOptions); it != mSamplersCache.end()) {
        return *it->second;
    }

    assert(false);
    return nullptr;
}

vk::ShaderModule RendererResources::getShader(std::filesystem::path shaderPath) {
    auto shaderFileName = shaderPath.filename().string();
    if (auto it = mShadersCache.find(shaderFileName); it != mShadersCache.end()) {
        return *it->second;
    }

    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    const size_t fileSize = file.tellg();
    std::vector<u32> buffer(fileSize / sizeof(u32));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    // Load whole file into buffer
    file.close();

    vk::ShaderModuleCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.pNext = nullptr;
    shaderCreateInfo.codeSize = buffer.size() * sizeof(u32);
    shaderCreateInfo.pCode = buffer.data();

    mShadersCache.try_emplace(shaderFileName, mRenderer->mCore.mDevice, shaderCreateInfo);
    return *mShadersCache.at(shaderFileName);
}

AllocatedBuffer RendererResources::createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage) const {
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;
    auto bufferInfo1 = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VkBuffer tempBuffer;
    vmaCreateBuffer(mRenderer->mCore.mVmaAllocator, &bufferInfo1, &vmaAllocInfo, &tempBuffer, &newBuffer.allocation, &newBuffer.info);
    newBuffer.buffer = vk::raii::Buffer(mRenderer->mCore.mDevice, tempBuffer);
    newBuffer.allocator = &mRenderer->mCore.mVmaAllocator;
    if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        vk::BufferDeviceAddressInfo bufferDeviceAddressInfo = {};
        bufferDeviceAddressInfo.buffer = *newBuffer.buffer;
        newBuffer.address = mRenderer->mCore.mDevice.getBufferAddress(bufferDeviceAddressInfo);
    }

    return newBuffer;
}

AllocatedImage RendererResources::createImage(
    vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, bool multisampling, bool cubemap
) const {
    vk::ImageCreateInfo newImageCreateInfo = vkhelper::imageCreateInfo(format, usage, multisampling, extent);
    if (mipmapped) {
        newImageCreateInfo.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
    }
    if (cubemap) {
        newImageCreateInfo.arrayLayers = NUMBER_OF_CUBEMAP_FACES;
        newImageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    }
    VkImageCreateInfo newImageCreateInfo1 = newImageCreateInfo;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaAllocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal);

    AllocatedImage newImage;
    newImage.format = format;
    newImage.extent = extent;
    VkImage tempImage;
    vmaCreateImage(mRenderer->mCore.mVmaAllocator, &newImageCreateInfo1, &vmaAllocInfo, &tempImage, &newImage.allocation, nullptr);
    newImage.image = vk::raii::Image(mRenderer->mCore.mDevice, tempImage);
    newImage.allocator = &mRenderer->mCore.mVmaAllocator;

    vk::ImageAspectFlags aspectFlag = vk::ImageAspectFlagBits::eColor;
    if (format == vk::Format::eD32Sfloat || format == vk::Format::eD24UnormS8Uint) aspectFlag = vk::ImageAspectFlagBits::eDepth;
    newImage.aspect = aspectFlag;

    vk::ImageViewCreateInfo newImageViewCreateInfo = vkhelper::imageViewCreateInfo(format, *newImage.image, newImage.aspect);
    newImageViewCreateInfo.subresourceRange.levelCount = newImageCreateInfo.mipLevels;
    if (cubemap) {
        newImageViewCreateInfo.subresourceRange.layerCount = NUMBER_OF_CUBEMAP_FACES;
        newImageViewCreateInfo.viewType = vk::ImageViewType::eCube;
    }
    newImage.view = mRenderer->mCore.mDevice.createImageView(newImageViewCreateInfo);

    return newImage;
}

AllocatedImage RendererResources::createImage(
    const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, bool multisampling, bool cubemap
) const {
    u32 numFaces = cubemap ? NUMBER_OF_CUBEMAP_FACES : 1;

    u32 bytesPerTexel = vkhelper::getFormatTexelSize(format);
    const size_t faceSize = extent.depth * extent.width * extent.height * bytesPerTexel;
    const size_t dataSize = faceSize * numFaces;
    std::memcpy(mImageStagingBuffer.info.pMappedData, data, dataSize);

    AllocatedImage newImage =
        createImage(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipmapped, multisampling, cubemap);

    mRenderer->mImmSubmit.individualSubmit([&](Renderer* renderer, vk::CommandBuffer cmd) {
        newImage.transition(cmd, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);

        std::vector<vk::BufferImageCopy> copyRegions;
        copyRegions.reserve(numFaces);
        for (u32 face = 0; face < numFaces; face++) {
            vk::BufferImageCopy copyRegion;
            copyRegion.bufferOffset = face * faceSize;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = face;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = extent;
            copyRegions.push_back(copyRegion);
        }

        cmd.copyBufferToImage(*mImageStagingBuffer.buffer, *newImage.image, vk::ImageLayout::eTransferDstOptimal, copyRegions);

        if (mipmapped)
            vkhelper::generateMipmaps(cmd, newImage, cubemap);
        else {
            newImage.transition(cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2KHR::eFragmentShader, vk::AccessFlagBits2::eShaderRead);
        }
    });

    return newImage;
}

AllocatedBuffer RendererResources::createStagingBuffer(size_t allocSize) const {
    return createBuffer(allocSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void RendererResources::cleanup() {
    mBoundsStagingBuffer.cleanup();
    LOG_INFO(mRenderer->mLogger, "Bounds Staging Buffer Destroyed");
    mNodeTransformsStagingBuffer.cleanup();
    LOG_INFO(mRenderer->mLogger, "Node Transforms Staging Buffer Destroyed");
    mImageStagingBuffer.cleanup();
    LOG_INFO(mRenderer->mLogger, "Image Staging Buffer Destroyed");
    mMeshStagingBuffer.cleanup();
    LOG_INFO(mRenderer->mLogger, "Mesh Staging Buffer Destroyed");
    mMaterialConstantsStagingBuffer.cleanup();
    LOG_INFO(mRenderer->mLogger, "Material Constants Staging Buffer Destroyed");
    mDefaultImages.clear();
    LOG_INFO(mRenderer->mLogger, "Default Images Destroyed");
    mSamplersCache.clear();
    LOG_INFO(mRenderer->mLogger, "All Samplers Destroyed");
    mShadersCache.clear();
    LOG_INFO(mRenderer->mLogger, "All Shaders Destroyed");
}

RendererResources::RendererResources(RendererResources&& other) noexcept
    : mRenderer(std::move(other.mRenderer)),
      mImageStagingBuffer(std::move(other.mImageStagingBuffer)),
      mMeshStagingBuffer(std::move(other.mMeshStagingBuffer)),
      mDefaultImages(std::move(other.mDefaultImages)),
      mSamplersCache(std::move(other.mSamplersCache)),
      mShadersCache(std::move(other.mShadersCache)) {}

RendererResources& RendererResources::operator=(RendererResources&& other) noexcept {
    if (this != &other) {
        mRenderer = std::move(other.mRenderer);
        mImageStagingBuffer = std::move(other.mImageStagingBuffer);
        mMeshStagingBuffer = std::move(other.mMeshStagingBuffer);
        mDefaultImages = std::move(other.mDefaultImages);
        mSamplersCache = std::move(other.mSamplersCache);
        mShadersCache = std::move(other.mShadersCache);
    }
    return *this;
}
