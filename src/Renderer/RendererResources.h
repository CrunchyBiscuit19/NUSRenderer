#pragma once

#include <Data/Material.h>
#include <Data/Instance.h>

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <map>

struct GLTFModel;
struct Mesh;
struct Vertex;

enum class DefaultImage {
	White,
	Grey,
	Black,
	Blue,
	Checkerboard
};

class Renderer;

struct AllocatedImage {
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
		imageExtent({ 0, 0, 0 }),
		allocator(nullptr),
		allocation(nullptr)
	{
	}

	AllocatedImage(vk::raii::Image image, vk::raii::ImageView imageView, vk::Format imageFormat, vk::Extent3D imageExtent, VmaAllocator* allocator, VmaAllocation allocation) :
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

	AllocatedImage& operator=(AllocatedImage&& other) noexcept {
		if (this != &other) {
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

	void cleanup() {
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

	~AllocatedImage() {
		cleanup();
	}
};

struct AllocatedBuffer {
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

	AllocatedBuffer(vk::raii::Buffer buffer, VmaAllocator* allocator, VmaAllocation allocation, VmaAllocationInfo info) :
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

	AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept {
		if (this != &other) {
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

	void cleanup() {
		if (allocator == nullptr) { return; } // If destroying a moved AllocatedBuffer
		buffer.clear();
		vmaFreeMemory(*allocator, allocation);

		buffer = nullptr;
		allocator = nullptr;
		allocation = nullptr;
		info = {};
	}

	~AllocatedBuffer() {
		cleanup();
	}
};

class RendererResources {
private:
	Renderer* mRenderer;
	AllocatedBuffer mImageStagingBuffer;

public:
	AllocatedBuffer mMeshStagingBuffer;
	AllocatedBuffer mMaterialConstantsStagingBuffer;
	AllocatedBuffer mInstancesStagingBuffer;
	AllocatedBuffer mNodeTransformsStagingBuffer;

	std::unordered_map<DefaultImage, AllocatedImage> mDefaultImages;
	vk::raii::Sampler mDefaultSampler;
	std::optional<vk::ClearValue> mColorClearValue;

	RendererResources(Renderer* renderer);

	void init();

	void initStaging();
	void initDefault();

	AllocatedBuffer createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	AllocatedBuffer createStagingBuffer(size_t allocSize);

	AllocatedImage createImage(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, bool multisampling = false, bool cubemap = false);
	AllocatedImage createImage(const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, bool multisampling = false, bool cubemap = false);

	void cleanup();

	RendererResources(RendererResources&& other) noexcept;
	RendererResources& operator=(RendererResources&& other) noexcept;

	RendererResources(const RendererResources&) = delete;
	RendererResources& operator=(const RendererResources&) = delete;
};

