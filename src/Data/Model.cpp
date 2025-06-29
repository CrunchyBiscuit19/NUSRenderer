#include <Renderer/Renderer.h>
#include <Data/Model.h>
#include <Utils/Helper.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtx/euler_angles.hpp>
#include <fmt/core.h>
#include <quill/LogMacros.h>
#include <fastgltf/glm_element_traits.hpp>

GLTFModel::GLTFModel(Renderer* renderer, const std::filesystem::path& modelPath) :
	mRenderer(renderer),
	mModelDescriptorAllocator(DescriptorAllocatorGrowable(renderer))
{
	mName = modelPath.stem().string();
	LOG_INFO(mRenderer->mLogger, "{} Open GLTF / GLB File", mName);

	mId = mRenderer->mScene.mLatestModelId++;

	fastgltf::Parser parser{};
	fastgltf::Asset gltf;
	fastgltf::GltfDataBuffer data;
	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
		fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
		fastgltf::Options::LoadExternalImages;

	data.loadFromFile(modelPath);

	auto type = fastgltf::determineGltfFileType(&data);
	if (type == fastgltf::GltfType::Invalid)
	{
		LOG_ERROR(mRenderer->mLogger, "{} Failed to determine GLTF Container", mName);
	}
	auto load = (type == fastgltf::GltfType::glTF)
		            ? (parser.loadGLTF(&data, modelPath.parent_path(), gltfOptions))
		            : (parser.loadBinaryGLTF(&data, modelPath.parent_path(), gltfOptions));
	if (load)
	{
		gltf = std::move(load.get());
	}
	else
	{
		LOG_ERROR(mRenderer->mLogger, "{} Failed to load GLTF Model: {}", mName, fastgltf::to_underlying(load.error()));
	}

	mAsset = std::move(gltf);

	this->load();

	createInstanceAtCamera(mRenderer->mCamera);
}

