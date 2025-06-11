#include <Renderer/Renderer.h>
#include <Data/Model.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtx/euler_angles.hpp>
#include <fmt/core.h>

GLTFModel::GLTFModel(Renderer* renderer, std::filesystem::path modelPath) :
	mRenderer(renderer),
	mModelDescriptorAllocator(DescriptorAllocatorGrowable(renderer))
{
	mName = modelPath.stem().string();
	LOG_INFO(mRenderer->mLogger, "{} Open GLTF / GLB File", mName);

	mId = mRenderer->mRendererScene.mLatestModelId;
	mRenderer->mRendererScene.mLatestModelId++;

	fastgltf::Parser parser{};
	fastgltf::Asset gltf;
	fastgltf::GltfDataBuffer data;
	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

	data.loadFromFile(modelPath);

	auto type = fastgltf::determineGltfFileType(&data);
	if (type == fastgltf::GltfType::Invalid) {
		LOG_ERROR(mRenderer->mLogger, "{} Failed to determine GLTF Container", mName);
	}
	auto load = (type == fastgltf::GltfType::glTF) ? (parser.loadGLTF(&data, modelPath.parent_path(), gltfOptions)) : (parser.loadBinaryGLTF(&data, modelPath.parent_path(), gltfOptions));
	if (load) { 
		gltf = std::move(load.get()); 
	} else { 
		LOG_ERROR(mRenderer->mLogger, "{} Failed to load GLTF Model: {}", mName, fastgltf::to_underlying(load.error()));
	}

	mAsset = std::move(gltf);

	this->load();

	createInstance();
}

vk::Filter GLTFModel::extractFilter(fastgltf::Filter filter)
{
	switch (filter) {
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return vk::Filter::eNearest;

	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return vk::Filter::eLinear;
	}
}

vk::SamplerMipmapMode GLTFModel::extractMipmapMode(fastgltf::Filter filter)
{
	switch (filter) {
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return vk::SamplerMipmapMode::eNearest;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return vk::SamplerMipmapMode::eLinear;
	}
}

AllocatedImage GLTFModel::loadImage(fastgltf::Image& image)
{
	AllocatedImage newImage;
	int width, height, nrChannels;

	std::visit(fastgltf::visitor{
		// Image stored outside of GLTF / GLB file.
		[&](const fastgltf::sources::URI& filePath) {
			assert(filePath.fileByteOffset == 0);
			assert(filePath.uri.isLocalPath());
			const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
			if (unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4)) {
				vk::Extent3D imagesize;
				imagesize.width = width;
				imagesize.height = height;
				imagesize.depth = 1;
				newImage = mRenderer->mRendererResources.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
				stbi_image_free(data);
			}
		},
		// Image is loaded directly into a std::vector. If the texture is on base64, or if we instruct it to load external image files (fastgltf::Options::LoadExternalImages).
		[&](const fastgltf::sources::Vector& vector) {
			if (unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
					&width, &height, &nrChannels, 4)) {
				vk::Extent3D imagesize;
				imagesize.width = width;
				imagesize.height = height;
				imagesize.depth = 1;
				newImage = mRenderer->mRendererResources.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
				stbi_image_free(data);
			}
		},
		// Image embedded into the binary GLB file.
		[&](const fastgltf::sources::BufferView& view) {
			const auto& bufferView = mAsset.bufferViews[view.bufferViewIndex];
			auto& buffer = mAsset.buffers[bufferView.bufferIndex];
			std::visit(fastgltf::visitor { [&](const fastgltf::sources::Vector& vector) {
						if (unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4)) {
							vk::Extent3D imagesize;
							imagesize.width = width;
							imagesize.height = height;
							imagesize.depth = 1;
							newImage = mRenderer->mRendererResources.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
							stbi_image_free(data);
						}
					},
					[](const auto& arg) {},
				},
			buffer.data);
		},
		[](const auto& arg) {},
		}, image.data);
	// Move the lambda taking const auto to the bottom. Otherwise it always get runs and the other lambdas don't.
	// Needs to be exactly const auto&?
	// No idea why it's even needed in the first place.

	return newImage;
}

