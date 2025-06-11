#include <Renderer/Renderer.h>
#include <Utils/Descriptor.h>

#include "quill/LogMacros.h"

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

vk::raii::DescriptorSetLayout DescriptorLayoutBuilder::build(vk::raii::Device& device, vk::ShaderStageFlags shaderStages, bool useBindless)
{
	for (auto& b : mBindings) {
		b.stageFlags |= shaderStages;
	}

	vk::DescriptorSetLayoutCreateInfo info{};
	info.pBindings = mBindings.data();
	info.bindingCount = static_cast<uint32_t>(mBindings.size());

	vk::DescriptorBindingFlags bindlessFlags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
	vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo {};
	bindingFlagsInfo.pBindingFlags = &bindlessFlags;
	bindingFlagsInfo.bindingCount = static_cast<uint32_t>(mBindings.size());

	if (useBindless) {
		info.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
		info.pNext = &bindingFlagsInfo;
	}

	return device.createDescriptorSetLayout(info, nullptr);
}

DescriptorAllocatorGrowable::DescriptorAllocatorGrowable(Renderer* renderer) :
	mRenderer(renderer)
{
}

void DescriptorAllocatorGrowable::init(uint32_t maxSets, std::vector<DescriptorTypeRatio>& poolRatios)
{
	mRatios = poolRatios;

	vk::raii::DescriptorPool newPool = createPool(maxSets, poolRatios);
	mReadyPools.emplace_back(std::move(newPool));

	mSetsPerPool = maxSets * 1.5;
}

void DescriptorAllocatorGrowable::clearPools()
{
	for (vk::raii::DescriptorPool& p : mReadyPools)
		p.reset();

	for (vk::raii::DescriptorPool& p : mFullPools) {
		p.reset();
		mReadyPools.emplace_back(std::move(p));
	}

	mFullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools()
{
	mReadyPools.clear();
	mFullPools.clear();
}

vk::raii::DescriptorSet DescriptorAllocatorGrowable::allocate(const vk::DescriptorSetLayout layout, bool useBindless)
{
	vk::raii::DescriptorPool poolToUse = getPool();

	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = *poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	vk::DescriptorSetVariableDescriptorCountAllocateInfo countInfo;
	countInfo.descriptorSetCount = 1;
	countInfo.pDescriptorCounts = &MAX_TEXTURE_ARRAY_SLOTS;
	if (useBindless)
		allocInfo.pNext = &countInfo;

	std::vector<vk::raii::DescriptorSet> ds;
	try {
		ds = mRenderer->mRendererCore.mDevice.allocateDescriptorSets(allocInfo);
		mReadyPools.emplace_back(std::move(poolToUse));
	}
	catch (vk::SystemError e) { // OutOfPoolMemory or FragmentedPool Errors
		mFullPools.emplace_back(std::move(poolToUse));
		vk::raii::DescriptorPool poolToUse1 = getPool();
		allocInfo.descriptorPool = *poolToUse1;
		ds = mRenderer->mRendererCore.mDevice.allocateDescriptorSets(allocInfo);
		mReadyPools.emplace_back(std::move(poolToUse1));
	}
	return std::move(ds[0]);
}

vk::raii::DescriptorPool DescriptorAllocatorGrowable::getPool()
{
	// Check if available pools to use, else create new pool
	if (!mReadyPools.empty()) {
		vk::raii::DescriptorPool newPool = std::move(mReadyPools.back());
		mReadyPools.pop_back();
		return newPool;
	}
	else {
		vk::raii::DescriptorPool newPool = createPool(mSetsPerPool, mRatios);
		mSetsPerPool *= 1.5;
		if (mSetsPerPool > 4092)
			mSetsPerPool = 4092;
		return newPool;
	}
}

vk::raii::DescriptorPool DescriptorAllocatorGrowable::createPool(uint32_t setCount, std::vector<DescriptorTypeRatio>& poolRatios)
{
	std::vector<vk::DescriptorPoolSize> poolSizes;
	for (const DescriptorTypeRatio ratio : poolRatios)
		poolSizes.push_back(vk::DescriptorPoolSize(ratio.type, static_cast<uint32_t>(ratio.amountPerSet * setCount)));
	vk::DescriptorPoolCreateInfo pool_info = {};
	pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	pool_info.maxSets = setCount;
	pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	pool_info.pPoolSizes = poolSizes.data();
	return mRenderer->mRendererCore.mDevice.createDescriptorPool(pool_info, nullptr);
}

void DescriptorAllocatorGrowable::cleanup()
{
	mReadyPools.clear();
	mFullPools.clear();
}

void DescriptorSetBinder::bindImage(int binding, const vk::ImageView image, const vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type)
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

void DescriptorSetBinder::bindImageArray(int binding, uint32_t arrayIndex, const vk::ImageView image, const vk::Sampler sampler, vk::ImageLayout layout, vk::DescriptorType type)
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

void DescriptorSetBinder::bindBuffer(int binding, const vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type)
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

void DescriptorSetBinder::clear()
{
	mImageInfos.clear();
	mBufferInfos.clear();
	mWrites.clear();
}

void DescriptorSetBinder::updateSetBindings(const vk::raii::Device& device, const vk::DescriptorSet set)
{
	for (vk::WriteDescriptorSet& write : mWrites)
		write.dstSet = set;
	device.updateDescriptorSets(mWrites, nullptr);
}