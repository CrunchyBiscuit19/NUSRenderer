#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <deque>
#include <span>

class Renderer;

struct DescriptorLayoutBuilder {
    std::vector<vk::DescriptorSetLayoutBinding> mBindings;

    void addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count = 1);
    void clear();
    vk::raii::DescriptorSetLayout build(vk::raii::Device& device, vk::ShaderStageFlags shaderStages);
};

struct DescriptorAllocatorGrowable {
private:
    Renderer* mRenderer;

public:
    struct DescriptorTypeRatio {
        vk::DescriptorType type;
        int amountPerSet;
    };

    DescriptorAllocatorGrowable(Renderer* renderer);

    void init(uint32_t initialSets, std::vector<DescriptorTypeRatio>& poolRatios);

    void clearPools();
    void destroyPools();
    vk::raii::DescriptorSet allocate(const vk::DescriptorSetLayout layout);
    
    void cleanup();

private:
    vk::raii::DescriptorPool getPool();
    vk::raii::DescriptorPool createPool(uint32_t setCount, std::vector<DescriptorTypeRatio>& poolRatios);

    std::vector<DescriptorTypeRatio> mRatios;
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