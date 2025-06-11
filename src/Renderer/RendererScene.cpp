#include <Renderer/RendererScene.h>
#include <Renderer/Renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <ranges>

Perspective::Perspective(Renderer* renderer) :
	mRenderer(renderer),
	mPerspectiveDescriptorSet(nullptr),
	mPerspectiveDescriptorSetLayout(nullptr)
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
	mPerspectiveData.ambientColor = glm::vec4(.1f);
	mPerspectiveData.sunlightColor = glm::vec4(1.f);
	mPerspectiveData.sunlightDirection = glm::vec4(0.f, 1.f, 0.5, 1.f);
}

void Perspective::initBuffer()
{
	mPerspectiveBuffer = mRenderer->mRendererResources.createBuffer(sizeof(PerspectiveData), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
	mRenderer->mRendererCore.labelResourceDebug(mPerspectiveBuffer.buffer, "SceneBuffer");
}

void Perspective::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	mPerspectiveDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	mPerspectiveDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mPerspectiveDescriptorSetLayout);

	DescriptorSetBinder writer;
	writer.bindBuffer(0, *mPerspectiveBuffer.buffer, sizeof(PerspectiveData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mPerspectiveDescriptorSet);
}

void Perspective::update()
{
	mRenderer->mCamera.update(mRenderer->mStats.mFrametime, static_cast<float>(ONE_SECOND_IN_MS / EXPECTED_FRAME_RATE));
	mPerspectiveData.view = mRenderer->mCamera.getViewMatrix();
	mPerspectiveData.proj = glm::perspective(glm::radians(70.f), static_cast<float>(mRenderer->mRendererCore.mWindowExtent.width) / static_cast<float>(mRenderer->mRendererCore.mWindowExtent.height), 10000.f, 0.1f);
	mPerspectiveData.proj[1][1] *= -1;

	auto* sceneBufferPtr = static_cast<PerspectiveData*>(mPerspectiveBuffer.info.pMappedData);
	std::memcpy(sceneBufferPtr, &mPerspectiveData, 1 * sizeof(PerspectiveData));
}

void Perspective::cleanup()
{
	mPerspectiveDescriptorSet.clear();
	mPerspectiveDescriptorSetLayout.clear();
	mPerspectiveBuffer.cleanup();
}

SceneManager::SceneManager(Renderer* renderer) :
	mRenderer(renderer),
	mMainMaterialResourcesDescriptorSet(nullptr),
	mMainMaterialResourcesDescriptorSetLayout(nullptr)
{
}

void SceneManager::init()
{
	initBuffers();
	initDescriptor();
	initPushConstants();
}

void SceneManager::initBuffers()
{
	mMainVertexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainVertexBuffer.buffer, "MainVertexBuffer");
	mMainVertexBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainVertexBuffer.buffer));

	mMainIndexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainIndexBuffer.buffer, "MainIndexBuffer");

	mMainMaterialConstantsBuffer = mRenderer->mRendererResources.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialConstantsBuffer.buffer, "MainMaterialConstantsBuffer");
	mMainMaterialConstantsBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainMaterialConstantsBuffer.buffer));

	mMainNodeTransformsBuffer = mRenderer->mRendererResources.createBuffer(MAX_NODES * sizeof(glm::mat4), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainNodeTransformsBuffer.buffer, "MainNodeTransformsBuffer");
	mMainNodeTransformsBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainNodeTransformsBuffer.buffer));

	mMainInstancesBuffer = mRenderer->mRendererResources.createBuffer(MAX_INSTANCES * sizeof(InstanceData), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainInstancesBuffer.buffer, "MainInstancesBuffer");
	mMainInstancesBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainInstancesBuffer.buffer));
}

void SceneManager::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS);
	mMainMaterialResourcesDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, true);
	mMainMaterialResourcesDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(mMainMaterialResourcesDescriptorSetLayout, true);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialResourcesDescriptorSetLayout, "MainMaterialResourcesDescriptorSetLayout");
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialResourcesDescriptorSet, "MainMaterialResourcesDescriptorSet");
}

void SceneManager::initPushConstants()
{
	mScenePushConstants.vertexBuffer = mMainVertexBuffer.address;
	mScenePushConstants.materialConstantsBuffer = mMainMaterialConstantsBuffer.address;
	mScenePushConstants.nodeTransformsBuffer = mMainNodeTransformsBuffer.address;
	mScenePushConstants.instancesBuffer = mMainInstancesBuffer.address;
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

void SceneManager::regenerateRenderItems()
{
    for (auto& batch : mBatches | std::views::values) {
        batch.renderItems.clear();
    }
    
	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

        model.generateRenderItems();
    }
    
	for (auto& batch : mBatches | std::views::values) {
		mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
			cmd.fillBuffer(*batch.renderItemsBuffer.buffer, 0, vk::WholeSize, 0);
			cmd.fillBuffer(*batch.visibleRenderItemsBuffer.buffer, 0, vk::WholeSize, 0);
		}); 

		if (batch.renderItems.size() == 0) { continue; }

        std::memcpy(batch.renderItemsStagingBuffer.info.pMappedData, batch.renderItems.data(), batch.renderItems.size() * sizeof(RenderItem));

        vk::BufferCopy renderItemsCopy {};
        renderItemsCopy.dstOffset = 0;
        renderItemsCopy.srcOffset = 0;
        renderItemsCopy.size = batch.renderItems.size() * sizeof(RenderItem);

        mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
            cmd.copyBuffer(*batch.renderItemsStagingBuffer.buffer, *batch.renderItemsBuffer.buffer, renderItemsCopy);
        });
    }
}