vk::Filter GLTFModel::extractFilter(fastgltf::Filter filter)
{
	switch (filter)
	{
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
	switch (filter)
	{
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return vk::SamplerMipmapMode::eNearest;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return vk::SamplerMipmapMode::eLinear;
	}
}

vk::SamplerAddressMode GLTFModel::extractAddressMode(fastgltf::Wrap wrap)
{
	switch (wrap)
	{
	case fastgltf::Wrap::Repeat:
		return vk::SamplerAddressMode::eRepeat;
	case fastgltf::Wrap::ClampToEdge:
		return vk::SamplerAddressMode::eClampToEdge;
	case fastgltf::Wrap::MirroredRepeat:
		return vk::SamplerAddressMode::eMirroredRepeat;
	default:
		return vk::SamplerAddressMode::eRepeat;
	}
}

AllocatedImage GLTFModel::loadImage(fastgltf::Image& image)
{
	AllocatedImage newImage;
	int width, height, nrChannels;

	std::visit(fastgltf::visitor{
		           // Image stored outside of GLTF / GLB file.
		           [&](const fastgltf::sources::URI& filePath)
		           {
			           assert(filePath.fileByteOffset == 0);
			           assert(filePath.uri.isLocalPath());
			           const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
			           if (unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4))
			           {
				           vk::Extent3D imagesize;
				           imagesize.width = width;
				           imagesize.height = height;
				           imagesize.depth = 1;
				           newImage = mRenderer->mResources.createImage(
					           data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
				           stbi_image_free(data);
			           }
		           },
		           // Image is loaded directly into a std::vector. If the texture is on base64, or if we instruct it to load external image files (fastgltf::Options::LoadExternalImages).
		           [&](const fastgltf::sources::Vector& vector)
		           {
			           if (unsigned char* data = stbi_load_from_memory(vector.bytes.data(),
			                                                           static_cast<int>(vector.bytes.size()),
			                                                           &width, &height, &nrChannels, 4))
			           {
				           vk::Extent3D imagesize;
				           imagesize.width = width;
				           imagesize.height = height;
				           imagesize.depth = 1;
				           newImage = mRenderer->mResources.createImage(
					           data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
				           stbi_image_free(data);
			           }
		           },
		           // Image embedded into the binary GLB file.
		           [&](const fastgltf::sources::BufferView& view)
		           {
			           const auto& bufferView = mAsset.bufferViews[view.bufferViewIndex];
			           auto& buffer = mAsset.buffers[bufferView.bufferIndex];
			           std::visit(fastgltf::visitor{
				                      [&](const fastgltf::sources::Vector& vector)
				                      {
					                      if (unsigned char* data = stbi_load_from_memory(
						                      vector.bytes.data() + bufferView.byteOffset,
						                      static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4))
					                      {
						                      vk::Extent3D imagesize;
						                      imagesize.width = width;
						                      imagesize.height = height;
						                      imagesize.depth = 1;
						                      newImage = mRenderer->mResources.createImage(
							                      data, imagesize, vk::Format::eR8G8B8A8Unorm,
							                      vk::ImageUsageFlagBits::eSampled, true);
						                      stbi_image_free(data);
					                      }
				                      },
				                      [](const auto& arg)
				                      {
				                      },
			                      },
			                      buffer.data);
		           },
		           [](const auto& arg)
		           {
		           },
	           }, image.data);
	// Move the lambda taking const auto to the bottom. Otherwise it always get runs and the other lambdas don't.
	// Needs to be exactly const auto&?
	// No idea why it's even needed in the first place.

	return newImage;
}

void GLTFModel::assignBase(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material)
{
	constants.baseFactor = glm::vec4(material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1],
	                                 material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3]);

	resources.base = {
		&mRenderer->mResources.mDefaultImages.at(DefaultImage::White),
		mRenderer->mResources.getSampler(vk::SamplerCreateInfo())
	};
	if (material.pbrData.baseColorTexture.has_value())
	{
		auto imageIndex = mAsset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex;
		auto samplerIndex = mAsset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex;
		if (imageIndex.has_value())
			resources.base.image = &mImages[imageIndex.value()];
		if (samplerIndex.has_value())
			resources.base.sampler = mRenderer->mResources.
			                                    getSampler(mSamplerCreateInfos[samplerIndex.value()]);
	}
}

void GLTFModel::assignMetallicRoughness(MaterialConstants& constants, MaterialResources& resources,
                                        const fastgltf::Material& material)
{
	constants.metallicRoughnessFactor = glm::vec2(material.pbrData.metallicFactor, material.pbrData.roughnessFactor);

	resources.metallicRoughness = {
		&mRenderer->mResources.mDefaultImages.at(DefaultImage::White),
		mRenderer->mResources.getSampler(vk::SamplerCreateInfo())
	};
	if (material.pbrData.metallicRoughnessTexture.has_value())
	{
		auto imageIndex = mAsset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex;
		auto samplerIndex = mAsset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].
			samplerIndex;
		if (imageIndex.has_value())
			resources.metallicRoughness.image = &mImages[imageIndex.value()];
		if (samplerIndex.has_value())
			resources.metallicRoughness.sampler = mRenderer->mResources.getSampler(
				mSamplerCreateInfos[samplerIndex.value()]);
	}
}

void GLTFModel::assignEmissive(MaterialConstants& constants, MaterialResources& resources, const fastgltf::Material& material)
{
	constants.emissiveFactor = glm::vec4(material.emissiveFactor[0], material.emissiveFactor[1],
	                                     material.emissiveFactor[2], 0);

	resources.emissive = {
		&mRenderer->mResources.mDefaultImages.at(DefaultImage::White),
		mRenderer->mResources.getSampler(vk::SamplerCreateInfo())
	};
	if (material.emissiveTexture.has_value())
	{
		auto imageIndex = mAsset.textures[material.emissiveTexture.value().textureIndex].imageIndex;
		auto samplerIndex = mAsset.textures[material.emissiveTexture.value().textureIndex].samplerIndex;
		if (imageIndex.has_value())
			resources.emissive.image = &mImages[imageIndex.value()];
		if (samplerIndex.has_value())
			resources.emissive.sampler = mRenderer->mResources.getSampler(
				mSamplerCreateInfos[samplerIndex.value()]);
	}
}

