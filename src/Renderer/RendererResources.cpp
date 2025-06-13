#include <Renderer/RendererResources.h>
#include <Renderer/Renderer.h>
#include <Utils/Descriptor.h>
#include <Utils/Helper.h>

#include <fmt/core.h>

#include <bit>
#include <fstream>

RendererResources::RendererResources(Renderer* renderer) :
	mRenderer(renderer),
	mColorClearValue(CLEAR_COLOR)
{
}

void RendererResources::init()
{
	initStaging();
	initDefaultImages();
	initDefaultSampler();
}

void RendererResources::initStaging()
{
	mImageStagingBuffer = createStagingBuffer(MAX_IMAGE_SIZE);
	mRenderer->mRendererCore.labelResourceDebug(mImageStagingBuffer.buffer, "ImageStagingBuffer");
	LOG_INFO(mRenderer->mLogger, "Image Staging Buffer Created");

	mMeshStagingBuffer = createStagingBuffer(MESH_VERTEX_BUFFER_SIZE + MESH_INDEX_BUFFER_SIZE);
	mRenderer->mRendererCore.labelResourceDebug(mMeshStagingBuffer.buffer, "MeshStagingBuffer");
	LOG_INFO(mRenderer->mLogger, "Mesh Staging Buffer Created");

	mMaterialConstantsStagingBuffer = createStagingBuffer(MAX_MATERIALS * sizeof(MaterialConstants));
	mRenderer->mRendererCore.labelResourceDebug(mMaterialConstantsStagingBuffer.buffer, "MaterialConstantsStagingBuffer");
	LOG_INFO(mRenderer->mLogger, "Material Constants Staging Buffer Created");

	mInstancesStagingBuffer = createStagingBuffer(MAX_INSTANCES * sizeof(InstanceData));
	mRenderer->mRendererCore.labelResourceDebug(mInstancesStagingBuffer.buffer, "InstancesStagingBuffer");
	LOG_INFO(mRenderer->mLogger, "Instances Staging Buffer Created");

	mNodeTransformsStagingBuffer = createStagingBuffer(MAX_NODES * sizeof(glm::mat4));
	mRenderer->mRendererCore.labelResourceDebug(mNodeTransformsStagingBuffer.buffer, "NodeTransformsStagingBuffer");
	LOG_INFO(mRenderer->mLogger, "Node Transforms Staging Buffer Created");

}

void RendererResources::initDefaultImages()
{
	// Colour data interpreted as little endian
	constexpr uint32_t white = std::byteswap(0xFFFFFFFF);
	mDefaultImages.try_emplace(DefaultImage::White, createImage(&white, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
	constexpr uint32_t grey = std::byteswap(0xAAAAAAFF);
	mDefaultImages.try_emplace(DefaultImage::Grey, createImage(&grey, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
	constexpr uint32_t black = std::byteswap(0x000000FF);
	mDefaultImages.try_emplace(DefaultImage::Black, createImage(&black, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
	constexpr uint32_t blue = std::byteswap(0x769DDBFF);
	mDefaultImages.try_emplace(DefaultImage::Blue, createImage(&blue, vk::Extent3D{ 1, 1, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));
	std::array<uint32_t, 16 * 16> pixels;
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			constexpr uint32_t magenta = std::byteswap(0xFF00FFFF);
			pixels[static_cast<std::array<uint32_t, 256Ui64>::size_type>(y) * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	mDefaultImages.try_emplace(DefaultImage::Checkerboard, createImage(pixels.data(), vk::Extent3D{ 16, 16, 1 }, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled));

	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::White).image, "DefaultWhiteImage");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::White).imageView, "DefaultWhiteImageView");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Grey).image, "DefaultGreyImage");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Grey).imageView, "DefaultGreyImageView");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Black).image, "DefaultBlackImage");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Black).imageView, "DefaultBlackImageView");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Blue).image, "DefaultBlueImage");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Blue).imageView, "DefaultBlueImageView");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Checkerboard).image, "DefaultCheckboardImage");
	mRenderer->mRendererCore.labelResourceDebug(mDefaultImages.at(DefaultImage::Checkerboard).imageView, "DefaultCheckboardImageView");

	LOG_INFO(mRenderer->mLogger, "Default Images Created");
}

