#include <Renderer/RendererScene.h>
#include <Renderer/Renderer.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <fmt/core.h>

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
	mRenderer->mRendererCore.labelResourceDebug(mPerspectiveBuffer.buffer, "PerspectiveBuffer");
	LOG_INFO(mRenderer->mLogger, "Node Transforms Staging Buffer Created");
}

void Perspective::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
	mPerspectiveDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	mRenderer->mRendererCore.labelResourceDebug(mPerspectiveDescriptorSetLayout, "PerspectiveDescriptorSetLayout");
	mPerspectiveDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mPerspectiveDescriptorSetLayout);
	mRenderer->mRendererCore.labelResourceDebug(mPerspectiveDescriptorSet, "PerspectiveDescriptorSet");

	DescriptorSetBinder writer;
	writer.bindBuffer(0, *mPerspectiveBuffer.buffer, sizeof(PerspectiveData), 0, vk::DescriptorType::eUniformBuffer);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mPerspectiveDescriptorSet);
	LOG_INFO(mRenderer->mLogger, "Perspective Descriptor Set and Layout Created");
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
	LOG_INFO(mRenderer->mLogger, "Perspective Descriptor Set and Layout Destroyed");
	mPerspectiveBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Perspective Buffer Destroyed");
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
	LOG_INFO(mRenderer->mLogger, "Main Vertex Buffer Created");

	mMainIndexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainIndexBuffer.buffer, "MainIndexBuffer");
	LOG_INFO(mRenderer->mLogger, "Main Index Buffer Created");

	mMainMaterialConstantsBuffer = mRenderer->mRendererResources.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialConstantsBuffer.buffer, "MainMaterialConstantsBuffer");
	mMainMaterialConstantsBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainMaterialConstantsBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Material Constants Buffer Created");

	mMainNodeTransformsBuffer = mRenderer->mRendererResources.createBuffer(MAX_NODES * sizeof(glm::mat4), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainNodeTransformsBuffer.buffer, "MainNodeTransformsBuffer");
	mMainNodeTransformsBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainNodeTransformsBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Node Transforms Buffer Created");

	mMainInstancesBuffer = mRenderer->mRendererResources.createBuffer(MAX_INSTANCES * sizeof(InstanceData), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainInstancesBuffer.buffer, "MainInstancesBuffer");
	mMainInstancesBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*mMainInstancesBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Instances Buffer Created");
}

void SceneManager::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS);
	mMainMaterialResourcesDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, true);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialResourcesDescriptorSetLayout, "MainMaterialResourcesDescriptorSetLayout");
	mMainMaterialResourcesDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(mMainMaterialResourcesDescriptorSetLayout, true);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialResourcesDescriptorSet, "MainMaterialResourcesDescriptorSet");
	LOG_INFO(mRenderer->mLogger, "Main Material Resources and Descriptor Set Created");
}

void SceneManager::initPushConstants()
{
	mScenePushConstants.vertexBuffer = mMainVertexBuffer.address;
	mScenePushConstants.materialConstantsBuffer = mMainMaterialConstantsBuffer.address;
	mScenePushConstants.nodeTransformsBuffer = mMainNodeTransformsBuffer.address;
	mScenePushConstants.instancesBuffer = mMainInstancesBuffer.address;
	LOG_INFO(mRenderer->mLogger, "Scene Push Constants Initialized");
}

void SceneManager::loadModels(const std::vector<std::filesystem::path>& paths)
{
	for (const auto& modelPath : paths) {
		auto modelShortPath = modelPath.stem().string();
		auto modelFullPath = MODELS_PATH / modelPath;
		auto [_, inserted] = mModels.try_emplace(modelShortPath, mRenderer, modelFullPath);
		if (inserted) { mFlags.modelAddedFlag = true; }
	}
}

void SceneManager::deleteModels()
{
	std::erase_if(mModels, [&](const std::pair <const std::string, GLTFModel>& pair) {
		if (pair.second.mDeleteSignal.has_value()) { mFlags.modelDestroyedFlag = true; }
		return pair.second.mDeleteSignal.has_value() && (pair.second.mDeleteSignal.value() == mRenderer->mRendererInfrastructure.mFrameNumber);
	});
}

