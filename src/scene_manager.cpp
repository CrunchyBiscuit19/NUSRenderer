#include <scene_manager.h>
#include <renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <ranges>

vk::raii::DescriptorSetLayout SceneEncapsulation::mSceneDescriptorSetLayout = nullptr;

SceneManager::SceneManager(Renderer* renderer):
	mRenderer(renderer)
{}

void SceneManager::init()
{
	mRenderer->mSceneEncapsulation.init();
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
	std::erase_if(mRenderer->mModels, [&](const std::pair <const std::string, GLTFModel>& pair) {
		return (pair.second.mDeleteSignal.has_value()) && (pair.second.mDeleteSignal.value() == mRenderer->mFrameNumber);
	});
}

void SceneManager::deleteInstances()
{
	for (auto& model : mRenderer->mModels | std::views::values) {
		std::erase_if(model.mInstances, [&](const GLTFInstance& instance) { return instance.mDeleteSignal; });
	}
}

void SceneManager::generateRenderItems()
{
	for (auto& model : mRenderer->mModels | std::views::values) {
		model.generateRenderItems();
	}
}

void SceneManager::updateScene()
{
	mRenderer->mSceneEncapsulation.mSceneData.ambientColor = glm::vec4(.1f);
	mRenderer->mSceneEncapsulation.mSceneData.sunlightColor = glm::vec4(1.f);
	mRenderer->mSceneEncapsulation.mSceneData.sunlightDirection = glm::vec4(0.f, 1.f, 0.5, 1.f);

	mRenderer->mCamera.update(mRenderer->mStats.mFrametime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mRenderer->mSceneEncapsulation.mSceneData.view = mRenderer->mCamera.getViewMatrix();
	mRenderer->mSceneEncapsulation.mSceneData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(mRenderer->mWindowExtent.width) / static_cast<float>(mRenderer->mWindowExtent.height), 10000.f, 0.1f);
	mRenderer->mSceneEncapsulation.mSceneData.proj[1][1] *= -1;

	auto* sceneBufferPtr = static_cast<SceneData*>(mRenderer->mSceneEncapsulation.mSceneBuffer.info.pMappedData);
	std::memcpy(sceneBufferPtr, &mRenderer->mSceneEncapsulation.mSceneData, 1 * sizeof(SceneData));

	DescriptorWriter writer;
	writer.writeBuffer(0, *mRenderer->mSceneEncapsulation.mSceneBuffer.buffer, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSet(mRenderer->mDevice, *mRenderer->mSceneEncapsulation.mSceneDescriptorSet);
}

void SceneManager::cleanup()
{
	mRenderer->mSceneEncapsulation.cleanup();
	mRenderer->mModels.clear();
}

SceneEncapsulation::SceneEncapsulation(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorAllocator(nullptr),
	mSceneDescriptorSet(nullptr)
{}

void SceneEncapsulation::init()
{
	mSceneBuffer = mRenderer->mResourceManager.createBuffer(sizeof(SceneData),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	mSceneDescriptorSetLayout = builder.build(mRenderer->mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	mSceneDescriptorSet = mRenderer->mDescriptorAllocator.allocate(mRenderer->mDevice, *mSceneDescriptorSetLayout);
}

void SceneEncapsulation::cleanup()
{
	mSceneDescriptorSet.clear();
	mSceneBuffer.cleanup();
}