void SceneManager::realignVertexIndexOffset()
{
	int vertexCumulative = 0;
	int indexCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		for (auto& mesh : model.mMeshes) {
			mesh.mMainVertexOffset = vertexCumulative;
			mesh.mMainFirstIndex = indexCumulative;
			vertexCumulative += mesh.mNumVertices;
			indexCumulative += mesh.mNumIndices;
		}
	}
}

void SceneManager::realignMaterialOffset()
{
	int materialCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstMaterial = materialCumulative;
		materialCumulative += model.mMaterials.size();
	}
}

void SceneManager::realignNodeTransformsOffset()
{
	int nodeTransformCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstNodeTransform = nodeTransformCumulative;
		nodeTransformCumulative += model.mNodes.size();
	}
}

void SceneManager::realignInstancesOffset()
{
	int instanceCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstInstance = instanceCumulative;
		instanceCumulative += model.mInstances.size();
	}
}

void SceneManager::realignOffsets()
{
	realignVertexIndexOffset();
	realignMaterialOffset();
	realignNodeTransformsOffset();
	realignInstancesOffset();
}

void SceneManager::reloadMainVertexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		for (auto& mesh : model.mMeshes) {
			vk::BufferCopy meshVertexCopy{};
			meshVertexCopy.dstOffset = dstOffset;
			meshVertexCopy.srcOffset = 0;
			meshVertexCopy.size = mesh.mNumVertices * sizeof(Vertex);

			dstOffset += meshVertexCopy.size;

			mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
				cmd.copyBuffer(*mesh.mVertexBuffer.buffer, *mMainVertexBuffer.buffer, meshVertexCopy);
			});
		}
	}
}

void SceneManager::reloadMainIndexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		for (auto& mesh : model.mMeshes) {
			vk::BufferCopy meshIndexCopy{};
			meshIndexCopy.dstOffset = dstOffset;
			meshIndexCopy.srcOffset = 0;
			meshIndexCopy.size = mesh.mNumIndices * sizeof(uint32_t);

			dstOffset += meshIndexCopy.size;

			mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
				cmd.copyBuffer(*mesh.mIndexBuffer.buffer, *mMainIndexBuffer.buffer, meshIndexCopy);
			});

		}
	}
}

void SceneManager::reloadMainMaterialConstantsBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

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

void SceneManager::reloadMainNodeTransformsBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

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

void SceneManager::reloadMainInstancesBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }
		if (model.mInstances.size() == 0) { continue; }

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

void SceneManager::reloadMainMaterialResourcesArray()
{
	for (auto& model : mModels | std::views::values) {
		for (auto& material : model.mMaterials) {
			int materialTextureArrayIndex = (model.mMainFirstMaterial + material.mRelativeMaterialIndex) * 5;

			DescriptorSetBinder writer;
			writer.bindImageArray(0, materialTextureArrayIndex + 0, *material.mPbrData.resources.base.image->imageView, material.mPbrData.resources.base.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 1, *material.mPbrData.resources.metallicRoughness.image->imageView, material.mPbrData.resources.metallicRoughness.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 2, *material.mPbrData.resources.emissive.image->imageView, material.mPbrData.resources.emissive.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 3, *material.mPbrData.resources.normal.image->imageView, material.mPbrData.resources.normal.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 4, *material.mPbrData.resources.occlusion.image->imageView, material.mPbrData.resources.occlusion.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mMainMaterialResourcesDescriptorSet);
		}
	}
}

void SceneManager::reloadMainBuffers()
{
	reloadMainVertexBuffer();
	reloadMainIndexBuffer();
	reloadMainMaterialConstantsBuffer();
	reloadMainInstancesBuffer();
	reloadMainNodeTransformsBuffer();
	reloadMainMaterialResourcesArray();
}

void SceneManager::resetFlags()
{
	mFlags.mModelAddedFlag = false;
	mFlags.mModelDestroyedFlag = false;
	mFlags.mInstanceAddedFlag = false;
	mFlags.mInstanceDestroyedFlag = false;
	mFlags.mReloadMainInstancesBuffer = false;
}

void SceneManager::cleanup()
{
	mModels.clear();
	mBatches.clear();
	mMainMaterialResourcesDescriptorSet.clear();
	mMainMaterialResourcesDescriptorSetLayout.clear();
	mMainInstancesBuffer.cleanup();
	mMainNodeTransformsBuffer.cleanup();
	mMainMaterialConstantsBuffer.cleanup();
	mMainIndexBuffer.cleanup();
	mMainVertexBuffer.cleanup();
}

RendererScene::RendererScene(Renderer* renderer) :
	mRenderer(renderer),
	mPerspective(Perspective(renderer)),
	mSkybox(Skybox(renderer)),
	mSceneManager(SceneManager(renderer))
{
}

void RendererScene::init()
{
	mPerspective.init();
	mSkybox.init(std::filesystem::path(std::string(SKYBOXES_PATH) + "ocean/"));
	mSceneManager.init();
}

void RendererScene::cleanup()
{
	mPerspective.cleanup();
	mSkybox.cleanup();
	mSceneManager.cleanup();

}