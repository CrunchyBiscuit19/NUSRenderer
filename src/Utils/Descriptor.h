#pragma once

#include <Utils/Types.h>

#include <deque>
#include <vulkan/vulkan_raii.hpp>

class Renderer;

struct DescriptorLayoutBuilder {
    std::vector<vk::DescriptorSetLayoutBinding> mBindings;

    void addBinding(u32 binding, vk::DescriptorType type, u32 count = 1);
    void clear();
    vk::raii::DescriptorSetLayout build(vk::raii::Device& device, vk::ShaderStageFlags shaderStages, bool useBindless = false);
};

struct DescriptorAllocatorGrowable {
   private:
    Renderer* mRenderer;

   public:
    struct DescriptorTypeRatio {
        vk::DescriptorType type;
        u32 amountPerSet;
    };

    DescriptorAllocatorGrowable(Renderer* renderer);

    void init(u32 initialSets, std::vector<DescriptorTypeRatio>& poolRatios);

    void clearPools();
    void destroyPools();
    vk::raii::DescriptorSet allocate(vk::DescriptorSetLayout layout, bool useBindless = false);

    void cleanup();

   private:
    vk::raii::DescriptorPool getPool();
    vk::raii::DescriptorPool createPool(u32 setCount, std::vector<DescriptorTypeRatio>& poolRatios);

    std::vector<DescriptorTypeRatio> mRatios;
    std::vector<vk::raii::DescriptorPool> mFullPools;
    std::vector<vk::raii::DescriptorPool> mReadyPools;
    u32 mSetsPerPool = 0;
};

struct DescriptorSetBinder {
    std::deque<vk::DescriptorImageInfo> mImageInfos;  // Deques are guaranteed to keep pointers to elements valid
    std::deque<vk::DescriptorBufferInfo> mBufferInfos;
    std::vector<vk::WriteDescriptorSet> mWrites;

    void bindImage(u32 binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);
    void bindImageArray(u32 binding, u32 arrayIndex, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type);
    void bindSampler(u32 binding, vk::Sampler sampler, vk::DescriptorType type);
    void bindBuffer(u32 binding, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);

    void clear();
    void updateSetBindings(const vk::raii::Device& device, vk::DescriptorSet set);
};