void SceneManager::deleteInstances()
{
	for (auto& model : mModels | std::views::values) {
		std::erase_if(model.mInstances, [&](const GLTFInstance& instance) { 
			return instance.mDeleteSignal; 
		});
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

	LOG_INFO(mRenderer->mLogger, "Render Items Regenerated");
    
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

		LOG_INFO(mRenderer->mLogger, "Batch {} Render Items Uploading", batch.pipeline->id);
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

	LOG_INFO(mRenderer->mLogger, "Models Vertex / Index Buffers Offsets Realigned");
}

void SceneManager::realignMaterialOffset()
{
	int materialCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstMaterial = materialCumulative;
		materialCumulative += model.mMaterials.size();
	}

	LOG_INFO(mRenderer->mLogger, "Models Material Constants Buffer / Resources Descriptor Set Offsets Realigned");
}

void SceneManager::realignNodeTransformsOffset()
{
	int nodeTransformCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstNodeTransform = nodeTransformCumulative;
		nodeTransformCumulative += model.mNodes.size();
	}

	LOG_INFO(mRenderer->mLogger, "Models Node Transforms Buffer Offsets Realigned");
}

void SceneManager::realignInstancesOffset()
{
	int instanceCumulative = 0;

	for (auto& model : mModels | std::views::values) {
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstInstance = instanceCumulative;
		instanceCumulative += model.mInstances.size();
	}

	LOG_INFO(mRenderer->mLogger, "Models Instances Buffers Offsets Realigned");
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

	LOG_INFO(mRenderer->mLogger, "Main Vertex Buffer Reloading");
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

	LOG_INFO(mRenderer->mLogger, "Main Index Buffer Reloading");
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

	LOG_INFO(mRenderer->mLogger, "Main Material Constants Buffer Reloading");
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

	LOG_INFO(mRenderer->mLogger, "Main Node Transforms Buffer Reloading");
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

	LOG_INFO(mRenderer->mLogger, "Main Instances Buffer Reloading");
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

	LOG_INFO(mRenderer->mLogger, "Main Material Resources Descriptor Set Reloaded");
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
	mFlags.modelAddedFlag = false;
	mFlags.modelDestroyedFlag = false;
	mFlags.instanceAddedFlag = false;
	mFlags.instanceDestroyedFlag = false;
	mFlags.reloadMainInstancesBuffer = false;
}

void SceneManager::cleanup()
{
	mModels.clear();
	mBatches.clear();
	LOG_INFO(mRenderer->mLogger, "All Batches Destroyed");
	mMainMaterialResourcesDescriptorSet.clear();
	LOG_INFO(mRenderer->mLogger, "Main Material Resources Descriptor Set Destroyed");
	mMainMaterialResourcesDescriptorSetLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Main Material Resources Descriptor Set Layout Destroyed");
	mMainInstancesBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Main Instances Buffer Destroyed");
	mMainNodeTransformsBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Main Node Transforms Buffer Destroyed");
	mMainMaterialConstantsBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Main Material Constants Buffer Destroyed");
	mMainIndexBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Main Index Buffer Destroyed");
	mMainVertexBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Main Vertex Buffer Destroyed");
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

Batch::Batch(Renderer* renderer, Primitive& primitive, int pipelineId)
{
	pipeline = primitive.mMaterial->mPipeline;
	renderItemsBuffer = renderer->mRendererResources.createBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mRendererCore.labelResourceDebug(renderItemsBuffer.buffer, fmt::format("RenderItemsBuffer{}", pipelineId).c_str());
	renderItemsBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*renderItemsBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Buffer Created", pipelineId);

	visibleRenderItemsBuffer = renderer->mRendererResources.createBuffer(
		MAX_RENDER_ITEMS * sizeof(RenderItem),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mRendererCore.labelResourceDebug(visibleRenderItemsBuffer.buffer, fmt::format("VisibleRenderItemsBuffer{}", pipelineId).c_str());
	visibleRenderItemsBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*visibleRenderItemsBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Visible Render Items Buffer Created", pipelineId);

	countBuffer = renderer->mRendererResources.createBuffer(
		sizeof(uint32_t),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	renderer->mRendererCore.labelResourceDebug(countBuffer.buffer, fmt::format("CountBuffer{}", pipelineId).c_str());
	countBuffer.address = renderer->mRendererCore.mDevice.getBufferAddress(vk::BufferDeviceAddressInfo(*countBuffer.buffer));
	LOG_INFO(renderer->mLogger, "Batch {} Count Buffer Created", pipelineId);

	renderItemsStagingBuffer = renderer->mRendererResources.createStagingBuffer(MAX_RENDER_ITEMS * sizeof(RenderItem));
	LOG_INFO(renderer->mLogger, "Batch {} Render Items Staging Buffer Created", pipelineId);
}

Batch::~Batch()
{
	renderItemsStagingBuffer.cleanup();
	countBuffer.cleanup();
	visibleRenderItemsBuffer.cleanup();
	renderItemsBuffer.cleanup();
	renderItems.clear();	
}