#pragma once

#include <Data/Model.h>
#include <Scene/Perspective.h>
#include <Scene/Skybox.h>
#include <Scene/Culler.h>
#include <Scene/Batch.h>
#include <Scene/Picker.h>

#include <vector>
#include <filesystem>

struct ForwardPushConstants
{
	vk::DeviceAddress vertexBuffer;
	vk::DeviceAddress materialConstantsBuffer;
	vk::DeviceAddress nodeTransformsBuffer;
	vk::DeviceAddress instancesBuffer;
	vk::DeviceAddress visibleRenderItemsBuffer;
};

class RendererScene
{
	Renderer* mRenderer;

public:
	Perspective mPerspective;
	Skybox mSkybox;
	Culler mCuller;
	Picker mPicker;

	int mLatestInstanceId{0};
	int mLatestMeshId{0};
	int mLatestModelId{0};

	struct Flags
	{
		bool modelAddedFlag;
		bool modelDestroyedFlag;
		bool instanceAddedFlag;
		bool instanceDestroyedFlag;
		bool reloadMainInstancesBuffer;
	};

	Flags mFlags;

	std::unordered_map<std::string, GLTFModel> mModelsCache;
	std::unordered_map<int, std::string> mModelsReverse;

	std::unordered_map<int, Batch> mBatches;
	ForwardPushConstants mForwardPushConstants;

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
	void initKeyBinding();

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
