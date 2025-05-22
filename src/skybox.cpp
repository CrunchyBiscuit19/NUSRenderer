#include <renderer.h>
#include <skybox.h>
#include <vk_initializers.h>

#include <stb_image.h>

Skybox::Skybox(Renderer* renderer) :
    mRenderer(renderer),
    mSkyboxDescriptorSet(nullptr),
    mSkyboxDescriptorSetLayout(nullptr)
{}

void Skybox::loadSkyboxImage(fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back)
{
    std::vector<fs::path> skyboxImagePaths = {right, left, top, bottom, front, back};
    std::vector<std::byte> skyboxImageData(MAX_IMAGE_SIZE);

    int width, height, nrChannels;
    int offset = 0;

    for (auto& path : skyboxImagePaths) {
        if (unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &nrChannels, 4)) {
            int imageSize = width * height * 4;
            std::memcpy(skyboxImageData.data() + offset, data, imageSize);
            offset += imageSize;
            stbi_image_free(data);
        }
    }

    mSkyboxImage = mRenderer->mResourceManager.createImage(skyboxImageData.data(), vk::Extent3D {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true, true);
}

void Skybox::initSkyboxDescriptor()
{
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler);
    mSkyboxDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eFragment);
    mSkyboxDescriptorSet = mRenderer->mRendererInfrastructure.mDescriptorAllocator.allocate(*mSkyboxDescriptorSetLayout);

    DescriptorWriter writer;
    writer.writeImage(0, *mSkyboxImage.imageView, *mRenderer->mResourceManager.mDefaultSampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.updateSet(mRenderer->mRendererCore.mDevice, *mSkyboxDescriptorSet);
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

    mSkyboxVertexBuffer = mRenderer->mResourceManager.createBuffer(skyboxVertexSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);

    std::memcpy(mRenderer->mResourceManager.mMeshStagingBuffer.info.pMappedData, mSkyboxVertices.data(), skyboxVertexSize);

    vk::BufferCopy skyboxVertexCopy{};
    skyboxVertexCopy.dstOffset = 0;
    skyboxVertexCopy.srcOffset = 0;
    skyboxVertexCopy.size = skyboxVertexSize;

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        cmd.copyBuffer(*mRenderer->mResourceManager.mMeshStagingBuffer.buffer, *mSkyboxVertexBuffer.buffer, skyboxVertexCopy);
    });

    vk::BufferDeviceAddressInfo skyboxVertexBufferDeviceAddressInfo;
    skyboxVertexBufferDeviceAddressInfo.buffer = *mSkyboxVertexBuffer.buffer;
    mSkyboxPushConstants.vertexBuffer = mRenderer->mRendererCore.mDevice.getBufferAddress(skyboxVertexBufferDeviceAddressInfo);
}

void Skybox::init(fs::path right, fs::path left, fs::path top, fs::path bottom, fs::path front, fs::path back)
{
    loadSkyboxImage(right, left, top, bottom, front, back);
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