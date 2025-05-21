#include <scene_manager.h>
#include <renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <ranges>

SceneManager::SceneManager(Renderer* renderer):
	mRenderer(renderer),
	mSceneResources(SceneResources(renderer)),
	mSkybox(Skybox(renderer))
{}

void SceneManager::init()
{
	mSceneResources.init();
	mSkybox.init(
		fs::path(std::string(SKYBOXES_PATH) + "ocean/right.jpg"),
		fs::path(std::string(SKYBOXES_PATH) + "ocean/left.jpg"),
		fs::path(std::string(SKYBOXES_PATH) + "ocean/top.jpg"),
		fs::path(std::string(SKYBOXES_PATH) + "ocean/bottom.jpg"),
		fs::path(std::string(SKYBOXES_PATH) + "ocean/front.jpg"),
		fs::path(std::string(SKYBOXES_PATH) + "ocean/back.jpg")); 
}

void SceneManager::loadModels(const std::vector<std::filesystem::path>& paths)
{
	for (const auto& modelPath : paths) {
		if (mModels.contains(modelPath.stem().string())) {
			continue;
		}
		auto modelFullPath = MODELS_PATH / modelPath;
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
	mSceneResources.mSceneData.sunlightDirection = glm::vec4(mSceneResources.mSceneData.sunlightDirection[0], mRenderer->mCamera.getDirectionVector());

	mRenderer->mCamera.update(mRenderer->mStats.mFrametime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mSceneResources.mSceneData.view = mRenderer->mCamera.getViewMatrix();
	mSceneResources.mSceneData.proj = glm::perspective(glm::radians(70.f), 
		static_cast<float>(mRenderer->mRendererCore.mWindowExtent.width) / static_cast<float>(mRenderer->mRendererCore.mWindowExtent.height), 10000.f, 0.1f);
	mSceneResources.mSceneData.proj[1][1] *= -1;

	auto* sceneBufferPtr = static_cast<SceneData*>(mSceneResources.mSceneBuffer.info.pMappedData);
	std::memcpy(sceneBufferPtr, &mSceneResources.mSceneData, 1 * sizeof(SceneData));

	DescriptorWriter writer;
	writer.writeBuffer(0, *mSceneResources.mSceneBuffer.buffer, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSet(mRenderer->mRendererCore.mDevice, *mSceneResources.mSceneDescriptorSet);
}

void SceneManager::cleanup()
{
	mSceneResources.cleanup();
	mModels.clear();
	mSkybox.cleanup();
}

SceneResources::SceneResources(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorAllocator(nullptr),
	mSceneDescriptorSet(nullptr),
	mSceneDescriptorSetLayout(nullptr)
{}

void SceneResources::init()
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

void SceneResources::cleanup()
{
	mSceneDescriptorSet.clear();
	mSceneDescriptorSetLayout.clear();
	mSceneBuffer.cleanup();
}