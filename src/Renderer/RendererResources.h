#pragma once

#include <Data/Material.h>
#include <vk_mem_alloc.h>

#include <vulkan/vulkan_raii.hpp>

struct GLTFModel;
struct Mesh;
struct Vertex;

enum class DefaultImage { White, Grey, Black, Blue, Checkerboard };

class Renderer;

struct AllocatedImage {
    vk::raii::Image image;
    vk::raii::ImageView view;
    vk::Format format;
    vk::Extent3D extent;
    vk::ImageAspectFlags aspect;
    VmaAllocator* allocator;
    VmaAllocation allocation;

    vk::ImageLayout currentLayout;
    vk::PipelineStageFlagBits2 currentStage;
    vk::AccessFlags2 currentAccess;

    AllocatedImage();
    AllocatedImage(
        vk::raii::Image image, vk::raii::ImageView view, vk::Format format, vk::Extent3D extent, vk::ImageAspectFlags aspect, VmaAllocator* allocator,
        VmaAllocation allocation
    );

    AllocatedImage(AllocatedImage&& other) noexcept;
    AllocatedImage& operator=(AllocatedImage&& other) noexcept;

    AllocatedImage(const AllocatedImage&) = delete;
    AllocatedImage& operator=(const AllocatedImage&) = delete;

    void barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess);
    void transition(vk::CommandBuffer cmd, vk::ImageLayout nexLayout, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess);

    void cleanup();
    ~AllocatedImage();
};

struct AllocatedBuffer {
    vk::raii::Buffer buffer;
    std::optional<vk::DeviceAddress> address;
    VmaAllocator* allocator;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    vk::BufferUsageFlags usage;
    VmaAllocationCreateFlags flags;
    u32 size;
    vk::PipelineStageFlagBits2 currentStage;
    vk::AccessFlags2 currentAccess;

    AllocatedBuffer();
    AllocatedBuffer(
        vk::raii::Buffer buffer, std::optional<vk::DeviceAddress> address, VmaAllocator* allocator, VmaAllocation allocation, VmaAllocationInfo info,
        vk::BufferUsageFlagBits usage, VmaAllocationCreateFlagBits flags, u32 size
    );

    AllocatedBuffer(AllocatedBuffer&& other) noexcept;
    AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept;

    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

    void barrier(vk::CommandBuffer cmd, vk::PipelineStageFlagBits2 nextStage, vk::AccessFlags2 nextAccess);
    void resize(vk::CommandBuffer cmd, Renderer* renderer, u32 newSize);

    void cleanup();
    ~AllocatedBuffer();
};

struct SamplerOptions {
    vk::Filter magFilter;
    vk::Filter minFilter;
    vk::SamplerMipmapMode mipmapMode;
    vk::SamplerAddressMode addressModeU;
    vk::SamplerAddressMode addressModeV;
    vk::SamplerAddressMode addressModeW;
    const void* pNext;

    SamplerOptions()
        : magFilter(vk::Filter::eLinear),
          minFilter(vk::Filter::eLinear),
          mipmapMode(vk::SamplerMipmapMode::eLinear),
          addressModeU(vk::SamplerAddressMode::eRepeat),
          addressModeV(vk::SamplerAddressMode::eRepeat),
          addressModeW(vk::SamplerAddressMode::eRepeat),
          pNext(nullptr) {}

    SamplerOptions(vk::SamplerCreateInfo samplerCreateInfo)
        : magFilter(samplerCreateInfo.magFilter),
          minFilter(samplerCreateInfo.minFilter),
          mipmapMode(samplerCreateInfo.mipmapMode),
          addressModeU(samplerCreateInfo.addressModeU),
          addressModeV(samplerCreateInfo.addressModeV),
          addressModeW(samplerCreateInfo.addressModeW),
          pNext(samplerCreateInfo.pNext) {}

    bool operator==(const SamplerOptions& other) const {
        return (
            magFilter == other.magFilter && minFilter == other.minFilter && mipmapMode == other.mipmapMode && addressModeU == other.addressModeU &&
            addressModeV == other.addressModeV && addressModeW == other.addressModeW && pNext == other.pNext
        );
    }
};

template <>
struct std::hash<SamplerOptions> {
    // Compute individual hash values for strings
    // Combine them using XOR and bit shifting
    std::size_t operator()(const SamplerOptions& k) const {
        std::size_t seed = 0;
        hashCombine(seed, static_cast<u32>(k.magFilter));
        hashCombine(seed, static_cast<u32>(k.minFilter));
        hashCombine(seed, static_cast<u32>(k.mipmapMode));
        hashCombine(seed, static_cast<u32>(k.addressModeU));
        hashCombine(seed, static_cast<u32>(k.addressModeV));
        hashCombine(seed, static_cast<u32>(k.addressModeW));
        return seed;
    }

    static void hashCombine(std::size_t seed, std::size_t value) {
        std::hash<std::size_t> hasher;
        seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
};

class RendererResources {
    Renderer* mRenderer;
    AllocatedBuffer mImageStagingBuffer;

   public:
    AllocatedBuffer mMeshStagingBuffer;
    AllocatedBuffer mMaterialConstantsStagingBuffer;
    AllocatedBuffer mNodeTransformsStagingBuffer;
    AllocatedBuffer mBoundsStagingBuffer;

    std::unordered_map<DefaultImage, AllocatedImage> mDefaultImages;
    std::optional<vk::ClearValue> mColorClearValue;

    std::unordered_map<SamplerOptions, vk::raii::Sampler> mSamplersCache;
    std::unordered_map<std::string, vk::raii::ShaderModule> mShadersCache;

    RendererResources(Renderer* renderer);

    void initStaging();
    void initDefaultImages();
    void initDefaultSampler();

    vk::Sampler getSampler(
        vk::SamplerCreateInfo samplerCreateInfo = vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear)
    );
    vk::ShaderModule getShader(std::filesystem::path shaderFileName);

    AllocatedBuffer createBuffer(size_t allocSize, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags = 0) const;
    AllocatedBuffer createStagingBuffer(size_t allocSize) const;

    AllocatedImage createImage(
        vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, bool multisampling = false, bool cubemap = false
    ) const;
    AllocatedImage createImage(
        const void* data, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, bool mipmapped = false, bool multisampling = false,
        bool cubemap = false
    ) const;

    void cleanup();

    RendererResources(RendererResources&& other) noexcept;
    RendererResources& operator=(RendererResources&& other) noexcept;

    RendererResources(const RendererResources&) = delete;
    RendererResources& operator=(const RendererResources&) = delete;
};