auto GLTFModel::assignNormal(MaterialConstants& constants, MaterialResources& resources,
                             const fastgltf::Material& material) -> void
{
	resources.normal = {
		&mRenderer->mResources.mDefaultImages.at(DefaultImage::White),
		mRenderer->mResources.getSampler(vk::SamplerCreateInfo())
	};
	if (material.normalTexture.has_value())
	{
		auto imageIndex = mAsset.textures[material.normalTexture.value().textureIndex].imageIndex;
		auto samplerIndex = mAsset.textures[material.normalTexture.value().textureIndex].samplerIndex;
		if (imageIndex.has_value())
			resources.normal.image = &mImages[imageIndex.value()];
		if (samplerIndex.has_value())
			resources.normal.sampler = mRenderer->mResources.getSampler(
				mSamplerCreateInfos[samplerIndex.value()]);

		constants.normalScale = material.normalTexture.value().scale;
	}
}

void GLTFModel::assignOcclusion(MaterialConstants& constants, MaterialResources& resources,
                                const fastgltf::Material& material)
{
	resources.occlusion = {
		&mRenderer->mResources.mDefaultImages.at(DefaultImage::White),
		mRenderer->mResources.getSampler(vk::SamplerCreateInfo())
	};
	if (material.occlusionTexture.has_value())
	{
		auto imageIndex = mAsset.textures[material.occlusionTexture.value().textureIndex].imageIndex;
		auto samplerIndex = mAsset.textures[material.occlusionTexture.value().textureIndex].samplerIndex;
		if (imageIndex.has_value())
			resources.occlusion.image = &mImages[imageIndex.value()];
		if (samplerIndex.has_value())
			resources.occlusion.sampler = mRenderer->mResources.getSampler(
				mSamplerCreateInfos[samplerIndex.value()]);

		constants.occlusionStrength = material.occlusionTexture.value().strength;
	}
}

void GLTFModel::initBuffers()
{
	mMaterialConstantsBuffer = mRenderer->mResources.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants),
	                                                                      vk::BufferUsageFlagBits::eTransferSrc |
	                                                                      vk::BufferUsageFlagBits::eTransferDst |
	                                                                      vk::BufferUsageFlagBits::eStorageBuffer,
	                                                                      VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mCore.labelResourceDebug(mMaterialConstantsBuffer.buffer,
	                                            fmt::format("{}MaterialConstantsBuffer", mName).c_str());
	LOG_INFO(mRenderer->mLogger, "{} Material Constants Buffer Created", mName);

	mNodeTransformsBuffer = mRenderer->mResources.createBuffer(MAX_NODES * sizeof(glm::mat4),
	                                                                   vk::BufferUsageFlagBits::eTransferSrc |
	                                                                   vk::BufferUsageFlagBits::eTransferDst |
	                                                                   vk::BufferUsageFlagBits::eStorageBuffer,
	                                                                   VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mCore.labelResourceDebug(mNodeTransformsBuffer.buffer,
	                                            fmt::format("{}NodeTransformsBuffer", mName).c_str());
	LOG_INFO(mRenderer->mLogger, "{} Node Transforms Buffer Created", mName);

	mInstancesBuffer = mRenderer->mResources.createBuffer(MAX_INSTANCES * sizeof(InstanceData),
	                                                              vk::BufferUsageFlagBits::eTransferSrc |
	                                                              vk::BufferUsageFlagBits::eTransferDst |
	                                                              vk::BufferUsageFlagBits::eStorageBuffer,
	                                                              VMA_MEMORY_USAGE_CPU_TO_GPU);
	mRenderer->mCore.labelResourceDebug(mInstancesBuffer.buffer,
	                                            fmt::format("{}InstancesBuffer", mName).c_str());
	LOG_INFO(mRenderer->mLogger, "{} Instances Buffer Created", mName);
}

