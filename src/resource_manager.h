#pragma once

#include <materials.h>
#include <instances.h>

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <map>

struct GLTFModel;
struct Mesh;
struct Vertex;

constexpr int NUMBER_OF_CUBEMAP_FACES = 6;

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
    {}

    AllocatedImage(vk::raii::Image image, vk::raii::ImageView imageView, vk::Format imageFormat, vk::Extent3D imageExtent, VmaAllocator* allocator, VmaAllocation allocation) :
		image(std::move(image)),
		imageView(std::move(imageView)),
		imageFormat(imageFormat),
		imageExtent(imageExtent),
		allocator(allocator),
		allocation(allocation)
    {}

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
        other.imageExtent = vk::Extent3D {};
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
            other.imageExtent = vk::Extent3D {};
        }
        return *this;
    }

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
		imageExtent = vk::Extent3D {};
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
	{}

	AllocatedBuffer(vk::raii::Buffer buffer, VmaAllocator* allocator, VmaAllocation allocation, VmaAllocationInfo info) :
		buffer(std::move(buffer)),
		allocator(allocator),
		allocation(allocation),
		info(info)
	{}

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

class ResourceManager {
private:
    Renderer* mRenderer;
    AllocatedBuffer mImageStagingBuffer;

public:
    AllocatedBuffer mMeshStagingBuffer;
    AllocatedBuffer mMaterialConstantsStagingBuffer;
    AllocatedBuffer mInstancesStagingBuffer;

    std::unordered_map<DefaultImage, AllocatedImage> mDefaultImages;
    vk::raii::Sampler mDefaultSampler;
    std::optional<vk::ClearValue> mDefaultColorClearValue;

    ResourceManager(Renderer* renderer);

    void init();

    void initStaging();
    void initDefault();

    AllocatedBuffer createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    AllocatedBuffer createStagingBuffer(size_t allocSize);

    AllocatedImage createImage(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, bool cubemap = false);
    AllocatedImage createImage(const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, bool cubemap = false);

	void cleanup();

    ResourceManager(ResourceManager&& other) noexcept;
    ResourceManager& operator=(ResourceManager&& other) noexcept;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
};

