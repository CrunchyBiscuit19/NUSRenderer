#include <scene_manager.h>
#include <renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <ranges>

vk::raii::DescriptorSetLayout SceneEncapsulation::mSceneDescriptorSetLayout = nullptr;

SceneManager::SceneManager(Renderer* renderer):
	mRenderer(renderer),
	mSceneEncapsulation(SceneEncapsulation(renderer)),
	mSkybox(Skybox(renderer))
{}

void SceneManager::init()
{
	mSceneEncapsulation.init();
	mSkybox.loadSkyboxImage(
		fs::path(std::string(RESOURCES_PATH) + "skybox/right.jpg"),
		fs::path(std::string(RESOURCES_PATH) + "skybox/left.jpg"),
		fs::path(std::string(RESOURCES_PATH) + "skybox/top.jpg"),
		fs::path(std::string(RESOURCES_PATH) + "skybox/bottom.jpg"),
		fs::path(std::string(RESOURCES_PATH) + "skybox/front.jpg"),
		fs::path(std::string(RESOURCES_PATH) + "skybox/back.jpg"));
}

void SceneManager::loadModels(const std::vector<std::filesystem::path>& paths)
{
	for (const auto& modelPath : paths) {
		if (mModels.contains(modelPath.stem().string())) {
			continue;
		}
		auto modelFullPath = RESOURCES_PATH / modelPath;
		mModels.emplace(modelPath.stem().string(), std::move(GLTFModel(mRenderer, modelFullPath)));
	}
}

void SceneManager::deleteModels()
{
	std::erase_if(mModels, [&](const std::pair <const std::string, GLTFModel>& pair) {
		return (pair.second.mDeleteSignal.has_value()) && (pair.second.mDeleteSignal.value() == mRenderer->mRendererInfrastructure.mFrameNumber);
	});
}

void SceneManager::deleteInstances()
{
	for (auto& model : mModels | std::views::values) {
		std::erase_if(model.mInstances, [&](const GLTFInstance& instance) { return instance.mDeleteSignal; });
	}
}

void SceneManager::generateRenderItems()
{
	for (auto& model : mModels | std::views::values) {
		model.generateRenderItems();
	}
}

void SceneManager::updateScene()
{
	mSceneEncapsulation.mSceneData.sunlightDirection = glm::vec4(mSceneEncapsulation.mSceneData.sunlightDirection[0], mRenderer->mCamera.getDirectionVector());

	mRenderer->mCamera.update(mRenderer->mStats.mFrametime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mSceneEncapsulation.mSceneData.view = mRenderer->mCamera.getViewMatrix();
	mSceneEncapsulation.mSceneData.proj = glm::perspective(glm::radians(70.f), 
		static_cast<float>(mRenderer->mRendererCore.mWindowExtent.width) / static_cast<float>(mRenderer->mRendererCore.mWindowExtent.height), 10000.f, 0.1f);
	mSceneEncapsulation.mSceneData.proj[1][1] *= -1;

	auto* sceneBufferPtr = static_cast<SceneData*>(mSceneEncapsulation.mSceneBuffer.info.pMappedData);
	std::memcpy(sceneBufferPtr, &mSceneEncapsulation.mSceneData, 1 * sizeof(SceneData));

	DescriptorWriter writer;
	writer.writeBuffer(0, *mSceneEncapsulation.mSceneBuffer.buffer, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSet(mRenderer->mRendererCore.mDevice, *mSceneEncapsulation.mSceneDescriptorSet);
}

void SceneManager::cleanup()
{
	SceneEncapsulation::mSceneDescriptorSetLayout.clear();
	mSceneEncapsulation.cleanup();
	mModels.clear();
	mSkybox.cleanup();
}

SceneEncapsulation::SceneEncapsulation(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorAllocator(nullptr),
	mSceneDescriptorSet(nullptr)
{}

void SceneEncapsulation::init()
{
	mSceneData.ambientColor = glm::vec4(.1f);
	mSceneData.sunlightColor = glm::vec4(1.f);
	mSceneData.sunlightDirection = glm::vec4(0.f, 1.f, 0.5, 1.f);

	mSceneBuffer = mRenderer->mResourceManager.createBuffer(sizeof(SceneData),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	mSceneDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	mSceneDescriptorSet = mRenderer->mRendererInfrastructure.mDescriptorAllocator.allocate(*mSceneDescriptorSetLayout);
}

void SceneEncapsulation::cleanup()
{
	mSceneDescriptorSet.clear();
	mSceneBuffer.cleanup();
}