void GLTFModel::assignBase(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material)
{
    constants.baseFactor = glm::vec4(material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1], material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3]);

    resources.base = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
    if (material.pbrData.baseColorTexture.has_value()) {
        auto img = mAsset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex;
        auto sampler = mAsset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex;
        if (img.has_value())
            resources.base.image = &mImages[img.value()];
        if (sampler.has_value())
            resources.base.sampler = *mSamplers[sampler.value()];
    }
}

void GLTFModel::assignMetallicRoughness(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material)
{
    constants.metallicRoughnessFactor = glm::vec2(material.pbrData.metallicFactor, material.pbrData.roughnessFactor);

    resources.metallicRoughness = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
    if (material.pbrData.metallicRoughnessTexture.has_value()) {
        auto img = mAsset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex;
        auto sampler = mAsset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex;
        if (img.has_value())
            resources.metallicRoughness.image = &mImages[img.value()];
        if (sampler.has_value())
            resources.metallicRoughness.sampler = *mSamplers[sampler.value()];
    }
}

void GLTFModel::assignEmissive(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material)
{
    constants.emissiveFactor = glm::vec4(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2], 0);

    resources.emissive = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
    if (material.emissiveTexture.has_value()) {
        auto img = mAsset.textures[material.emissiveTexture.value().textureIndex].imageIndex;
        auto sampler = mAsset.textures[material.emissiveTexture.value().textureIndex].samplerIndex;
        if (img.has_value())
            resources.emissive.image = &mImages[img.value()];
        if (sampler.has_value())
            resources.emissive.sampler = *mSamplers[sampler.value()];
    }
}

void GLTFModel::assignNormal(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material)
{
    resources.normal = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
    if (material.normalTexture.has_value()) {
        auto img = mAsset.textures[material.normalTexture.value().textureIndex].imageIndex;
        auto sampler = mAsset.textures[material.normalTexture.value().textureIndex].samplerIndex;
        if (img.has_value())
            resources.normal.image = &mImages[img.value()];
        if (sampler.has_value())
            resources.normal.sampler = *mSamplers[sampler.value()];

		constants.normalScale = material.normalTexture.value().scale;
    }
}

void GLTFModel::assignOcclusion(MaterialConstants& constants, MaterialResources& resources, fastgltf::Material& material)
{
    resources.occlusion = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
    if (material.occlusionTexture.has_value()) {
        auto img = mAsset.textures[material.occlusionTexture.value().textureIndex].imageIndex;
        auto sampler = mAsset.textures[material.occlusionTexture.value().textureIndex].samplerIndex;
        if (img.has_value())
            resources.occlusion.image = &mImages[img.value()];
        if (sampler.has_value())
            resources.occlusion.sampler = *mSamplers[sampler.value()];

		constants.occlusionStrength = material.occlusionTexture.value().strength;
    }
}

void GLTFModel::initBuffers()
{
	mMaterialConstantsBuffer = mRenderer->mRendererResources.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants),
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mMaterialConstantsBuffer.buffer, fmt::format("{}MaterialConstantsBuffer", mName).c_str());
	LOG_INFO(mRenderer->mLogger, "{} Material Constants Buffer Created", mName);

	mNodeTransformsBuffer = mRenderer->mRendererResources.createBuffer(MAX_NODES * sizeof(glm::mat4),
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mNodeTransformsBuffer.buffer, fmt::format("{}NodeTransformsBuffer", mName).c_str());
	LOG_INFO(mRenderer->mLogger, "{} Node Transforms Buffer Created", mName);
	
	mInstancesBuffer = mRenderer->mRendererResources.createBuffer(MAX_INSTANCES * sizeof(TransformData),
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mRendererCore.labelResourceDebug(mInstancesBuffer.buffer, fmt::format("{}InstancesBuffer", mName).c_str());
	LOG_INFO(mRenderer->mLogger, "{} Instances Buffer Created", mName);
}

