#include <vk_descriptors.h>

void DescriptorLayoutBuilder::addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count)
{
    vk::DescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorType = type;
    newbind.descriptorCount = count;

    mBindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    mBindings.clear();
}

const vk::raii::DescriptorSetLayout DescriptorLayoutBuilder::build(vk::raii::Device& device, vk::ShaderStageFlags shaderStages, bool useBindless)
{
    for (auto& b : mBindings) {
        b.stageFlags |= shaderStages;
    }

    vk::DescriptorSetLayoutCreateInfo info{};
    info.pNext = nullptr;
    info.pBindings = mBindings.data();
    info.bindingCount = static_cast<uint32_t>(mBindings.size());
    //info.flags = 0;

    vk::DescriptorBindingFlags bindlessFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount | vk::DescriptorBindingFlagBits::eUpdateAfterBind;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo extended_info{};
    extended_info.pNext = nullptr;
    extended_info.bindingCount = static_cast<uint32_t>(mBindings.size());
    extended_info.pBindingFlags = &bindlessFlags;
    if (useBindless) {
        info.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
        info.pNext = &extended_info;
    }

    return device.createDescriptorSetLayout(info, nullptr);
}

void DescriptorAllocatorGrowable::init(const vk::raii::Device& device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    mRatios.clear();
    for (auto r : poolRatios)
        mRatios.push_back(r);

    vk::raii::DescriptorPool newPool = createPool(device, maxSets, poolRatios);
    mReadyPools.push_back(std::move(newPool));

    mSetsPerPool *= 1.5; // Grow it next allocation
}

void DescriptorAllocatorGrowable::clearPools()
{
    for (vk::raii::DescriptorPool& p : mReadyPools)
        p.reset();

    for (vk::raii::DescriptorPool& p : mFullPools) {
        p.reset();
        mReadyPools.push_back(std::move(p));
    }

    mFullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools()
{
    mReadyPools.clear();
    mFullPools.clear();
}

vk::raii::DescriptorSet DescriptorAllocatorGrowable::allocate(const vk::raii::Device& device, const vk::raii::DescriptorSetLayout& layout, bool useBindless, uint32_t maxBindings)
{
    vk::raii::DescriptorPool poolToUse = getPool(device);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = *poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &(*layout);

    vk::DescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo{};
    uint32_t maxBinding = maxBindings - 1;
    countInfo.descriptorSetCount = 1;
    countInfo.pDescriptorCounts = &maxBinding;
    if (useBindless)
        allocInfo.pNext = &countInfo;

    std::vector<vk::raii::DescriptorSet> ds;
    try {
        ds = device.allocateDescriptorSets(allocInfo);
        mReadyPools.push_back(std::move(poolToUse));
    }
    catch (vk::SystemError e) {
        mFullPools.push_back(std::move(poolToUse));
        vk::raii::DescriptorPool poolToUse1 = getPool(device);
        allocInfo.descriptorPool = *poolToUse1;
        ds = device.allocateDescriptorSets(allocInfo);
        mReadyPools.push_back(std::move(poolToUse1));
    }
    return std::move(ds[0]);
}

vk::raii::DescriptorPool DescriptorAllocatorGrowable::getPool(const vk::raii::Device& device)
{
    // Check if available pools to use, else create new pool
    if (!mReadyPools.empty()) {
        vk::raii::DescriptorPool newPool = std::move(mReadyPools.back());
        mReadyPools.pop_back();
        return newPool;
    }
    else {
        vk::raii::DescriptorPool newPool = createPool(device, mSetsPerPool, mRatios);
        mSetsPerPool *= 1.5;
        if (mSetsPerPool > 4092) 
            mSetsPerPool = 4092;
        return newPool;
    }
}

vk::raii::DescriptorPool DescriptorAllocatorGrowable::createPool(const vk::raii::Device& device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    for (const PoolSizeRatio ratio : poolRatios)
        poolSizes.push_back(vk::DescriptorPoolSize(ratio.type, static_cast<uint32_t>(ratio.ratio * setCount)));
    vk::DescriptorPoolCreateInfo pool_info = {};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool_info.pPoolSizes = poolSizes.data();
    return device.createDescriptorPool(pool_info, nullptr);
}

void DescriptorAllocatorGrowable::cleanup() 
{
	mReadyPools.clear();
	mFullPools.clear();
}

void DescriptorWriter::writeImage(int binding, const vk::ImageView image, const vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type)
{
    const vk::DescriptorImageInfo& info = mImageInfos.emplace_back(sampler, image, layout);
    vk::WriteDescriptorSet write = {};
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    write.dstBinding = binding;
    write.pImageInfo = &info;
    mWrites.push_back(write);
}

void DescriptorWriter::writeImageArray(int binding, const vk::ImageView image, const vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type, uint32_t arrayIndex)
{
    const vk::DescriptorImageInfo& info = mImageInfos.emplace_back(sampler, image, layout);
    vk::WriteDescriptorSet write = {};
    write.descriptorCount = 1;
    write.dstArrayElement = arrayIndex;
    write.descriptorType = type;
    write.dstSet = VK_NULL_HANDLE;
    write.dstBinding = binding;
    write.pImageInfo = &info;
    mWrites.push_back(write);
}

void DescriptorWriter::writeBuffer(int binding, const vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type)
{
    const vk::DescriptorBufferInfo& info = mBufferInfos.emplace_back(buffer, offset, size);
    vk::WriteDescriptorSet write = {};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;
    mWrites.push_back(write);
}

void DescriptorWriter::clear()
{
    mImageInfos.clear();
    mBufferInfos.clear();
    mWrites.clear();
}

void DescriptorWriter::updateSet(const vk::raii::Device& device, const vk::raii::DescriptorSet& set)
{
    for (vk::WriteDescriptorSet& write : mWrites)
        write.dstSet = *set;
    device.updateDescriptorSets(mWrites, nullptr);
}