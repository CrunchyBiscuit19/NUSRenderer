#include <Renderer/Renderer.h>
#include <Renderer/Scene/Skybox.h>
#include <Utils/Helper.h>

#include <stb_image.h>

Skybox::Skybox(Renderer* renderer) :
	mRenderer(renderer),
	mDescriptorSet(nullptr),
	mDescriptorSetLayout(nullptr),
	mPipelineLayout(nullptr)
{
}

void Skybox::init(std::optional<std::filesystem::path> skyboxDir)
{
	if (skyboxDir.has_value()) { loadImage(skyboxDir.value()); }
	initDescriptor();
	initPipelineLayout();
	initPipeline();
	initBuffer();
}

void Skybox::initDescriptor()
{
	DescriptorLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler);
	mDescriptorSetLayout = builder.build(mRenderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eFragment);
	mDescriptorSet = mRenderer->mRendererInfrastructure.mMainDescriptorAllocator.allocate(*mDescriptorSetLayout);
	LOG_INFO(mRenderer->mLogger, "Skybox Descriptor Set and Layout Created");

	setBindings();
}

void Skybox::initPipelineLayout()
{
	vk::PushConstantRange skyBoxPushConstantRange{};
	skyBoxPushConstantRange.offset = 0;
	skyBoxPushConstantRange.size = sizeof(SkyBoxPushConstants);
	skyBoxPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	std::vector<vk::DescriptorSetLayout> skyboxDescriptorLayouts = {
		*mRenderer->mRendererScene.mPerspective.mDescriptorSetLayout,
		mDescriptorSetLayout
	};
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkhelper::pipelineLayoutCreateInfo();
	pipelineLayoutCreateInfo.pSetLayouts = skyboxDescriptorLayouts.data();
	pipelineLayoutCreateInfo.setLayoutCount = skyboxDescriptorLayouts.size();
	pipelineLayoutCreateInfo.pPushConstantRanges = &skyBoxPushConstantRange;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

	mPipelineLayout = mRenderer->mRendererCore.mDevice.createPipelineLayout(pipelineLayoutCreateInfo);
	mRenderer->mRendererCore.labelResourceDebug(mPipelineLayout, "SkyboxPipelineLayout");
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Layout Created");
}

void Skybox::initPipeline()
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
	skyboxPipelineBuilder.enableDepthTest(false, vk::CompareOp::eGreaterOrEqual);
	skyboxPipelineBuilder.mPipelineLayout = *mRenderer->mRendererScene.mSkybox.mPipelineLayout;

	mPipelineBundle = PipelineBundle(
		mRenderer->mRendererInfrastructure.mLatestPipelineId,
		skyboxPipelineBuilder.buildPipeline(mRenderer->mRendererCore.mDevice),
		*mPipelineLayout
	);
	mRenderer->mRendererCore.labelResourceDebug(mPipelineBundle.pipeline, "SkyboxPipeline");
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Created");

	mRenderer->mRendererInfrastructure.mLatestPipelineId++;
}

void Skybox::initBuffer()
{
	mVertices = {
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

	int skyboxVertexSize = mVertices.size() * sizeof(float);

	mVertexBuffer = mRenderer->mRendererResources.createBuffer(skyboxVertexSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mVertexBuffer.buffer, "SkyboxVertexBuffer");
	LOG_INFO(mRenderer->mLogger, "Skybox Vertex Buffer Created");

	std::memcpy(mRenderer->mRendererResources.mMeshStagingBuffer.info.pMappedData, mVertices.data(), skyboxVertexSize);

	vk::BufferCopy skyboxVertexCopy{};
	skyboxVertexCopy.dstOffset = 0;
	skyboxVertexCopy.srcOffset = 0;
	skyboxVertexCopy.size = skyboxVertexSize;

	mRenderer->mImmSubmit.mCallbacks.push_back([this, skyboxVertexCopy](Renderer* renderer, vk::CommandBuffer cmd) {
		vkhelper::createBufferPipelineBarrier(
			cmd,
			*renderer->mRendererResources.mMeshStagingBuffer.buffer,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);

		cmd.copyBuffer(*renderer->mRendererResources.mMeshStagingBuffer.buffer, *mVertexBuffer.buffer, skyboxVertexCopy);
	
		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mVertexBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eVertexShader,
			vk::AccessFlagBits2::eShaderRead
		);
	});
	LOG_INFO(mRenderer->mLogger, "Skybox Vertex Buffer Uploading");

	vk::BufferDeviceAddressInfo skyboxVertexBufferDeviceAddressInfo;
	skyboxVertexBufferDeviceAddressInfo.buffer = *mVertexBuffer.buffer;
	mPushConstants.vertexBuffer = mRenderer->mRendererCore.mDevice.getBufferAddress(skyboxVertexBufferDeviceAddressInfo);
}

void Skybox::setBindings()
{
	DescriptorSetBinder writer;
	writer.bindImage(0, *mImage.imageView, mRenderer->mRendererResources.getSampler(vk::SamplerCreateInfo()), vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
	writer.updateSetBindings(mRenderer->mRendererCore.mDevice, *mDescriptorSet);
}

void Skybox::loadImage(std::filesystem::path skyboxImageDir)
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

	mImage = mRenderer->mRendererResources.createImage(
		skyboxImageData.data(),
		vk::Extent3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageUsageFlagBits::eSampled,
		true, false, true);

	LOG_INFO(mRenderer->mLogger, "Skybox Loaded");
}

void Skybox::updateImage(std::filesystem::path skyboxDir)
{
	AllocatedImage oldSkyboxImage = std::move(mImage);
	loadImage(skyboxDir);
	setBindings();
	oldSkyboxImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Image Updated");
}

void Skybox::cleanup()
{
	mImage.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Image Destroyed");
	mDescriptorSet.clear();
	mDescriptorSetLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Skybox Descriptor Set and Layout Destroyed");
	mVertexBuffer.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Vertex Buffer Destroyed");
	mPipelineBundle.cleanup();
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Destroyed");
	mPipelineLayout.clear();
	LOG_INFO(mRenderer->mLogger, "Skybox Pipeline Layout Destroyed");
}