void GLTFModel::loadSamplers()
{
	mSamplers.reserve(mAsset.samplers.size());
	for (fastgltf::Sampler& sampler : mAsset.samplers) {
		vk::SamplerCreateInfo sampl;
		sampl.pNext = nullptr;
		sampl.maxLod = vk::LodClampNone;
		sampl.minLod = 0;
		sampl.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		sampl.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
		sampl.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
		mSamplers.emplace_back(mRenderer->mRendererCore.mDevice.createSampler(sampl));
	}

	LOG_INFO(mRenderer->mLogger, "{} Samplers Loaded", mName);
}

void GLTFModel::loadImages()
{
	mImages.reserve(mAsset.images.size());
	int id = 0;
	for (fastgltf::Image& image : mAsset.images) {
		AllocatedImage newImage = loadImage(image);
		mRenderer->mRendererCore.labelResourceDebug(newImage.image, fmt::format("{}Image{}", mName, id).c_str());
		mRenderer->mRendererCore.labelResourceDebug(newImage.imageView, fmt::format("{}ImageView{}", mName, id).c_str());
		mImages.emplace_back(std::move(newImage));
		id++;
	}

	LOG_INFO(mRenderer->mLogger, "{} Images Loaded", mName);
}

void GLTFModel::loadMaterials()
{
	std::vector<MaterialConstants> materialConstants;
	materialConstants.reserve(mAsset.materials.size());

	mMaterials.reserve(mAsset.materials.size());
	int materialIndex = 0;
	for (fastgltf::Material& mat : mAsset.materials) {
		auto newMat = PbrMaterial(mRenderer);

		auto matName = std::string(mat.name);
		if (matName.empty()) {
			matName = fmt::format("{}", materialIndex);
		}
		newMat.mName = fmt::format("{}_mat{}", mName, matName);

		newMat.mPbrData.alphaMode = mat.alphaMode;
		newMat.mPbrData.doubleSided = mat.doubleSided;
		
		assignBase(newMat.mPbrData.constants, newMat.mPbrData.resources, mat);
        assignMetallicRoughness(newMat.mPbrData.constants, newMat.mPbrData.resources, mat);
        assignEmissive(newMat.mPbrData.constants, newMat.mPbrData.resources, mat);
        assignNormal(newMat.mPbrData.constants, newMat.mPbrData.resources, mat);
        assignOcclusion(newMat.mPbrData.constants, newMat.mPbrData.resources, mat);

		materialConstants.push_back(newMat.mPbrData.constants);

		newMat.mRelativeMaterialIndex = materialIndex;
		newMat.mConstantsBuffer = *mMaterialConstantsBuffer.buffer;
		newMat.mConstantsBufferOffset = materialIndex * sizeof(MaterialConstants);

		newMat.getMaterialPipeline();

		mMaterials.push_back(std::move(newMat));
		materialIndex++;
	}

	LOG_INFO(mRenderer->mLogger, "{} Materials Loaded", mName);

	loadMaterialsConstantsBuffer(materialConstants);
}