void RendererResources::initDefaultSampler()
{
	vk::SamplerCreateInfo defaultSamplerCreateInfo;
	defaultSamplerCreateInfo.magFilter = vk::Filter::eLinear;
	defaultSamplerCreateInfo.minFilter = vk::Filter::eLinear;
	defaultSamplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	defaultSamplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
	defaultSamplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
	defaultSamplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;	

	SamplerOptions defaultSamplerOptions;
	mSamplersCache.try_emplace(defaultSamplerOptions, mRenderer->mRendererCore.mDevice.createSampler(defaultSamplerCreateInfo));

	LOG_INFO(mRenderer->mLogger, "Default Sampler Created");
}

vk::Sampler RendererResources::getSampler(vk::SamplerCreateInfo samplerCreateInfo)
{
	SamplerOptions samplerOptions(samplerCreateInfo);
	mSamplersCache.try_emplace(samplerOptions, mRenderer->mRendererCore.mDevice, samplerCreateInfo);
	if (auto it = mSamplersCache.find(samplerOptions); it != mSamplersCache.end()) {
		return *it->second;
	}

	assert(false);
	return nullptr;
}

vk::ShaderModule RendererResources::getShader(std::filesystem::path shaderPath)
{
	auto shaderFileName = shaderPath.filename().string();
	if (auto it = mShadersCache.find(shaderFileName); it != mShadersCache.end()) {
		return *it->second;
	}

	std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
	const size_t fileSize = file.tellg();
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize)); // Load whole file into buffer
	file.close();

	vk::ShaderModuleCreateInfo shaderCreateInfo = {};
	shaderCreateInfo.pNext = nullptr;
	shaderCreateInfo.codeSize = buffer.size() * sizeof(uint32_t);
	shaderCreateInfo.pCode = buffer.data();

	mShadersCache.try_emplace(shaderFileName, mRenderer->mRendererCore.mDevice, shaderCreateInfo);
	return *mShadersCache.at(shaderFileName);
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

	mRenderer->mImmSubmit.individualSubmit([&](Renderer* renderer, vk::CommandBuffer cmd) {
		vkhelper::transitionImage(cmd, *newImage.image,
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
			vkhelper::generateMipmaps(cmd, *newImage.image, vk::Extent2D{ newImage.imageExtent.width, newImage.imageExtent.height }, cubemap);
		else
			vkhelper::transitionImage(cmd, *newImage.image,
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
	mNodeTransformsStagingBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Node Transforms Staging Buffer Destroyed");
	mImageStagingBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Image Staging Buffer Destroyed");
	mMeshStagingBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Mesh Staging Buffer Destroyed");
	mMaterialConstantsStagingBuffer.cleanup( );
	LOG_INFO(mRenderer->mLogger, "Material Constants Staging Buffer Destroyed");
	mInstancesStagingBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Instances Staging Buffer Destroyed");
	mDefaultImages.clear();
	LOG_INFO(mRenderer->mLogger, "Default Images Destroyed");
	mSamplersCache.clear();
	LOG_INFO(mRenderer->mLogger, "All Samplers Destroyed");
	mShadersCache.clear();
	LOG_INFO(mRenderer->mLogger, "All Shaders Destroyed");
}

RendererResources::RendererResources(RendererResources&& other) noexcept :
	mRenderer(std::exchange(other.mRenderer, nullptr)),
	mImageStagingBuffer(std::move(other.mImageStagingBuffer)),
	mMeshStagingBuffer(std::move(other.mMeshStagingBuffer)),
	mDefaultImages(std::move(other.mDefaultImages)),
	mSamplersCache(std::move(other.mSamplersCache)),
	mShadersCache(std::move(other.mShadersCache))
{
}

RendererResources& RendererResources::operator=(RendererResources&& other) noexcept {
	if (this != &other) {
		mRenderer = std::exchange(other.mRenderer, nullptr);
		mImageStagingBuffer = std::move(other.mImageStagingBuffer);
		mMeshStagingBuffer = std::move(other.mMeshStagingBuffer);
		mDefaultImages = std::move(other.mDefaultImages);
		mSamplersCache = std::move(other.mSamplersCache);
		mShadersCache = std::move(other.mShadersCache);
	}
	return *this;
}