void GLTFModel::loadSamplerCreateInfos()
{
	mSamplerCreateInfos.reserve(mAsset.samplers.size());
	for (fastgltf::Sampler& sampler : mAsset.samplers)
	{
		vk::SamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.pNext = nullptr;
		samplerCreateInfo.maxLod = vk::LodClampNone;
		samplerCreateInfo.minLod = 0;
		samplerCreateInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		samplerCreateInfo.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
		samplerCreateInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
		samplerCreateInfo.addressModeU = extractAddressMode(sampler.wrapS);
		samplerCreateInfo.addressModeV = extractAddressMode(sampler.wrapT);
		samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
		mSamplerCreateInfos.push_back(samplerCreateInfo);
	}

	LOG_INFO(mRenderer->mLogger, "{} Sampler Options Loaded", mName);
}

void GLTFModel::loadImages()
{
	mImages.reserve(mAsset.images.size());
	int id = 0;
	for (fastgltf::Image& image : mAsset.images)
	{
		AllocatedImage newImage = loadImage(image);
		mRenderer->mCore.labelResourceDebug(newImage.image, fmt::format("{}Image{}", mName, id).c_str());
		mRenderer->mCore.
		           labelResourceDebug(newImage.imageView, fmt::format("{}ImageView{}", mName, id).c_str());
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
	for (fastgltf::Material& mat : mAsset.materials)
	{
		auto newMat = PbrMaterial(mRenderer);

		auto matName = std::string(mat.name);
		if (matName.empty())
		{
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

	mMeshes.reserve(mAsset.meshes.size());
	for (fastgltf::Mesh& mesh : mAsset.meshes)
	{
		Mesh newMesh;

		newMesh.mName = fmt::format("{}{}", mName, mesh.name);
		newMesh.mId = mRenderer->mScene.mLatestMeshId;

		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives)
		{
			Primitive newPrimitive;
			newPrimitive.mRelativeFirstIndex = static_cast<uint32_t>(indices.size());
			newPrimitive.mRelativeVertexOffset = static_cast<uint32_t>(vertices.size());
			newPrimitive.mIndexCount = static_cast<uint32_t>(mAsset.accessors[p.indicesAccessor.value()].count);

			size_t vertexStartOffset = vertices.size();

			// Load indexes
			fastgltf::Accessor& indexAccessor = mAsset.accessors[p.indicesAccessor.value()];
			indices.reserve(indices.size() + indexAccessor.count);
			fastgltf::iterateAccessor<std::uint32_t>(mAsset, indexAccessor,
			                                         [&](std::uint32_t index)
			                                         {
				                                         indices.push_back(index);
			                                         });

			// Load vertex positions           
			fastgltf::Accessor& posAccessor = mAsset.accessors[p.findAttribute("POSITION")->second];
			vertices.resize(vertices.size() + posAccessor.count);
			fastgltf::iterateAccessorWithIndex<glm::vec3>(mAsset, posAccessor, // Default all the params
			                                              [&](glm::vec3 v, size_t pos)
			                                              {
				                                              Vertex newVertex;
				                                              newVertex.position = v;
				                                              newVertex.normal = {1, 0, 0};
				                                              newVertex.color = glm::vec4{1.f};
				                                              newVertex.uv_x = 0;
				                                              newVertex.uv_y = 0;
				                                              vertices[vertexStartOffset + pos] = newVertex;
			                                              });

			// Load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(mAsset, mAsset.accessors[normals->second],
				                                              [&](glm::vec3 n, size_t pos)
				                                              {
					                                              vertices[vertexStartOffset + pos].normal = n;
				                                              });
			}

			// Load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(mAsset, mAsset.accessors[uv->second],
				                                              [&](glm::vec2 uv, size_t pos)
				                                              {
					                                              vertices[vertexStartOffset + pos].uv_x = uv.x;
					                                              vertices[vertexStartOffset + pos].uv_y = uv.y;
				                                              });
			}

			// Load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(mAsset, mAsset.accessors[colors->second],
				                                              [&](glm::vec4 c, size_t pos)
				                                              {
					                                              vertices[vertexStartOffset + pos].color = c;
				                                              });
			}

			if (p.materialIndex.has_value())
				newPrimitive.mMaterial = &mMaterials[p.materialIndex.value()];
			else
				newPrimitive.mMaterial = &mMaterials[0];

			newMesh.mPrimitives.push_back(newPrimitive);
		}

		newMesh.mNumVertices = vertices.size();
		newMesh.mNumIndices = indices.size();

		newMesh.mBounds.min = glm::vec4(vertices[0].position, 0.f);
		newMesh.mBounds.max = glm::vec4(vertices[0].position, 0.f);
		for (auto& vertex : vertices)
		{
			newMesh.mBounds.min = glm::min(newMesh.mBounds.min, glm::vec4(vertex.position, 0.f));
			newMesh.mBounds.max = glm::max(newMesh.mBounds.max, glm::vec4(vertex.position, 0.f));
		}

		mMeshes.push_back(std::move(newMesh));

		loadMeshBuffers(mMeshes.back(), indices, vertices);

		mRenderer->mScene.mLatestMeshId++;
	}

	LOG_INFO(mRenderer->mLogger, "{} Meshes Loaded", mName);
}