void GLTFModel::loadMeshes()
{
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh& mesh : mAsset.meshes) {
		Mesh newMesh;

		newMesh.mName = fmt::format("{}{}", mName, mesh.name);
		newMesh.mId = mRenderer->mRendererScene.mLatestMeshId;

		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives) {
			Primitive newPrimitive;
			newPrimitive.mRelativeFirstIndex = static_cast<uint32_t>(indices.size());
			newPrimitive.mRelativeVertexOffset = static_cast<uint32_t>(vertices.size());
			newPrimitive.mIndexCount = static_cast<uint32_t>(mAsset.accessors[p.indicesAccessor.value()].count);

			size_t vertexStartOffset = vertices.size();

			// Load indexes
			fastgltf::Accessor& indexAccessor = mAsset.accessors[p.indicesAccessor.value()];
			indices.reserve(indices.size() + indexAccessor.count);
			fastgltf::iterateAccessor<std::uint32_t>(mAsset, indexAccessor,
				[&](std::uint32_t index) {
					indices.push_back(index);
				});

			// Load vertex positions           
			fastgltf::Accessor& posAccessor = mAsset.accessors[p.findAttribute("POSITION")->second];
			vertices.resize(vertices.size() + posAccessor.count);
			fastgltf::iterateAccessorWithIndex<glm::vec3>(mAsset, posAccessor, // Default all the params
				[&](glm::vec3 v, size_t pos) {
					Vertex newVertex;
					newVertex.position = v;
					newVertex.normal = { 1, 0, 0 };
					newVertex.color = glm::vec4{ 1.f };
					newVertex.uv_x = 0;
					newVertex.uv_y = 0;
					vertices[vertexStartOffset + pos] = newVertex;
				});

			// Load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(mAsset, mAsset.accessors[normals->second],
					[&](glm::vec3 n, size_t pos) {
						vertices[vertexStartOffset + pos].normal = n;
					});
			}

			// Load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(mAsset, mAsset.accessors[uv->second],
					[&](glm::vec2 uv, size_t pos) {
						vertices[vertexStartOffset + pos].uv_x = uv.x;
						vertices[vertexStartOffset + pos].uv_y = uv.y;
					});
			}

			// Load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(mAsset, mAsset.accessors[(*colors).second],
					[&](glm::vec4 c, size_t pos) {
						vertices[vertexStartOffset + pos].color = c;
					});
			}

			if (p.materialIndex.has_value())
				newPrimitive.mMaterial = &mMaterials[p.materialIndex.value()];
			else
				newPrimitive.mMaterial = &mMaterials[0];

			newMesh.mPrimitives.push_back(newPrimitive);
		}

		loadMeshBuffers(&newMesh, indices, vertices);

		newMesh.mNumVertices = vertices.size();
		newMesh.mNumIndices = indices.size();

		newMesh.mBounds.min = glm::vec4(vertices[0].position, 0.f);
		newMesh.mBounds.max = glm::vec4(vertices[0].position, 0.f);
		for (auto& vertex: vertices) {
			newMesh.mBounds.min = glm::min(newMesh.mBounds.min, glm::vec4(vertex.position, 0.f));
			newMesh.mBounds.max = glm::max(newMesh.mBounds.max, glm::vec4(vertex.position, 0.f));
		}

		mMeshes.push_back(std::move(newMesh));
		mRenderer->mRendererScene.mLatestMeshId++;
	}

	LOG_INFO(mRenderer->mLogger, "{} Meshes Loaded", mName);
}

