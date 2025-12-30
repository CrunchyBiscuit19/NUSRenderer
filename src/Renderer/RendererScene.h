#pragma once

#include <Data/Model.h>
#include <Scene/Batch.h>
#include <Scene/Culler.h>
#include <Scene/Perspective.h>
#include <Scene/Picker.h>
#include <Scene/Skybox.h>

#include <filesystem>
#include <vector>

struct ForwardPushConstants {
    vk::DeviceAddress vertexBuffer;
    vk::DeviceAddress materialConstantsBuffer;
    vk::DeviceAddress nodeTransformsBuffer;
    vk::DeviceAddress instancesBuffer;
    vk::DeviceAddress postCullRenderItemsBuffer;
};

class RendererScene {
    Renderer* mRenderer;

   public:
    Perspective mPerspective;
    Skybox mSkybox;
    Culler mCuller;
    Picker mPicker;

    u32 mLatestInstanceId{0};
    u32 mLatestMeshId{0};
    u32 mLatestModelId{0};

    struct Flags {
        bool modelAddedFlag;
        bool modelDestroyedFlag;
        bool instanceAddedFlag;
        bool instanceDestroyedFlag;
        bool reloadMainInstancesBuffer;
    };

    Flags mFlags;

    std::unordered_map<std::string, GLTFModel> mModelsCache;
    std::unordered_map<u32, std::string> mModelsReverse;

    std::unordered_map<u32, Batch> mOpaqueBatches;
    std::unordered_map<u32, Batch> mMaskBatches;
    std::unordered_map<u32, Batch> mTransparentBatches;
    std::array<std::unordered_map<u32, Batch>*, 3> mBatchTypes;
    ForwardPushConstants mForwardPushConstants;

    AddressedBuffer mMainVertexBuffer;
    AllocatedBuffer mMainIndexBuffer;

    AddressedBuffer mMainMaterialConstantsBuffer;
    vk::raii::DescriptorSet mMainMaterialResourcesDescriptorSet;
    vk::raii::DescriptorSetLayout mMainMaterialResourcesDescriptorSetLayout;

    AddressedBuffer mMainNodeTransformsBuffer;
    AddressedBuffer mMainInstancesBuffer;
    AddressedBuffer mMainBoundsBuffer;
    AddressedBuffer mVisibleInstancesIndicesBuffer;

    RendererScene(Renderer* renderer);

    void initBuffers();
    void initDescriptor();
    void initPushConstants();
    void initKeyBinding() const;
    void initComponents();

    void loadModels(const std::vector<std::filesystem::path>& files);
    void deleteModels();
    void deleteInstances();

    void regenerateRenderItemsInstances();

    void realignVertexIndexOffset();
    void realignMaterialOffset();
    void realignNodeTransformsOffset();
    void realignBoundsOffset();
    void realignInstancesOffset();

    void realignOffsets();

    void reloadMainVertexBuffer();
    void reloadMainIndexBuffer();
    void reloadMainMaterialConstantsBuffer();
    void reloadMainNodeTransformsBuffer();
    void reloadMainBoundsBuffer();
    void reloadMainInstancesBuffer();
    void reloadMainMaterialResourcesArray();

    void reloadMainBuffers();

    void resetFlags();

    void cleanup();
};
