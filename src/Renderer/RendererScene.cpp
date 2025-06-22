#include <Renderer/RendererScene.h>
#include <Renderer/Renderer.h>
#include <Utils/Helper.h>

#include <quill/LogMacros.h>

#include <ranges>

RendererScene::RendererScene(Renderer* renderer) :
	mRenderer(renderer),
	mPerspective(Perspective(renderer)),
	mSkybox(Skybox(renderer)),
	mCuller(Culler(renderer)),
	mPicker(Picker(renderer)),
	mMainMaterialResourcesDescriptorSet(nullptr),
	mMainMaterialResourcesDescriptorSetLayout(nullptr)
{
}

void RendererScene::init()
{
	initBuffers();
	initDescriptor();
	initPushConstants();

	mPerspective.init();
	mSkybox.init(std::filesystem::path(std::string(SKYBOXES_PATH) + "ocean/"));
	mCuller.init();
	mPicker.init();
}

void RendererScene::initBuffers()
{
	mMainVertexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_VERTEX_BUFFER_SIZE,
	                                                               vk::BufferUsageFlagBits::eTransferDst |
	                                                               vk::BufferUsageFlagBits::eStorageBuffer |
	                                                               vk::BufferUsageFlagBits::eShaderDeviceAddress,
	                                                               VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainVertexBuffer.buffer, "MainVertexBuffer");
	mMainVertexBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*mMainVertexBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Vertex Buffer Created");

	mMainIndexBuffer = mRenderer->mRendererResources.createBuffer(MAIN_INDEX_BUFFER_SIZE,
	                                                              vk::BufferUsageFlagBits::eTransferDst |
	                                                              vk::BufferUsageFlagBits::eIndexBuffer,
	                                                              VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainIndexBuffer.buffer, "MainIndexBuffer");
	LOG_INFO(mRenderer->mLogger, "Main Index Buffer Created");

	mMainMaterialConstantsBuffer = mRenderer->mRendererResources.createBuffer(
		MAX_MATERIALS * sizeof(MaterialConstants),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialConstantsBuffer.buffer, "MainMaterialConstantsBuffer");
	mMainMaterialConstantsBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*mMainMaterialConstantsBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Material Constants Buffer Created");

	mMainNodeTransformsBuffer = mRenderer->mRendererResources.createBuffer(
		MAX_NODES * sizeof(glm::mat4),
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer |
		vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainNodeTransformsBuffer.buffer, "MainNodeTransformsBuffer");
	mMainNodeTransformsBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*mMainNodeTransformsBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Node Transforms Buffer Created");

	mMainInstancesBuffer = mRenderer->mRendererResources.createBuffer(MAX_INSTANCES * sizeof(InstanceData),
	                                                                  vk::BufferUsageFlagBits::eTransferDst |
	                                                                  vk::BufferUsageFlagBits::eStorageBuffer |
	                                                                  vk::BufferUsageFlagBits::eShaderDeviceAddress,
	                                                                  VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMainInstancesBuffer.buffer, "MainInstancesBuffer");
	mMainInstancesBuffer.address = mRenderer->mRendererCore.mDevice.getBufferAddress(
		vk::BufferDeviceAddressInfo(*mMainInstancesBuffer.buffer));
	LOG_INFO(mRenderer->mLogger, "Main Instances Buffer Created");
}

void RendererScene::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, MAX_TEXTURE_ARRAY_SLOTS);
	mMainMaterialResourcesDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice,
	                                                          vk::ShaderStageFlagBits::eVertex |
	                                                          vk::ShaderStageFlagBits::eFragment, true);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialResourcesDescriptorSetLayout,
	                                            "MainMaterialResourcesDescriptorSetLayout");
	mMainMaterialResourcesDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(
		mMainMaterialResourcesDescriptorSetLayout, true);
	mRenderer->mRendererCore.labelResourceDebug(mMainMaterialResourcesDescriptorSet,
	                                            "MainMaterialResourcesDescriptorSet");
	LOG_INFO(mRenderer->mLogger, "Main Material Resources and Descriptor Set Created");
}

void RendererScene::initPushConstants()
{
	mForwardPushConstants.vertexBuffer = mMainVertexBuffer.address;
	mForwardPushConstants.materialConstantsBuffer = mMainMaterialConstantsBuffer.address;
	mForwardPushConstants.nodeTransformsBuffer = mMainNodeTransformsBuffer.address;
	mForwardPushConstants.instancesBuffer = mMainInstancesBuffer.address;
	LOG_INFO(mRenderer->mLogger, "Scene Push Constants Initialized");
}

