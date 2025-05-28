#include <Renderer/RendererResources.h>
#include <Renderer/Renderer.h>
#include <Utils/Descriptor.h>
#include <Utils/Helper.h>

#include <fmt/core.h>

#include <bit>

RendererResources::RendererResources(Renderer* renderer) :
	mRenderer(renderer),
	mDefaultSampler(nullptr),
	mDefaultColorClearValue(CLEAR_COLOR)
{
}

void RendererResources::init()
{
	initStaging();
	initDefault();
}

void RendererResources::initStaging()
{
	mImageStagingBuffer = std::move(createStagingBuffer(MAX_IMAGE_SIZE));
	mMeshStagingBuffer = std::move(createStagingBuffer(DEFAULT_VERTEX_BUFFER_SIZE + DEFAULT_INDEX_BUFFER_SIZE));
	mMaterialConstantsStagingBuffer = std::move(createStagingBuffer(MAX_MATERIALS * sizeof(MaterialConstants)));
	mInstancesStagingBuffer = std::move(createStagingBuffer(MAX_INSTANCES * sizeof(InstanceData)));
}

void RendererResources::initDefault()
{
	// Colour data interpreted as little endian
	constexpr uint32_t white = std::byteswap(0xFFFFFFFF);
	mDefaultImages.emplace(DefaultImage::White, createImage(&white, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, false, false, false));
	constexpr uint32_t grey = std::byteswap(0xAAAAAAFF);
	mDefaultImages.emplace(DefaultImage::Grey, createImage(&grey, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, false, false, false));
	constexpr uint32_t black = std::byteswap(0x000000FF);
	mDefaultImages.emplace(DefaultImage::Black, createImage(&black, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, false, false, false));
	constexpr uint32_t blue = std::byteswap(0x769DDBFF);
	mDefaultImages.emplace(DefaultImage::Blue, createImage(&blue, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, false, false, false));
	std::array<uint32_t, 16 * 16> pixels;
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			constexpr uint32_t magenta = std::byteswap(0xFF00FFFF);
			pixels[static_cast<std::array<uint32_t, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	mDefaultImages.emplace(DefaultImage::Checkerboard, createImage(pixels.data(), vk::Extent3D{ 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, false, false, false));

	vk::SamplerCreateInfo sampl;
	sampl.magFilter = vk::Filter::eLinear;
	sampl.minFilter = vk::Filter::eLinear;
	mDefaultSampler = mRenderer->mRendererCore.mDevice.createSampler(sampl);
}

AllocatedBuffer RendererResources::createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

AllocatedImage RendererResources::createImage(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, bool multisampling, bool cubemap)
{
	vk::ImageCreateInfo newImageCreateInfo = vkhelper::imageCreateInfo(format, usage, multisampling, extent);
	if (mipmapped) {
		newImageCreateInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
	}
	if (cubemap) {
		newImageCreateInfo.arrayLayers = NUMBER_OF_CUBEMAP_FACES;
		newImageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
	}
	VkImageCreateInfo newImageCreateInfo1 = static_cast<vk::ImageCreateInfo>(newImageCreateInfo);

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaAllocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(vk::MemoryPropertyFlagBits::eDeviceLocal);

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
	vk::ImageViewCreateInfo newImageViewCreateInfo = vkhelper::imageViewCreateInfo(format, *newImage.image, aspectFlag);
	newImageViewCreateInfo.subresourceRange.levelCount = newImageCreateInfo.mipLevels;
	if (cubemap) {
		newImageViewCreateInfo.subresourceRange.layerCount = NUMBER_OF_CUBEMAP_FACES;
		newImageViewCreateInfo.viewType = vk::ImageViewType::eCube;
	}

	newImage.imageView = mRenderer->mRendererCore.mDevice.createImageView(newImageViewCreateInfo);

	return newImage;
}

AllocatedImage RendererResources::createImage(const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped, bool multisampling, bool cubemap)
{
	int numFaces = cubemap ? NUMBER_OF_CUBEMAP_FACES : 1;

	int bytesPerTexel = vkhelper::getFormatTexelSize(format);
	const size_t faceSize = extent.depth * extent.width * extent.height * bytesPerTexel;
	const size_t dataSize = faceSize * numFaces;
	std::memcpy(mImageStagingBuffer.info.pMappedData, data, dataSize);

	AllocatedImage newImage = createImage(extent, format, usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, mipmapped, multisampling, cubemap);

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		vkhelper::transitionImage(*cmd, *newImage.image,
			vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

		std::vector<vk::BufferImageCopy> copyRegions;
		copyRegions.reserve(numFaces);
		for (int face = 0; face < numFaces; face++) {
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
			vkhelper::generateMipmaps(*cmd, *newImage.image, vk::Extent2D{ newImage.imageExtent.width, newImage.imageExtent.height }, cubemap);
		else
			vkhelper::transitionImage(*cmd, *newImage.image,
				vk::PipelineStageFlagBits2KHR::eTransfer, vk::AccessFlagBits2::eTransferWrite,
				vk::PipelineStageFlagBits2KHR::eFragmentShader, vk::AccessFlagBits2::eShaderRead,
				vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		});

	return newImage;
}

AllocatedBuffer RendererResources::createStagingBuffer(size_t allocSize)
{
	return createBuffer(allocSize, vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void RendererResources::cleanup()
{
	mImageStagingBuffer.cleanup();
	mMeshStagingBuffer.cleanup();
	mMaterialConstantsStagingBuffer.cleanup();
	mInstancesStagingBuffer.cleanup();
	mDefaultImages.clear();
	mDefaultSampler.clear();
}

RendererResources::RendererResources(RendererResources&& other) noexcept :
	mRenderer(std::exchange(other.mRenderer, nullptr)),
	mImageStagingBuffer(std::move(other.mImageStagingBuffer)),
	mMeshStagingBuffer(std::move(other.mMeshStagingBuffer)),
	mDefaultImages(std::move(other.mDefaultImages)),
	mDefaultSampler(std::move(other.mDefaultSampler))
{
}

RendererResources& RendererResources::operator=(RendererResources&& other) noexcept {
	if (this != &other) {
		mRenderer = std::exchange(other.mRenderer, nullptr);
		mImageStagingBuffer = std::move(other.mImageStagingBuffer);
		mMeshStagingBuffer = std::move(other.mMeshStagingBuffer);
		mDefaultImages = std::move(other.mDefaultImages);
		mDefaultSampler = std::move(other.mDefaultSampler);
	}
	return *this;
}