void GLTFModel::loadNodes()
{
	int nodeIndex = 0;
	for (fastgltf::Node& node : mAsset.nodes) {
		std::shared_ptr<Node> newNode;

		if (node.meshIndex.has_value()) {
			newNode = std::make_shared<MeshNode>();
			dynamic_cast<MeshNode*>(newNode.get())->mMesh = &mMeshes[*node.meshIndex];
		}
		else {
			newNode = std::make_shared<Node>();
		}
		newNode->mName = fmt::format("{}_node{}", mName, node.name);
		newNode->mRelativeNodeIndex = nodeIndex;

		// First function if it's a mat4 transform, second function if it's separate transform / rotate / scale quaternion or vec3
		std::visit(fastgltf::visitor{
			[&](const fastgltf::Node::TransformMatrix& matrix) {
				std::memcpy(&newNode->mLocalTransform, matrix.data(), sizeof(matrix));
			},
			[&](const fastgltf::Node::TRS& transform) {
				const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
				const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
				const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

				const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
				const glm::mat4 rm = glm::toMat4(rot);
				const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

				glm::mat4 localTransform = tm * rm * sm;
				newNode->mLocalTransform = localTransform;
			} },
			node.transform);

		mNodes.push_back(newNode);
		nodeIndex++;
	}
	LOG_INFO(mRenderer->mLogger, "{} Nodes Loaded", mName);

	// Setup hierarchy
	for (int i = 0; i < mAsset.nodes.size(); i++) {
		fastgltf::Node& assetNode = mAsset.nodes[i];
		std::shared_ptr<Node> localNode = mNodes[i];
		for (auto& nodeChildIndex : assetNode.children) {
			localNode->mChildren.push_back(mNodes[nodeChildIndex]);
			mNodes[nodeChildIndex]->mParent = localNode;
		}
	}
	LOG_INFO(mRenderer->mLogger, "{} Nodes Hierarchy Established", mName);

	// Find the top nodes, with no parents
	for (auto& node : mNodes) {
		if (node->mParent.lock() == nullptr) {
			mTopNodes.push_back(node);
			node->refreshTransform(glm::mat4{ 1.f });
		}
	}
	LOG_INFO(mRenderer->mLogger, "{} Materials Loaded", mName);

	loadNodeTransformsBuffer(mNodes);
}

void GLTFModel::loadMeshBuffers(Mesh* mesh, std::span<uint32_t> srcIndexVector, std::span<Vertex> srcVertexVector)
{
	const vk::DeviceSize srcVertexVectorSize = srcVertexVector.size() * sizeof(Vertex);
	const vk::DeviceSize srcIndexVectorSize = srcIndexVector.size() * sizeof(uint32_t);

	mesh->mVertexBuffer = mRenderer->mRendererResources.createBuffer(srcVertexVectorSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
	mesh->mIndexBuffer = mRenderer->mRendererResources.createBuffer(srcIndexVectorSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

	mRenderer->mRendererCore.labelResourceDebug(mesh->mVertexBuffer.buffer, fmt::format("{}VertexBuffer", mesh->mName).c_str());
	mRenderer->mRendererCore.labelResourceDebug(mesh->mIndexBuffer.buffer, fmt::format("{}IndexBuffer", mesh->mName).c_str());

	std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mMeshStagingBuffer.info.pMappedData) + 0, srcVertexVector.data(), srcVertexVectorSize);
	std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mMeshStagingBuffer.info.pMappedData) + srcVertexVectorSize, srcIndexVector.data(), srcIndexVectorSize);

	vk::BufferCopy vertexCopy{};
	vertexCopy.dstOffset = 0;
	vertexCopy.srcOffset = 0;
	vertexCopy.size = srcVertexVectorSize;
	vk::BufferCopy indexCopy{};
	indexCopy.dstOffset = 0;
	indexCopy.srcOffset = srcVertexVectorSize;
	indexCopy.size = srcIndexVectorSize;

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		cmd.copyBuffer(*mRenderer->mRendererResources.mMeshStagingBuffer.buffer, *mesh->mVertexBuffer.buffer, vertexCopy);
		cmd.copyBuffer(*mRenderer->mRendererResources.mMeshStagingBuffer.buffer, *mesh->mIndexBuffer.buffer, indexCopy);
		LOG_INFO(mRenderer->mLogger, "{} Mesh Buffers Uploading", mName);
	});
}

void GLTFModel::loadMaterialsConstantsBuffer(std::span<MaterialConstants> materialConstantsVector)
{
	std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mMaterialConstantsStagingBuffer.info.pMappedData), materialConstantsVector.data(), materialConstantsVector.size() * sizeof(MaterialConstants));

	vk::BufferCopy materialConstantsCopy{};
	materialConstantsCopy.dstOffset = 0;
	materialConstantsCopy.srcOffset = 0;
	materialConstantsCopy.size = materialConstantsVector.size() * sizeof(MaterialConstants);

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		cmd.copyBuffer(*mRenderer->mRendererResources.mMaterialConstantsStagingBuffer.buffer, *mMaterialConstantsBuffer.buffer, materialConstantsCopy);
		LOG_INFO(mRenderer->mLogger, "{} Material Constants Buffers Uploading", mName);
	});
}