void RendererScene::loadModels(const std::vector<std::filesystem::path>& paths)
{
	for (const auto& modelPath : paths)
	{
		auto modelShortPath = modelPath.stem().string();
		auto modelFullPath = MODELS_PATH / modelPath;
		auto [_, inserted] = mModelsCache.try_emplace(modelShortPath, mRenderer, modelFullPath);
		if (inserted) { 
			mModelsReverse.try_emplace(mModelsCache.at(modelShortPath).mId, modelShortPath);
			mFlags.modelAddedFlag = true;
		}
	}
}

void RendererScene::deleteModels()
{
	std::erase_if(mModelsCache, [&](const std::pair<const std::string, GLTFModel>& pair)
	{
		if (pair.second.mDeleteSignal.has_value()) { mFlags.modelDestroyedFlag = true; }
		return pair.second.mDeleteSignal.has_value() && (pair.second.mDeleteSignal.value() == mRenderer->
			mRendererInfrastructure.mFrameNumber);
	});
}

void RendererScene::deleteInstances()
{
	for (auto& model : mModelsCache | std::views::values)
	{
		std::erase_if(model.mInstances, [&](const GLTFInstance& instance)
		{
			return instance.mDeleteSignal;
		});
	}
}

void RendererScene::regenerateRenderItems()
{
	for (auto& batch : mBatches | std::views::values)
	{
		batch.renderItems.clear();
	}

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		model.generateRenderItems();
	}

	LOG_INFO(mRenderer->mLogger, "Render Items Regenerated");

	for (auto& batch : mBatches | std::views::values)
	{
		if (batch.renderItems.empty()) { continue; }

		std::memcpy(batch.renderItemsStagingBuffer.info.pMappedData, batch.renderItems.data(),
		            batch.renderItems.size() * sizeof(RenderItem));

		vk::BufferCopy renderItemsCopy{};
		renderItemsCopy.dstOffset = 0;
		renderItemsCopy.srcOffset = 0;
		renderItemsCopy.size = batch.renderItems.size() * sizeof(RenderItem);

		mRenderer->mImmSubmit.mCallbacks.push_back([&batch, renderItemsCopy](Renderer* renderer, vk::CommandBuffer cmd)
		{
			cmd.fillBuffer(*batch.renderItemsBuffer.buffer, 0, vk::WholeSize, 0);

			vkhelper::createBufferPipelineBarrier( // Wait for render items buffer to be flushed
				cmd,
				*batch.renderItemsBuffer.buffer,
				vk::PipelineStageFlagBits2::eTransfer,
				vk::AccessFlagBits2::eTransferWrite,
				vk::PipelineStageFlagBits2::eTransfer,
				vk::AccessFlagBits2::eTransferWrite);

			cmd.copyBuffer(*batch.renderItemsStagingBuffer.buffer, *batch.renderItemsBuffer.buffer, renderItemsCopy);

			vkhelper::createBufferPipelineBarrier( // Wait for render items to finish uploading 
				cmd,
				*batch.renderItemsBuffer.buffer,
				vk::PipelineStageFlagBits2::eTransfer,
				vk::AccessFlagBits2::eTransferWrite,
				vk::PipelineStageFlagBits2::eComputeShader,
				vk::AccessFlagBits2::eShaderRead);
		});

		LOG_INFO(mRenderer->mLogger, "Batch {} Render Items Uploading", batch.pipelineBundle->id);
	}
}

void RendererScene::realignVertexIndexOffset()
{
	int vertexCumulative = 0;
	int indexCumulative = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		for (auto& mesh : model.mMeshes)
		{
			mesh.mMainVertexOffset = vertexCumulative;
			mesh.mMainFirstIndex = indexCumulative;
			vertexCumulative += mesh.mNumVertices;
			indexCumulative += mesh.mNumIndices;
		}
	}

	LOG_INFO(mRenderer->mLogger, "Models Vertex / Index Buffers Offsets Realigned");
}

void RendererScene::realignMaterialOffset()
{
	int materialCumulative = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstMaterial = materialCumulative;
		materialCumulative += model.mMaterials.size();
	}

	LOG_INFO(mRenderer->mLogger, "Models Material Constants Buffer / Resources Descriptor Set Offsets Realigned");
}