void GLTFModel::loadNodes()
{
	int nodeIndex = 0;
	mNodes.reserve(mAsset.nodes.size());
	for (fastgltf::Node& node : mAsset.nodes)
	{
		std::shared_ptr<Node> newNode;

		if (node.meshIndex.has_value())
		{
			newNode = std::make_shared<MeshNode>();
			dynamic_cast<MeshNode*>(newNode.get())->mMesh = &mMeshes[*node.meshIndex];
		}
		else
		{
			newNode = std::make_shared<Node>();
		}
		newNode->mName = fmt::format("{}_node{}", mName, node.name);
		newNode->mRelativeNodeIndex = nodeIndex;

		// First function if it's a mat4 transform, second function if it's separate transform / rotate / scale quaternion or vec3
		std::visit(fastgltf::visitor{
			           [&](const fastgltf::Node::TransformMatrix& matrix)
			           {
				           std::memcpy(&newNode->mLocalTransform, matrix.data(), sizeof(matrix));
			           },
			           [&](const fastgltf::Node::TRS& transform)
			           {
				           const glm::vec3 tl(transform.translation[0], transform.translation[1],
				                              transform.translation[2]);
				           const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
				                               transform.rotation[2]);
				           const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

				           const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
				           const glm::mat4 rm = glm::toMat4(rot);
				           const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

				           glm::mat4 localTransform = tm * rm * sm;
				           newNode->mLocalTransform = localTransform;
			           }
		           },
		           node.transform);

		mNodes.push_back(newNode);
		nodeIndex++;
	}
	LOG_INFO(mRenderer->mLogger, "{} Nodes Loaded", mName);

	// Setup hierarchy
	for (int i = 0; i < mAsset.nodes.size(); i++)
	{
		fastgltf::Node& assetNode = mAsset.nodes[i];
		std::shared_ptr<Node> localNode = mNodes[i];
		for (auto& nodeChildIndex : assetNode.children)
		{
			localNode->mChildren.push_back(mNodes[nodeChildIndex]);
			mNodes[nodeChildIndex]->mParent = localNode;
		}
	}
	LOG_INFO(mRenderer->mLogger, "{} Nodes Hierarchy Established", mName);

	// Find the top nodes, with no parents
	for (auto& node : mNodes)
	{
		if (node->mParent.lock() == nullptr)
		{
			mTopNodes.push_back(node);
			node->refreshTransform(glm::mat4{1.f});
		}
	}
	LOG_INFO(mRenderer->mLogger, "{} Materials Loaded", mName);

	loadNodeTransformsBuffer(mNodes);
}

