#include <Renderer/Renderer.h>
#include <Data/Skybox.h>
#include <Utils/Helper.h>

#include <stb_image.h>

Skybox::Skybox(Renderer* renderer) :
	mRenderer(renderer),
	mSkyboxDescriptorSet(nullptr),
	mSkyboxDescriptorSetLayout(nullptr),
	mSkyboxPipelineLayout(nullptr)
{
}

void Skybox::init(std::optional<std::filesystem::path> skyboxDir)
{
	if (skyboxDir.has_value()) { loadSkyboxImage(skyboxDir.value()); }
	initSkyboxDescriptor();
	initSkyboxPipelineLayout();
	initSkyboxPipeline();
	initSkyboxBuffer();
}

void Skybox::initSkyboxDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler);
	mSkyboxDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eFragment);
	mSkyboxDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mSkyboxDescriptorSetLayout);
	LOG_INFO(mRenderer->mLogger, "Skybox Descriptor Set and Layout Created");

	setSkyboxBindings();
}

void Skybox::initSkyboxPipelineLayout()
{
	vk::PushConstantRange skyBoxPushConstantRange{};
	skyBoxPushConstantRange.offset = 0;
	skyBoxPushConstantRange.size = sizeof(SkyBoxPushConstants);
	skyBoxPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> skyboxDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mPerspectiveDescriptorSetLayout,
		mSkyboxDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pipelineLayoutCreateInfo.pSetLayouts = skyboxDescriptorLayouts.data();
	pipelineLayoutCreateInfo.setLayoutCount = skyboxDescriptorLayouts.size();
	pipelineLayoutCreateInfo.pPushConstantRanges = &skyBoxPushConstantRange;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mSkyboxPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mSkyboxPipelineLayout, "SkyboxPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Layout Created");
}

void Skybox::initSkyboxPipeline()
{
	vk::ShaderModule fragShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "skybox/skybox.frag.spv");
	vk::ShaderModule vertexShader = mRenderer->mRendererResources.getShader(std::filesystem::path(SHADERS_PATH) / "skybox/skybox.vert.spv");

	GraphicsPipelineBuilder skyboxPipelineBuilder;
	skyboxPipelineBuilder.setShaders(vertexShader, fragShader);
	skyboxPipelineBuilder.setInputTopology(vk::PrimitiveTopology::eTriangleList);
	skyboxPipelineBuilder.setPolygonMode(vk::PolygonMode::eFill);
	skyboxPipelineBuilder.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise);
	skyboxPipelineBuilder.enableMultisampling();
	skyboxPipelineBuilder.enableSampleShading();
	skyboxPipelineBuilder.enableBlendingSkybox();
	skyboxPipelineBuilder.setColorAttachmentFormat(mRenderer->mRendererInfrastructure.mDrawImage.imageFormat);
	skyboxPipelineBuilder.setDepthFormat(mRenderer->mRendererInfrastructure.mDepthImage.imageFormat);
	skyboxPipelineBuilder.enableDepthtest(false, vk::CompareOp::eGreaterOrEqual);
	skyboxPipelineBuilder.mPipelineLayout = *mRenderer->mRendererScene.mSkybox.mSkyboxPipelineLayout;

	mSkyboxPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId,
		skyboxPipelineBuilder.createPipeline(mRenderer->mRendererCore.mDevice),
		*mSkyboxPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mSkyboxPipelineBundle.pipeline, "SkyboxPipeline");
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Created");

	mRenderer->mRendererInfrastructure.mLatestPipelineId++;
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
	mRenderer->mRendererCore.labelResourceDebug(mSkyboxVertexBuffer.buffer, "SkyboxVertexBuffer");
	LOG_INFO(mRenderer->mLogger, "Skybox Vertex Buffer Created");

	std::memcpy(mRenderer->mRendererResources.mMeshStagingBuffer.info.pMappedData, mSkyboxVertices.data(), skyboxVertexSize);

	vk::BufferCopy skyboxVertexCopy{};
	skyboxVertexCopy.dstOffset = 0;
	skyboxVertexCopy.srcOffset = 0;
	skyboxVertexCopy.size = skyboxVertexSize;

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		cmd.copyBuffer(*mRenderer->mRendererResources.mMeshStagingBuffer.buffer, *mSkyboxVertexBuffer.buffer, skyboxVertexCopy);
	});
	LOG_INFO(mRenderer->mLogger, "Skybox Vertex Buffer Uploading");

	vk::BufferDeviceAddressInfo skyboxVertexBufferDeviceAddressInfo;
	skyboxVertexBufferDeviceAddressInfo.buffer = *mSkyboxVertexBuffer.buffer;
	mSkyboxPushConstants.vertexBuffer = mRenderer->mRendererCore.mDevice.getBufferAddress(skyboxVertexBufferDeviceAddressInfo);
}

void Skybox::setSkyboxBindings()
{
	DescriptorSetBinder writer;
	writer.bindImage(0, *mSkyboxImage.imageView, mRenderer->mRendererResources.getSampler(vk::SamplerCreateInfo()), vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mSkyboxDescriptorSet);
}

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
		vk::Extent3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageUsageFlagBits::eSampled,
		true, false, true);

	LOG_INFO(mRenderer->mLogger, "Skybox Loaded");
}

void Skybox::updateSkyboxImage(std::filesystem::path skyboxDir)
{
	AllocatedImage oldSkyboxImage = std::move(mSkyboxImage);
	loadSkyboxImage(skyboxDir);
	setSkyboxBindings();
	oldSkyboxImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Image Updated");
}

void Skybox::cleanup()
{
	mSkyboxImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Image Destroyed");
	mSkyboxDescriptorSet.clear();
	mSkyboxDescriptorSetLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Skybox Descriptor Set and Layout Destroyed");
	mSkyboxVertexBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Vertex Buffer Destroyed");
	mSkyboxPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Destroyed");
	mSkyboxPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Layout Destroyed");
}