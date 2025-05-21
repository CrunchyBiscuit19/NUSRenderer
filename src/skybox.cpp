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
    builder.addBinding(0, vk::DescriptorType::eUniformBuffer);
    mSkyboxDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
    mSkyboxDescriptorSet = mRenderer->mRendererInfrastructure.mDescriptorAllocator.allocate(*mSkyboxDescriptorSetLayout);
}

void Skybox::initSkyboxPipeline()
{
    vk::raii::ShaderModule fragShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "skybox.frag.spv", mRenderer->mRendererCore.mDevice); // [TODO] Write shaders
    vk::raii::ShaderModule vertexShader = vkutil::loadShaderModule(std::filesystem::path(SHADERS_PATH) / "skybox.vert.spv", mRenderer->mRendererCore.mDevice);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SkyBoxPushConstants);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    std::vector<vk::DescriptorSetLayout> descriptorLayouts = {
        *mSkyboxDescriptorSetLayout
    };
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkinit::pipelineLayoutCreateInfo();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorLayouts.data();
    pipelineLayoutCreateInfo.setLayoutCount = descriptorLayouts.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    vk::raii::PipelineLayout pipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);

    GraphicsPipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(*vertexShader, *fragShader);
    pipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
    pipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
    pipelineBuilder.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.setColorAttachmentFormat(mRenderer->mRendererInfrastructure.mDrawImage.imageFormat);
    pipelineBuilder.setDepthFormat(mRenderer->mRendererInfrastructure.mDepthImage.imageFormat);
    pipelineBuilder.enableBlendingAdditive();
    pipelineBuilder.enableDepthtest(false, vk::CompareOp::eLessOrEqual); // [TODO] Depth shite
    pipelineBuilder.mPipelineLayout = *pipelineLayout;

    mSkyboxPipeline = PipelineBundle{
        std::move(pipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice)),
        std::move(pipelineLayout)
    };
}

void Skybox::initSkyboxBuffer()
{
    mSkyboxVertices = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
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
    //initSkyboxPipeline(); // [TODO] Uncomment when shaders are written
    initSkyboxBuffer();
}

void Skybox::updateSkybox()
{
    DescriptorWriter writer;
    writer.writeBuffer(0, *mRenderer->mSceneManager.mSceneEncapsulation.mSceneBuffer.buffer, sizeof(SceneData), 0, vk::DescriptorType::eUniformBuffer); // Reuse camera data
    writer.updateSet(mRenderer->mRendererCore.mDevice, *mSkyboxDescriptorSet);
}

void Skybox::cleanup()
{
    mSkyboxImage.cleanup();
    mSkyboxDescriptorSet.clear();
    mSkyboxDescriptorSetLayout.clear();
    mSkyboxVertexBuffer.cleanup();
}