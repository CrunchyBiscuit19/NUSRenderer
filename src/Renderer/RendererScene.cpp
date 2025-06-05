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

	mMainVertexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainVertexBuffer.buffer, "MainVertexBuffer");

	mMainIndexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainIndexBuffer.buffer, "MainIndexBuffer");

	mMainMaterialConstantsBuffer = mRenderer->mRendererResources.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialConstantsBuffer.buffer, "MainMaterialConstantsBuffer");

	mMainNodeTransformsBuffer = mRenderer->mRendererResources.createBuffer(MAX_NODES * sizeof(glm::mat4), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainNodeTransformsBuffer.buffer, "MainNodeTransformsBuffer");

	mMainInstancesBuffer = mRenderer->mRendererResources.createBuffer(MAX_INSTANCES * sizeof(InstanceData), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainInstancesBuffer.buffer, "MainInstancesBuffer");
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

void RendererScene::reloadMainVertexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		for (auto& mesh : model.mMeshes) {
			vk::BufferCopy meshVertexCopy{};
			meshVertexCopy.dstOffset = dstOffset;
			meshVertexCopy.srcOffset = 0;
			meshVertexCopy.size = mesh->mNumVertices * sizeof(Vertex);

			dstOffset += meshVertexCopy.size;

			mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
				cmd.copyBuffer(*mesh->mVertexBuffer.buffer, *mMainVertexBuffer.buffer, meshVertexCopy);
			});

		}
	}
}

void RendererScene::reloadMainIndexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		for (auto& mesh : model.mMeshes) {
			vk::BufferCopy meshIndexCopy{};
			meshIndexCopy.dstOffset = dstOffset;
			meshIndexCopy.srcOffset = 0;
			meshIndexCopy.size = mesh->mNumIndices * sizeof(uint32_t);

			dstOffset += meshIndexCopy.size;

			mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
				cmd.copyBuffer(*mesh->mIndexBuffer.buffer, *mMainIndexBuffer.buffer, meshIndexCopy);
			});

		}
	}
}

void RendererScene::reloadMainMaterialConstantsBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		vk::BufferCopy materialConstantCopy{};
		materialConstantCopy.dstOffset = dstOffset;
		materialConstantCopy.srcOffset = 0;
		materialConstantCopy.size = model.mMaterials.size() * sizeof(MaterialConstants);

		dstOffset += materialConstantCopy.size;

		mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
			cmd.copyBuffer(*model.mMaterialConstantsBuffer.buffer, *mMainMaterialConstantsBuffer.buffer, materialConstantCopy);
		});
	}
}

void RendererScene::reloadMainNodeTransformsBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		vk::BufferCopy nodeTransformsCopy{};
		nodeTransformsCopy.dstOffset = dstOffset;
		nodeTransformsCopy.srcOffset = 0;
		nodeTransformsCopy.size = model.mNodes.size() * sizeof(glm::mat4);

		dstOffset += nodeTransformsCopy.size;

		mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
			cmd.copyBuffer(*model.mNodeTransformsBuffer.buffer, *mMainNodeTransformsBuffer.buffer, nodeTransformsCopy);
		});
	}
}

void RendererScene::reloadMainInstancesBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		vk::BufferCopy instancesCopy{};
		instancesCopy.dstOffset = dstOffset;
		instancesCopy.srcOffset = 0;
		instancesCopy.size = model.mInstances.size() * sizeof(InstanceData);

		dstOffset += instancesCopy.size;

		mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
			cmd.copyBuffer(*model.mInstancesBuffer.buffer, *mMainInstancesBuffer.buffer, instancesCopy);
		});
	}
}

void RendererScene::alignOffsets()
{
	int vertexCumulative = 0;
	int indexCumulative = 0;
	int materialCumulative = 0;
	int nodeTransformCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		for (auto& mesh : model.mMeshes) {
			mesh->mMainVertexOffset = vertexCumulative;
			mesh->mMainFirstIndex = indexCumulative;
			vertexCumulative += mesh->mNumVertices;
			indexCumulative += mesh->mNumIndices;
		}

		model.mMainFirstMaterial = materialCumulative;
		model.mMainFirstNodeTransform = nodeTransformCumulative;
		materialCumulative += model.mMaterials.size();
		nodeTransformCumulative += model.mNodes.size();
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
	mMainInstancesBuffer.cleanup();
	mMainNodeTransformsBuffer.cleanup();
	mMainMaterialConstantsBuffer.cleanup();
	mMainIndexBuffer.cleanup();
	mMainVertexBuffer.cleanup();
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
	mRenderer->mRendererCore.labelResourceDebug(mSceneBuffer.buffer, "SceneBuffer");
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