void GLTFModel::loadMeshBuffers(Mesh& mesh, std::span<uint32_t> srcIndexVector, std::span<Vertex> srcVertexVector)
{
	const vk::DeviceSize srcVertexVectorSize = srcVertexVector.size() * sizeof(Vertex);
	const vk::DeviceSize srcIndexVectorSize = srcIndexVector.size() * sizeof(uint32_t);

	mesh.mVertexBuffer = mRenderer->mResources.createBuffer(srcVertexVectorSize,
	                                                                vk::BufferUsageFlagBits::eTransferSrc |
	                                                                vk::BufferUsageFlagBits::eTransferDst |
	                                                                vk::BufferUsageFlagBits::eStorageBuffer,
																	VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mCore.labelResourceDebug(mesh.mVertexBuffer.buffer,
	                                            fmt::format("{}VertexBuffer", mesh.mName).c_str());
	mesh.mIndexBuffer = mRenderer->mResources.createBuffer(srcIndexVectorSize,
	                                                               vk::BufferUsageFlagBits::eTransferSrc |
	                                                               vk::BufferUsageFlagBits::eTransferDst |
	                                                               vk::BufferUsageFlagBits::eIndexBuffer,
	                                                               VMA_MEMORY_USAGE_GPU_ONLY);
	mRenderer->mCore.labelResourceDebug(mesh.mIndexBuffer.buffer,
	                                            fmt::format("{}IndexBuffer", mesh.mName).c_str());

	std::memcpy(static_cast<char*>(mRenderer->mResources.mMeshStagingBuffer.info.pMappedData) + 0,
	            srcVertexVector.data(), srcVertexVectorSize);
	std::memcpy(
		static_cast<char*>(mRenderer->mResources.mMeshStagingBuffer.info.pMappedData) + srcVertexVectorSize,
		srcIndexVector.data(), srcIndexVectorSize);

	vk::BufferCopy vertexCopy{};
	vertexCopy.dstOffset = 0;
	vertexCopy.srcOffset = 0;
	vertexCopy.size = srcVertexVectorSize;
	vk::BufferCopy indexCopy{};
	indexCopy.dstOffset = 0;
	indexCopy.srcOffset = srcVertexVectorSize;
	indexCopy.size = srcIndexVectorSize;

	mRenderer->mImmSubmit.individualSubmit([&mesh, vertexCopy, indexCopy](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier(
			cmd,
			*renderer->mResources.mMeshStagingBuffer.buffer,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);

		cmd.copyBuffer(*renderer->mResources.mMeshStagingBuffer.buffer, *mesh.mVertexBuffer.buffer, vertexCopy);
		cmd.copyBuffer(*renderer->mResources.mMeshStagingBuffer.buffer, *mesh.mIndexBuffer.buffer, indexCopy);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mesh.mVertexBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mesh.mIndexBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);
	});
	LOG_INFO(mRenderer->mLogger, "{} Model Buffer {} Uploading", mName, mesh.mId);
}

void GLTFModel::loadMaterialsConstantsBuffer(std::span<MaterialConstants> materialConstantsVector)
{
	std::memcpy(mRenderer->mResources.mMaterialConstantsStagingBuffer.info.pMappedData,
	            materialConstantsVector.data(), materialConstantsVector.size() * sizeof(MaterialConstants));

	vk::BufferCopy materialConstantsCopy{};
	materialConstantsCopy.dstOffset = 0;
	materialConstantsCopy.srcOffset = 0;
	materialConstantsCopy.size = materialConstantsVector.size() * sizeof(MaterialConstants);

	mRenderer->mImmSubmit.individualSubmit([this, materialConstantsCopy](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier(
			cmd,
			*renderer->mResources.mMaterialConstantsStagingBuffer.buffer,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);

		cmd.copyBuffer(*renderer->mResources.mMaterialConstantsStagingBuffer.buffer,
		               *mMaterialConstantsBuffer.buffer, materialConstantsCopy);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mMaterialConstantsBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);
	});
	LOG_INFO(mRenderer->mLogger, "{} Material Constants Buffers Uploading", mName);
}