void GLTFModel::loadNodeTransformsBuffer(std::span<std::shared_ptr<Node>> nodesVector)
{
	for (int i = 0; i < nodesVector.size(); i++) {
		std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mNodeTransformsStagingBuffer.info.pMappedData) + i * sizeof(glm::mat4), &nodesVector[i]->mWorldTransform, sizeof(glm::mat4));
	}

	vk::BufferCopy nodeTransformsCopy{};
	nodeTransformsCopy.dstOffset = 0;
	nodeTransformsCopy.srcOffset = 0;
	nodeTransformsCopy.size = nodesVector.size() * sizeof(glm::mat4);

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		cmd.copyBuffer(*mRenderer->mRendererResources.mNodeTransformsStagingBuffer.buffer, *mNodeTransformsBuffer.buffer, nodeTransformsCopy);
		LOG_INFO(mRenderer->mLogger, "{} Node Transforms Buffers Uploading", mName);
	});
}

void GLTFModel::loadInstancesBuffer(std::span<InstanceData> instanceDataVector)
{
	std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mInstancesStagingBuffer.info.pMappedData), instanceDataVector.data(), instanceDataVector.size() * sizeof(InstanceData));

	vk::BufferCopy instancesCopy{};
	instancesCopy.dstOffset = 0;
	instancesCopy.srcOffset = 0;
	instancesCopy.size = instanceDataVector.size() * sizeof(InstanceData);

	mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
		cmd.copyBuffer(*mRenderer->mRendererResources.mInstancesStagingBuffer.buffer, *mInstancesBuffer.buffer, instancesCopy);
		LOG_INFO(mRenderer->mLogger, "{} Instances Buffers Uploading", mName);
	});
}

void GLTFModel::createInstance(TransformData initialTransform)
{
	mInstances.emplace_back(this, mRenderer->mRendererScene.mLatestInstanceId, initialTransform);
	mRenderer->mRendererScene.mLatestInstanceId++;
	LOG_INFO(mRenderer->mLogger, "{} Instance Created", mName);
}

void GLTFModel::updateInstances()
{
	if (mInstances.size() == 0) { return; }

	std::vector<InstanceData> instanceDataVector;
	instanceDataVector.reserve(mInstances.size());
	for (auto& instance : mInstances) {
		const glm::mat4 tm = glm::translate(glm::mat4(1.f), instance.mTransformComponents.translation);
		const glm::mat4 rm = glm::yawPitchRoll(instance.mTransformComponents.rotation.x, instance.mTransformComponents.rotation.y, instance.mTransformComponents.rotation.z);
		const glm::mat4 sm = glm::scale(glm::mat4(1.f), glm::vec3(instance.mTransformComponents.scale));
		InstanceData instanceData{ tm * rm * sm };
		instanceDataVector.push_back(instanceData);
	}

	LOG_INFO(mRenderer->mLogger, "{} Instances Updated", mName);

	loadInstancesBuffer(instanceDataVector);
}

void GLTFModel::markDelete()
{
	mDeleteSignal = mRenderer->mRendererInfrastructure.mFrameNumber + FRAME_OVERLAP;
	LOG_INFO(mRenderer->mLogger, "{} Marked to Delete", mName);
}

void GLTFModel::load()
{
	initBuffers();
	loadSamplers();
	loadImages();
	loadMaterials();
	loadMeshes();
	loadNodes();

	LOG_INFO(mRenderer->mLogger, "{} Completely Loaded", mName);
}

void GLTFModel::generateRenderItems()
{
	if (!mDeleteSignal.has_value()) {
		for (auto& n : mTopNodes) {
			n->generateRenderItems(mRenderer, this);
		}
		LOG_INFO(mRenderer->mLogger, "{} Render Items Generated", mName);
	}
}

