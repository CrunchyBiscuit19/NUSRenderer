#pragma once

#include <Data/Material.h>

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

struct GLTFModel;
struct Mesh;
struct Vertex;

enum class DefaultImage
{
	White,
	Grey,
	Black,
	Blue,
	Checkerboard
};

class Renderer;

struct AllocatedImage
{
	vk::raii::Image image;
	vk::raii::ImageView imageView;
	vk::Format imageFormat;
	vk::Extent3D imageExtent;
	VmaAllocator* allocator;
	VmaAllocation allocation;

	AllocatedImage() :
		image(nullptr),
		imageView(nullptr),
		imageFormat(vk::Format::eUndefined),
		imageExtent({0, 0, 0}),
		allocator(nullptr),
		allocation(nullptr)
	{
	}

	AllocatedImage(vk::raii::Image image, vk::raii::ImageView imageView, vk::Format imageFormat,
	               vk::Extent3D imageExtent, VmaAllocator* allocator, VmaAllocation allocation) :
		image(std::move(image)),
		imageView(std::move(imageView)),
		imageFormat(imageFormat),
		imageExtent(imageExtent),
		allocator(allocator),
		allocation(allocation)
	{
	}

	AllocatedImage(AllocatedImage&& other) noexcept :
		image(std::move(other.image)),
		imageView(std::move(other.imageView)),
		imageFormat(other.imageFormat),
		imageExtent(other.imageExtent),
		allocator(other.allocator),
		allocation(other.allocation)
	{
		other.allocator = nullptr;
		other.allocation = nullptr;
		other.imageFormat = vk::Format::eUndefined;
		other.imageExtent = vk::Extent3D{};
	}

	AllocatedImage& operator=(AllocatedImage&& other) noexcept
	{
		if (this != &other)
		{
			image = std::move(other.image);
			imageView = std::move(other.imageView);
			imageFormat = other.imageFormat;
			imageExtent = other.imageExtent;
			allocator = other.allocator;
			allocation = other.allocation;

			other.allocator = nullptr;
			other.allocation = nullptr;
			other.imageFormat = vk::Format::eUndefined;
			other.imageExtent = vk::Extent3D{};
		}
		return *this;
	}

	AllocatedImage(const AllocatedImage&) = delete;
	AllocatedImage& operator=(const AllocatedImage&) = delete;

	void cleanup()
	{
		if (allocator == nullptr) { return; } // If destroying a moved AllocatedImage
		image.clear();
		imageView.clear();
		vmaFreeMemory(*allocator, allocation);

		image = nullptr;
		imageView = nullptr;
		allocator = nullptr;
		allocation = nullptr;
		imageFormat = {};
		imageExtent = vk::Extent3D{};
	}

	~AllocatedImage()
	{
		cleanup();
	}
};

struct AllocatedBuffer
{
	vk::raii::Buffer buffer;
	VmaAllocator* allocator;
	VmaAllocation allocation;
	VmaAllocationInfo info;

	AllocatedBuffer() :
		buffer(nullptr),
		allocator(nullptr),
		allocation(nullptr),
		info({})
	{
	}

	AllocatedBuffer(vk::raii::Buffer buffer, VmaAllocator* allocator, VmaAllocation allocation,
	                VmaAllocationInfo info) :
		buffer(std::move(buffer)),
		allocator(allocator),
		allocation(allocation),
		info(info)
	{
	}

	AllocatedBuffer(AllocatedBuffer&& other) noexcept :
		buffer(std::move(other.buffer)),
		allocator(other.allocator),
		allocation(other.allocation),
		info(other.info)
	{
		other.allocator = nullptr;
		other.allocation = nullptr;
		other.info = {};
	}

	AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept
	{
		if (this != &other)
		{
			buffer = std::move(other.buffer);
			allocator = other.allocator;
			allocation = other.allocation;
			info = other.info;

			other.allocator = nullptr;
			other.allocation = nullptr;
			other.info = {};
		}
		return *this;
	}

	AllocatedBuffer(const AllocatedBuffer&) = delete;
	AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

	void cleanup()
	{
		if (allocator == nullptr) { return; } // If destroying a moved AllocatedBuffer
		buffer.clear();
		vmaFreeMemory(*allocator, allocation);

		buffer = nullptr;
		allocator = nullptr;
		allocation = nullptr;
		info = {};
	}

	~AllocatedBuffer()
	{
		cleanup();
	}
};

struct AddressedBuffer : AllocatedBuffer
{
	vk::DeviceAddress address{};

