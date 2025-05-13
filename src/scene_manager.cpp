#include <scene_manager.h>
#include <renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <ranges>

SceneManager::SceneManager(Renderer* renderer):
	mRenderer(renderer)
{}

void SceneManager::init()
{
	mRenderer->mSceneBuffer = mRenderer->mResourceManager.createBuffer(sizeof(SceneData), 
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, 
		VMA_MEMORY_USAGE_GPU_ONLY);

	mSceneStagingBuffer = mRenderer->mResourceManager.createStagingBuffer(sizeof(SceneData));

	vk::BufferDeviceAddressInfo deviceAddressInfo;
	deviceAddressInfo.buffer = *mRenderer->mSceneBuffer.buffer;
	mRenderer->mPushConstants.sceneBuffer = mRenderer->mDevice.getBufferAddress(deviceAddressInfo);
}

void SceneManager::loadModels(const std::vector<std::filesystem::path>& paths)
{
	for (const auto& modelPath : paths) {
		if (mRenderer->mModels.contains(modelPath.stem().string())) {
			continue;
		}
		auto modelFullPath = RESOURCES_PATH / modelPath;
		mRenderer->mModels.emplace(modelPath.stem().string(), std::move(GLTFModel(mRenderer, modelFullPath)));
	}
}

void SceneManager::deleteModels()
{
	std::erase_if(mRenderer->mModels, [](const auto& pair) {
		return pair.second.mToDelete; 
	});
}

void SceneManager::deleteInstances()
{
	for (auto& model : mRenderer->mModels | std::views::values) {
		std::erase_if(model.mInstances, [](const auto& instance) { return instance.toDelete; });
	}
}

void SceneManager::generateRenderItems()
{
	for (auto& model : mRenderer->mModels | std::views::values) {
		model.generateRenderItems();
	}
}

void SceneManager::updateSceneBuffer()
{
	mRenderer->mSceneData.ambientColor = glm::vec4(1.f);
	mRenderer->mSceneData.sunlightColor = glm::vec4(1.f);
	mRenderer->mSceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);

	mRenderer->mCamera.update(mRenderer->mStats.mFrametime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mRenderer->mSceneData.view = mRenderer->mCamera.getViewMatrix();
	mRenderer->mSceneData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(mRenderer->mWindowExtent.width) / static_cast<float>(mRenderer->mWindowExtent.height), 10000.f, 0.1f);
	mRenderer->mSceneData.proj[1][1] *= -1;

	const VkDeviceSize sceneDataSize = sizeof(SceneData);
	std::memcpy(mSceneStagingBuffer.info.pMappedData, &mRenderer->mSceneData, sceneDataSize);

	VkBufferCopy sceneCopy{}; 
	sceneCopy.dstOffset = 0;
	sceneCopy.srcOffset = 0;
	sceneCopy.size = sceneDataSize;

	mRenderer->mResourceManager.mPerDrawBufferCopyBatches.emplace_back(
		*mSceneStagingBuffer.buffer,
		*mRenderer->mSceneBuffer.buffer,
		vk::BufferCopy { sceneCopy }
	);
}

void SceneManager::updateMaterialTextureArray(std::shared_ptr<PbrMaterial> material)
{
	DescriptorWriter writer;

	writer.writeImageArray(0, *material->mPbrData.resources.base.image->imageView, material->mPbrData.resources.base.sampler, vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler, 0);
	writer.writeImageArray(0, *material->mPbrData.resources.emissive.image->imageView, material->mPbrData.resources.emissive.sampler, vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler, 1);
	writer.writeImageArray(0, *material->mPbrData.resources.metallicRoughness.image->imageView, material->mPbrData.resources.metallicRoughness.sampler, vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler, 2);
	writer.writeImageArray(0, *material->mPbrData.resources.normal.image->imageView, material->mPbrData.resources.normal.sampler, vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler, 3);
	writer.writeImageArray(0, *material->mPbrData.resources.occlusion.image->imageView, material->mPbrData.resources.occlusion.sampler, vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler, 4);
	
	writer.updateSet(mRenderer->mDevice, mRenderer->mMaterialTexturesArray.set);
}

void SceneManager::cleanup()
{
	mSceneStagingBuffer.cleanup();
	mRenderer->mSceneBuffer.cleanup();
	mRenderer->mModels.clear();
}