void RendererScene::realignNodeTransformsOffset()
{
	int nodeTransformCumulative = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstNodeTransform = nodeTransformCumulative;
		nodeTransformCumulative += model.mNodes.size();
	}

	LOG_INFO(mRenderer->mLogger, "Models Node Transforms Buffer Offsets Realigned");
}

void RendererScene::realignInstancesOffset()
{
	int instanceCumulative = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		model.mMainFirstInstance = instanceCumulative;
		instanceCumulative += model.mInstances.size();
	}

	LOG_INFO(mRenderer->mLogger, "Models Instances Buffers Offsets Realigned");
}

void RendererScene::realignOffsets()
{
	realignVertexIndexOffset();
	realignMaterialOffset();
	realignNodeTransformsOffset();
	realignInstancesOffset();
}

void RendererScene::reloadMainVertexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		for (auto& mesh : model.mMeshes)
		{
			vk::BufferCopy meshVertexCopy{};
			meshVertexCopy.dstOffset = dstOffset;
			meshVertexCopy.srcOffset = 0;
			meshVertexCopy.size = mesh.mNumVertices * sizeof(Vertex);

			dstOffset += meshVertexCopy.size;

			mRenderer->mImmSubmit.mCallbacks.push_back(
				[&mesh, this, meshVertexCopy](Renderer* renderer, vk::CommandBuffer cmd)
				{
					cmd.copyBuffer(*mesh.mVertexBuffer.buffer, *mMainVertexBuffer.buffer, meshVertexCopy);
				});
		}
	}

	mRenderer->mImmSubmit.mCallbacks.push_back([this](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier( // Wait for main vertex buffer to finish uploading
			cmd,
			*mMainVertexBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eVertexShader,
			vk::AccessFlagBits2::eShaderRead);
	});

	LOG_INFO(mRenderer->mLogger, "Main Vertex Buffer Reloading");
}

void RendererScene::reloadMainIndexBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		for (auto& mesh : model.mMeshes)
		{
			vk::BufferCopy meshIndexCopy{};
			meshIndexCopy.dstOffset = dstOffset;
			meshIndexCopy.srcOffset = 0;
			meshIndexCopy.size = mesh.mNumIndices * sizeof(uint32_t);

			dstOffset += meshIndexCopy.size;

			mRenderer->mImmSubmit.mCallbacks.push_back(
				[&mesh, this, meshIndexCopy](Renderer* renderer, vk::CommandBuffer cmd)
				{
					cmd.copyBuffer(*mesh.mIndexBuffer.buffer, *mMainIndexBuffer.buffer, meshIndexCopy);
				});
		}
	}

	mRenderer->mImmSubmit.mCallbacks.push_back([this](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier( // Wait for main index buffer to finish uploading
			cmd,
			*mMainIndexBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eVertexShader,
			vk::AccessFlagBits2::eShaderRead);
	});

	LOG_INFO(mRenderer->mLogger, "Main Index Buffer Reloading");
}

void RendererScene::reloadMainMaterialConstantsBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		vk::BufferCopy materialConstantCopy{};
		materialConstantCopy.dstOffset = dstOffset;
		materialConstantCopy.srcOffset = 0;
		materialConstantCopy.size = model.mMaterials.size() * sizeof(MaterialConstants);

		dstOffset += materialConstantCopy.size;

		mRenderer->mImmSubmit.mCallbacks.push_back(
			[&model, this, materialConstantCopy](Renderer* renderer, vk::CommandBuffer cmd)
			{
				cmd.copyBuffer(*model.mMaterialConstantsBuffer.buffer, *mMainMaterialConstantsBuffer.buffer,
				               materialConstantCopy);
			});
	}

	mRenderer->mImmSubmit.mCallbacks.push_back([this](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier( // Wait for main material constants buffer to finish uploading
			cmd,
			*mMainMaterialConstantsBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eVertexShader,
			vk::AccessFlagBits2::eShaderRead);
	});

	LOG_INFO(mRenderer->mLogger, "Main Material Constants Buffer Reloading");
}

void RendererScene::reloadMainNodeTransformsBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }

		vk::BufferCopy nodeTransformsCopy{};
		nodeTransformsCopy.dstOffset = dstOffset;
		nodeTransformsCopy.srcOffset = 0;
		nodeTransformsCopy.size = model.mNodes.size() * sizeof(glm::mat4);

		dstOffset += nodeTransformsCopy.size;

		mRenderer->mImmSubmit.mCallbacks.push_back(
			[&model, this, nodeTransformsCopy](Renderer* renderer, vk::CommandBuffer cmd)
			{
				cmd.copyBuffer(*model.mNodeTransformsBuffer.buffer, *mMainNodeTransformsBuffer.buffer,
				               nodeTransformsCopy);
			});
	}

	mRenderer->mImmSubmit.mCallbacks.push_back([this](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier( // Wait for main node transforms buffer to finish uploading
			cmd,
			*mMainNodeTransformsBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eVertexShader,
			vk::AccessFlagBits2::eShaderRead);
	});

	LOG_INFO(mRenderer->mLogger, "Main Node Transforms Buffer Reloading");
}

void RendererScene::reloadMainInstancesBuffer()
{
	int dstOffset = 0;

	for (auto& model : mModelsCache | std::views::values)
	{
		if (model.mDeleteSignal.has_value()) { continue; }
		if (model.mInstances.empty()) { continue; }

		vk::BufferCopy instancesCopy{};
		instancesCopy.dstOffset = dstOffset;
		instancesCopy.srcOffset = 0;
		instancesCopy.size = model.mInstances.size() * sizeof(InstanceData);

		dstOffset += instancesCopy.size;

		mRenderer->mImmSubmit.mCallbacks.push_back(
			[&model, this, instancesCopy](Renderer* renderer, vk::CommandBuffer cmd)
			{
				cmd.copyBuffer(*model.mInstancesBuffer.buffer, *mMainInstancesBuffer.buffer, instancesCopy);
			});
	}

	mRenderer->mImmSubmit.mCallbacks.push_back([this](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier( // Wait for main instances buffer to finish uploading
			cmd,
			*mMainInstancesBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eVertexShader,
			vk::AccessFlagBits2::eShaderRead);
	});

	LOG_INFO(mRenderer->mLogger, "Main Instances Buffer Reloading");
}

void RendererScene::reloadMainMaterialResourcesArray()
{
	for (auto& model : mModelsCache | std::views::values)
	{
		for (auto& material : model.mMaterials)
		{
			int materialTextureArrayIndex = (model.mMainFirstMaterial + material.mRelativeMaterialIndex) * 5;

			DescriptorSetBinder writer;
			writer.bindImageArray(0, materialTextureArrayIndex + 0, *material.mPbrData.resources.base.image->imageView,
			                      material.mPbrData.resources.base.sampler, vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 1,
			                      *material.mPbrData.resources.metallicRoughness.image->imageView,
			                      material.mPbrData.resources.metallicRoughness.sampler,
			                      vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 2,
			                      *material.mPbrData.resources.emissive.image->imageView,
			                      material.mPbrData.resources.emissive.sampler, vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 3,
			                      *material.mPbrData.resources.normal.image->imageView,
			                      material.mPbrData.resources.normal.sampler, vk::ImageLayout::eShaderReadOnlyOptimal,
			                      vk::DescriptorType::eCombinedImageSampler);
			writer.bindImageArray(0, materialTextureArrayIndex + 4,
			                      *material.mPbrData.resources.occlusion.image->imageView,
			                      material.mPbrData.resources.occlusion.sampler,
			                      vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
			writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mMainMaterialResourcesDescriptorSet);
		}
	}

	LOG_INFO(mRenderer->mLogger, "Main Material Resources Descriptor Set Reloaded");
}

void RendererScene::reloadMainBuffers()
{
	reloadMainVertexBuffer();
	reloadMainIndexBuffer();
	reloadMainMaterialConstantsBuffer();
	reloadMainInstancesBuffer();
	reloadMainNodeTransformsBuffer();
	reloadMainMaterialResourcesArray();
}

void RendererScene::resetFlags()
{
	mFlags.modelAddedFlag = false;
	mFlags.modelDestroyedFlag = false;
	mFlags.instanceAddedFlag = false;
	mFlags.instanceDestroyedFlag = false;
	mFlags.reloadMainInstancesBuffer = false;
}

void RendererScene::cleanup()
{
	mPerspective.cleanup();
	mSkybox.cleanup();
	mCuller.cleanup();
	mPicker.cleanup();

	mModelsCache.clear();
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
