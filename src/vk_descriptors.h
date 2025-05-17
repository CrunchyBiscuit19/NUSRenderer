#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <deque>
#include <span>

struct DescriptorBundle {
    vk::raii::DescriptorSet set;
    vk::raii::DescriptorSetLayout layout;

    DescriptorBundle() :
        set(nullptr),
        layout(nullptr)
    {
    };

    DescriptorBundle(vk::raii::DescriptorSet set, vk::raii::DescriptorSetLayout layout) :
        set(std::move(set)),
        layout(std::move(layout))
    {
    }

    DescriptorBundle(DescriptorBundle&& other) noexcept :
        set(std::move(other.set)),
        layout(std::move(other.layout))
    {
    }

    DescriptorBundle& operator=(DescriptorBundle&& other) noexcept {
        if (this != &other) {
            set = std::move(other.set);
            layout = std::move(other.layout);
        }
        return *this;
    }

    void cleanup() {
        set.clear();
        layout.clear();
    }

    DescriptorBundle(const DescriptorBundle&) = delete;
    DescriptorBundle& operator=(const DescriptorBundle&) = delete;
};

struct DescriptorLayoutBuilder {
    std::vector<vk::DescriptorSetLayoutBinding> mBindings;

    void addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count = 1);
    void clear();
    vk::raii::DescriptorSetLayout build(vk::raii::Device& device, vk::ShaderStageFlags shaderStages);
};

struct DescriptorAllocatorGrowable {
public:
    struct PoolSizeRatio {
        vk::DescriptorType type;
        float ratio;
    };

    void init(const vk::raii::Device& device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void cleanup();

    void clearPools();
    void destroyPools();
    vk::raii::DescriptorSet allocate(const vk::raii::Device& device, const vk::DescriptorSetLayout layout);

private:
    vk::raii::DescriptorPool getPool(const vk::raii::Device& device);
    static vk::raii::DescriptorPool createPool(const vk::raii::Device& device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> mRatios;
    std::vector<vk::raii::DescriptorPool> mFullPools;
    std::vector<vk::raii::DescriptorPool> mReadyPools;
    uint32_t mSetsPerPool = 0;
};

struct DescriptorWriter {
    std::deque<vk::DescriptorImageInfo> mImageInfos; // Deques are guaranteed to keep pointers to elements valid
    std::deque<vk::DescriptorBufferInfo> mBufferInfos;
    std::vector<vk::WriteDescriptorSet> mWrites;

    void writeImage(int binding, const vk::ImageView image, const vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);
    void writeImageArray(int binding, const vk::ImageView image, const vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type, uint32_t arrayIndex);
    void writeBuffer(int binding, const vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);

    void clear();
    void updateSet(const vk::raii::Device& device, const vk::DescriptorSet set);
};