	AddressedBuffer() = default;

	AddressedBuffer(AllocatedBuffer&& other) noexcept
		: AllocatedBuffer(std::move(other))
	{
	}

	AddressedBuffer& operator=(AllocatedBuffer&& other) noexcept
	{
		static_cast<AllocatedBuffer&>(*this) = std::move(other);
		return *this;
	}

	AddressedBuffer(const AddressedBuffer&) = delete;
	AddressedBuffer& operator=(const AddressedBuffer&) = delete;
};

struct SamplerOptions
{
	vk::Filter magFilter;
	vk::Filter minFilter;
	vk::SamplerMipmapMode mipmapMode;
	vk::SamplerAddressMode addressModeU;
	vk::SamplerAddressMode addressModeV;
	vk::SamplerAddressMode addressModeW;

	SamplerOptions() :
		magFilter(vk::Filter::eLinear),
		minFilter(vk::Filter::eLinear),
		mipmapMode(vk::SamplerMipmapMode::eLinear),
		addressModeU(vk::SamplerAddressMode::eRepeat),
		addressModeV(vk::SamplerAddressMode::eRepeat),
		addressModeW(vk::SamplerAddressMode::eRepeat)
	{
	}

	SamplerOptions(vk::SamplerCreateInfo samplerCreateInfo) :
		magFilter(samplerCreateInfo.magFilter),
		minFilter(samplerCreateInfo.minFilter),
		mipmapMode(samplerCreateInfo.mipmapMode),
		addressModeU(samplerCreateInfo.addressModeU),
		addressModeV(samplerCreateInfo.addressModeV),
		addressModeW(samplerCreateInfo.addressModeW)
	{
	}

	bool operator==(const SamplerOptions& other) const
	{
		return (
			magFilter == other.magFilter &&
			minFilter == other.minFilter &&
			mipmapMode == other.mipmapMode &&
			addressModeU == other.addressModeU &&
			addressModeV == other.addressModeV &&
			addressModeW == other.addressModeW
		);
	}
};

template <>
struct std::hash<SamplerOptions>
{
	// Compute individual hash values for strings
	// Combine them using XOR and bit shifting
	std::size_t operator()(const SamplerOptions& k) const
	{
		std::size_t seed = 0;
		hashCombine(seed, static_cast<uint32_t>(k.magFilter));
		hashCombine(seed, static_cast<uint32_t>(k.minFilter));
		hashCombine(seed, static_cast<uint32_t>(k.mipmapMode));
		hashCombine(seed, static_cast<uint32_t>(k.addressModeU));
		hashCombine(seed, static_cast<uint32_t>(k.addressModeV));
		hashCombine(seed, static_cast<uint32_t>(k.addressModeW));
		return seed;
	}

	static void hashCombine(std::size_t seed, std::size_t value)
	{
		std::hash<std::size_t> hasher;
		seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
};

class RendererResources
{
	Renderer* mRenderer;
	AllocatedBuffer mImageStagingBuffer;

public:
	AllocatedBuffer mMeshStagingBuffer;
	AllocatedBuffer mMaterialConstantsStagingBuffer;
	AllocatedBuffer mNodeTransformsStagingBuffer;

	std::unordered_map<DefaultImage, AllocatedImage> mDefaultImages;
	std::optional<vk::ClearValue> mColorClearValue;

	std::unordered_map<SamplerOptions, vk::raii::Sampler> mSamplersCache;
	std::unordered_map<std::string, vk::raii::ShaderModule> mShadersCache;

	RendererResources(Renderer* renderer);

	void init();

	void initStaging();
	void initDefaultImages();
	void initDefaultSampler();

	vk::Sampler getSampler(
		vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear,
		                                                                vk::SamplerMipmapMode::eLinear));
	vk::ShaderModule getShader(std::filesystem::path shaderFileName);

	AllocatedBuffer createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;
	AllocatedBuffer createStagingBuffer(size_t allocSize) const;

	AllocatedImage createImage(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage,
	                           bool mipmapped = false, bool multisampling = false, bool cubemap = false) const;
	AllocatedImage createImage(const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage,
	                           bool mipmapped = false, bool multisampling = false, bool cubemap = false) const;

	void cleanup();

	RendererResources(RendererResources&& other) noexcept;
	RendererResources& operator=(RendererResources&& other) noexcept;

	RendererResources(const RendererResources&) = delete;
	RendererResources& operator=(const RendererResources&) = delete;
};
