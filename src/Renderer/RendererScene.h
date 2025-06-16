#pragma once

#include <Data/Model.h>
#include <Renderer/Scene/Perspective.h>
#include <Renderer/Scene/Skybox.h>
#include <Renderer/Scene/Culler.h>
#include <Renderer/Scene/Batch.h>
#include <Renderer/Scene/Picker.h>

#include <glm/fwd.hpp>

#include <vector>
#include <map>
#include <filesystem>

struct ScenePushConstants {
	vk::DeviceAddress vertexBuffer;
	vk::DeviceAddress materialConstantsBuffer;
	vk::DeviceAddress nodeTransformsBuffer;
	vk::DeviceAddress instancesBuffer;
	vk::DeviceAddress visibleRenderItemsBuffer;
};

class RendererScene {
private:
	Renderer* mRenderer;

public:
	Perspective mPerspective;
	Skybox mSkybox;
	Culler mCuller;

	int mLatestInstanceId{ 0 };
	int mLatestMeshId{ 0 };
	int mLatestModelId{ 0 };

	struct Flags {
		bool modelAddedFlag;
		bool modelDestroyedFlag;
		bool instanceAddedFlag;
		bool instanceDestroyedFlag;
		bool reloadMainInstancesBuffer;
	};
	Flags mFlags;

	std::unordered_map<std::string, GLTFModel> mModels;

	std::unordered_map<int, Batch> mBatches;
	ScenePushConstants mScenePushConstants;

	AddressedBuffer mMainVertexBuffer;
	AllocatedBuffer mMainIndexBuffer;

	AddressedBuffer mMainMaterialConstantsBuffer;
	vk::raii::DescriptorSet mMainMaterialResourcesDescriptorSet;
	vk::raii::DescriptorSetLayout mMainMaterialResourcesDescriptorSetLayout;

	AddressedBuffer mMainNodeTransformsBuffer;
	AddressedBuffer mMainInstancesBuffer;

	RendererScene(Renderer* renderer);

	void init();
	void initBuffers();
	void initDescriptor();
	void initPushConstants();

	void loadModels(const std::vector<std::filesystem::path>& files);
	void deleteModels();
	void deleteInstances();

	void regenerateRenderItems();

	void realignVertexIndexOffset();
	void realignMaterialOffset();
	void realignNodeTransformsOffset();
	void realignInstancesOffset();

	void realignOffsets();

	void reloadMainVertexBuffer();
	void reloadMainIndexBuffer();
	void reloadMainMaterialConstantsBuffer();
	void reloadMainNodeTransformsBuffer();
	void reloadMainInstancesBuffer();
	void reloadMainMaterialResourcesArray();

	void reloadMainBuffers();

	void resetFlags();

	void cleanup();
};

