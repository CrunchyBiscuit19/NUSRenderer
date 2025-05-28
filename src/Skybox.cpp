#include <Renderer.h>
#include <Skybox.h>
#include <Helper.h>

#include <stb_image.h>

Skybox::Skybox(Renderer* renderer) :
    mRenderer(renderer),
    mSkyboxDescriptorSet(nullptr),
    mSkyboxDescriptorSetLayout(nullptr)
{}

void Skybox::loadSkyboxImage(std::filesystem::path skyboxImageDir)
{
    std::vector<std::filesystem::path> skyboxImagePaths = { skyboxImageDir / "px.png", skyboxImageDir / "nx.png", skyboxImageDir / "py.png", skyboxImageDir / "ny.png", skyboxImageDir / "pz.png", skyboxImageDir / "nz.png" };
    std::vector<std::byte> skyboxImageData(MAX_IMAGE_SIZE);

    int width = 0, height = 0, nrChannels = 0;
    int offset = 0;

    for (auto& path : skyboxImagePaths) {
        if (unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &nrChannels, 4)) {
            int imageSize = width * height * 4;
            std::memcpy(skyboxImageData.data() + offset, data, imageSize);
            offset += imageSize;
            stbi_image_free(data);
        }
    }

    mSkyboxImage = mRenderer->mRendererResources.createImage(
        skyboxImageData.data(), 
        vk::Extent3D {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}, 
        vk::Format::eR8G8B8A8Unorm, 
        vk::ImageUsageFlagBits::eSampled, 
        true, false, true);
}

void Skybox::initSkyboxDescriptor()
{
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler);
    mSkyboxDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eFragment);
    mSkyboxDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mSkyboxDescriptorSetLayout);

    setSkyboxBindings();
}

void Skybox::initSkyboxPipeline()
{
    mRenderer->mRendererInfrastructure.createSkyboxPipeline();
}

void Skybox::initSkyboxBuffer()
{
    mSkyboxVertices = {
        -1.0f,  1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, -1.0f, 1.0f,
         1.0f,  1.0f, -1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f, 1.0f,

        -1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, 1.0f,
                             
         1.0f, -1.0f, -1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, -1.0f, 1.0f,
                             
        -1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, 1.0f,
                             
        -1.0f,  1.0f, -1.0f, 1.0f,
         1.0f,  1.0f, -1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f, 1.0f,
                             
        -1.0f, -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, 1.0f,
         1.0f, -1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
    };

    int skyboxVertexSize = mSkyboxVertices.size() * sizeof(float);

    mSkyboxVertexBuffer = mRenderer->mRendererResources.createBuffer(skyboxVertexSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);

    std::memcpy(mRenderer->mRendererResources.mMeshStagingBuffer.info.pMappedData, mSkyboxVertices.data(), skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        cmd.copyBuffer(*mRenderer->mRendererResources.mMeshStagingBuffer.buffer, *mSkyboxVertexBuffer.buffer, skyboxVertexCopy);
    });

    vk::BufferDeviceAddressInfo skyboxVertexBufferDeviceAddressInfo;
    skyboxVertexBufferDeviceAddressInfo.buffer = *mSkyboxVertexBuffer.buffer;
    mSkyboxPushConstants.vertexBuffer = mRenderer->mRendererCore.mDevice.getBufferAddress(skyboxVertexBufferDeviceAddressInfo);
}

void Skybox::setSkyboxBindings()
{
    DescriptorSetBinder writer;
    writer.bindImage(0, *mSkyboxImage.imageView, *mRenderer->mRendererResources.mDefaultSampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mSkyboxDescriptorSet);
}

void Skybox::updateSkyboxImage(std::filesystem::path skyboxDir)
{
    AllocatedImage oldSkyboxImage = std::move(mSkyboxImage);
    loadSkyboxImage(skyboxDir);
    setSkyboxBindings();
    oldSkyboxImage.cleanup();
}

void Skybox::init(std::filesystem::path skyboxDir)
{
    loadSkyboxImage(skyboxDir);
    initSkyboxDescriptor();
    initSkyboxPipeline(); 
    initSkyboxBuffer();
}

void Skybox::cleanup()
{
    mSkyboxImage.cleanup();
    mSkyboxDescriptorSet.clear();
    mSkyboxDescriptorSetLayout.clear();
    mSkyboxVertexBuffer.cleanup();
    mSkyboxPipeline.cleanup();
}