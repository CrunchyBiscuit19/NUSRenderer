#include <Renderer/Scene/Perspective.h>
#include <Renderer/Renderer.h>

Perspective::Perspective(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorSet(nullptr),
	mDescriptorSetLayout(nullptr)
{
}

void Perspective::init()
{
	initData();
	initBuffer();
	initDescriptor();
}

void Perspective::initData()
{
	mData.ambientColor = glm::vec4(.1f);
	mData.sunlightColor = glm::vec4(1.f);
	mData.sunlightDirection = glm::vec4(0.f, 1.f, 0.5, 1.f);
}

void Perspective::initBuffer()
{
	mDataBuffer = mRenderer->mRendererResources.createBuffer(sizeof(PerspectiveData), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
	mRenderer->mRendererCore.labelResourceDebug(mDataBuffer.buffer, "PerspectiveBuffer");
	LOG_INFO(mRenderer->mLogger, "Node Transforms Staging Buffer Created");
}

void Perspective::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	mDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	mRenderer->mRendererCore.labelResourceDebug(mDescriptorSetLayout, "PerspectiveDescriptorSetLayout");
	mDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mDescriptorSetLayout);
	mRenderer->mRendererCore.labelResourceDebug(mDescriptorSet, "PerspectiveDescriptorSet");

	DescriptorSetBinder writer;
	writer.bindBuffer(0, *mDataBuffer.buffer, sizeof(PerspectiveData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mDescriptorSet);
	LOG_INFO(mRenderer->mLogger, "Perspective Descriptor Set and Layout Created");
}

void Perspective::update()
{
	mRenderer->mCamera.update(mRenderer->mStats.mFrameTime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mData.view = mRenderer->mCamera.getViewMatrix();
	mData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(mRenderer->mRendererCore.mWindowExtent.width) / static_cast<float>(mRenderer->mRendererCore.mWindowExtent.height), 10000.f, 0.1f);
	mData.proj[1][1] *= -1;

	auto* sceneBufferPtr = static_cast<PerspectiveData*>(mDataBuffer.info.pMappedData);
	std::memcpy(sceneBufferPtr, &mData, 1 * sizeof(PerspectiveData));
}

void Perspective::cleanup()
{
	mDescriptorSet.clear();
	mDescriptorSetLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Perspective Descriptor Set and Layout Destroyed");
	mDataBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Perspective Buffer Destroyed");
}