void GLTFModel::loadNodeTransformsBuffer(std::span<std::shared_ptr<Node>> nodesVector)
{
	for (int i = 0; i < nodesVector.size(); i++)
	{
		std::memcpy(
			static_cast<char*>(mRenderer->mResources.mNodeTransformsStagingBuffer.info.pMappedData) + i * sizeof
			(glm::mat4), &nodesVector[i]->mWorldTransform, sizeof(glm::mat4));
	}

	vk::BufferCopy nodeTransformsCopy{};
	nodeTransformsCopy.dstOffset = 0;
	nodeTransformsCopy.srcOffset = 0;
	nodeTransformsCopy.size = nodesVector.size() * sizeof(glm::mat4);

	mRenderer->mImmSubmit.individualSubmit([this, nodeTransformsCopy](Renderer* renderer, vk::CommandBuffer cmd)
	{
		vkhelper::createBufferPipelineBarrier(
			cmd,
			*renderer->mResources.mNodeTransformsStagingBuffer.buffer,
			vk::PipelineStageFlagBits2::eHost,
			vk::AccessFlagBits2::eHostWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);

		cmd.copyBuffer(*renderer->mResources.mNodeTransformsStagingBuffer.buffer, *mNodeTransformsBuffer.buffer,
		               nodeTransformsCopy);

		vkhelper::createBufferPipelineBarrier(
			cmd,
			*mNodeTransformsBuffer.buffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferRead
		);
	});
	LOG_INFO(mRenderer->mLogger, "{} Node Transforms Buffers Uploading", mName);
}

void GLTFModel::createInstance(InstanceData initialData)
{
	mInstances.emplace_back(this, mRenderer->mScene.mLatestInstanceId++, initialData);
	mReloadInstances = true;
	mRenderer->mScene.mFlags.instanceAddedFlag = true;
	LOG_INFO(mRenderer->mLogger, "{} Instance Created", mName);
}

void GLTFModel::createInstanceAtCamera(Camera& camera)
{
	glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), mRenderer->mCamera.mPosition + mRenderer->mCamera.getDirectionVector());
	glm::mat4 rotationMatrix = glm::mat4(1.f);
	glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(1.f));
	createInstance(transformMatrix * rotationMatrix * scaleMatrix);
}

void GLTFModel::reloadInstances()
{
	if (mInstances.empty()) {
		mReloadInstances = false;
		return;
	}

	int dstOffset = 0;
	for (auto& instance : mInstances) {
		std::memcpy(static_cast<char*>(mInstancesBuffer.info.pMappedData) + dstOffset, &instance.mData, sizeof(InstanceData));
		dstOffset += sizeof(InstanceData);
	}

	LOG_INFO(mRenderer->mLogger, "{} Instances Updated", mName);
	LOG_INFO(mRenderer->mLogger, "{} Instances Buffer Uploading", mName);

	mReloadInstances = false;
}

void GLTFModel::markDelete()
{
	mDeleteSignal = mRenderer->mInfrastructure.mFrameNumber + FRAME_OVERLAP;
	for (auto& instance: mInstances) {
		instance.markDelete();
	}
	LOG_INFO(mRenderer->mLogger, "{} Marked to Delete", mName);
}

void GLTFModel::load()
{
	initBuffers();
	loadSamplerCreateInfos();
	loadImages();
	loadMaterials();
	loadMeshes();
	loadNodes();

	LOG_INFO(mRenderer->mLogger, "{} Completely Loaded", mName);
}

Renderer* GLTFModel::getRenderer() const
{
	return mRenderer;
}

void GLTFModel::generateRenderItems()
{
	if (!mDeleteSignal.has_value())
	{
		for (auto& n : mTopNodes)
		{
			n->generateRenderItems(mRenderer, this);
		}
		LOG_INFO(mRenderer->mLogger, "{} Render Items Generated", mName);
	}
}
