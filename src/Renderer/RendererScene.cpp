#include <Renderer/RendererScene.h>
#include <Renderer/Renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <ranges>

RendererScene::RendererScene(Renderer* renderer) :
	mRenderer(renderer),
	mSceneResources(SceneResources(renderer)),
	mSkybox(Skybox(renderer))
{
}

void RendererScene::init()
{
	mSceneResources.init();
	mSkybox.init(std::filesystem::path(std::string(SKYBOXES_PATH) + "ocean/"));

	mGlobalVertexBuffer = mRenderer->mRendererResources.createBuffer(GLOBAL_VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mGlobalVertexBuffer.buffer, "GlobalVertexBuffer");
}

void RendererScene::loadModels(const std::vector<std::filesystem::path>& paths)
{
	for (const auto& modelPath : paths) {
		if (mModels.contains(modelPath.stem().string())) {
			continue;
		}
		auto modelFullPath = MODELS_PATH / modelPath;
		mModels.emplace(modelPath.stem().string(), std::move(GLTFModel(mRenderer, modelFullPath)));
	}
}

void RendererScene::deleteModels()
{
	std::erase_if(mModels, [&](const std::pair <const std::string, GLTFModel>& pair) {
		return (pair.second.mDeleteSignal.has_value()) && (pair.second.mDeleteSignal.value() == mRenderer->mRendererInfrastructure.mFrameNumber);
		});
}

void RendererScene::deleteInstances()
{
	for (auto& model : mModels | std::views::values) {
		std::erase_if(model.mInstances, [&](const GLTFInstance& instance) { return instance.mDeleteSignal; });
	}
}

void RendererScene::reloadGlobalVertexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		for (auto& mesh : model.mMeshes) {
			vk::BufferCopy meshVertexCopy{};
			meshVertexCopy.dstOffset = dstOffset;
			meshVertexCopy.srcOffset = 0;
			meshVertexCopy.size = mesh->mVertexBuffer.info.size;

			dstOffset += mesh->mVertexBuffer.info.size;

			mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
				cmd.copyBuffer(*mesh->mVertexBuffer.buffer, *mGlobalVertexBuffer.buffer, meshVertexCopy);
			});

		}
	}
}

void RendererScene::generateRenderItems()
{
	for (auto& model : mModels | std::views::values) {
		model.generateRenderItems();
	}
}

void RendererScene::updateScene()
{
	mSceneResources.updateResources();
}

void RendererScene::cleanup()
{
	mSceneResources.cleanup();
	mModels.clear();
	mSkybox.cleanup();
	mGlobalVertexBuffer.cleanup();
}

SceneResources::SceneResources(Renderer* renderer) :
	mRenderer(renderer),
	mSceneDescriptorSet(nullptr),
	mSceneDescriptorSetLayout(nullptr)
{
}

void SceneResources::initSceneResourcesData()
{
	mSceneData.ambientColor = glm::vec4(.1f);
	mSceneData.sunlightColor = glm::vec4(1.f);
	mSceneData.sunlightDirection = glm::vec4(0.f, 1.f, 0.5, 1.f);
}

void SceneResources::initSceneResourcesBuffer()
{
	mSceneBuffer = mRenderer->mRendererResources.createBuffer(sizeof(SceneData), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void SceneResources::initSceneResourcesDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	mSceneDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	mSceneDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mSceneDescriptorSetLayout);

	DescriptorSetBinder writer;
	writer.bindBuffer(0, *mSceneBuffer.buffer, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mSceneDescriptorSet);
}

void SceneResources::init()
{
	initSceneResourcesData();
	initSceneResourcesBuffer();
	initSceneResourcesDescriptor();
}

void SceneResources::updateResources()
{
	mRenderer->mCamera.update(mRenderer->mStats.mFrametime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mSceneData.view = mRenderer->mCamera.getViewMatrix();
	mSceneData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(mRenderer->mRendererCore.mWindowExtent.width) / static_cast<float>(mRenderer->mRendererCore.mWindowExtent.height), 10000.f, 0.1f);
	mSceneData.proj[1][1] *= -1;

	auto* sceneBufferPtr = static_cast<SceneData*>(mSceneBuffer.info.pMappedData);
	std::memcpy(sceneBufferPtr, &mSceneData, 1 * sizeof(SceneData));
}

void SceneResources::cleanup()
{
	mSceneDescriptorSet.clear();
	mSceneDescriptorSetLayout.clear();
	mSceneBuffer.cleanup();
}