#include <Renderer.h>
#include <Model.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtx/euler_angles.hpp>
#include <fmt/core.h>

GLTFModel::GLTFModel(Renderer* renderer, std::filesystem::path modelPath):
	mRenderer(renderer),
    mDescriptorAllocator(DescriptorAllocatorGrowable(renderer))
{
    mName = modelPath.stem().string();
    fmt::println("{} Model [Open File]", mName);

    fastgltf::Parser parser{};
    fastgltf::Asset gltf;
    fastgltf::GltfDataBuffer data;
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    data.loadFromFile(modelPath);

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::Invalid) { fmt::println("Failed to determine GLTF container for {}", mName); }
    auto load = (type == fastgltf::GltfType::glTF) ? (parser.loadGLTF(&data, modelPath.parent_path(), gltfOptions)) : (parser.loadBinaryGLTF(&data, modelPath.parent_path(), gltfOptions));
    if (load) { gltf = std::move(load.get()); } else { fmt::println("Failed to load GLTF Model for {}: {}", mName, fastgltf::to_underlying(load.error())); }

    mAsset = std::move(gltf);

    this->load();

    createInstance();
}

GLTFModel::~GLTFModel()
{
    // Jank solution to cleanup per-material resource description sets.
    // Somehow with it being a shared_ptr it does not get cleaned up when the model is deleted.
    for (auto& material : mMaterials) {
        material->mResourcesDescriptorSet.clear();
    }

    fmt::println("{} Model [Deletion]", mName);
}

GLTFModel::GLTFModel(GLTFModel&& other) noexcept: 
    mRenderer(other.mRenderer),
    mName(std::move(other.mName)),
    mLatestId(other.mLatestId),
    mDeleteSignal(std::move(other.mDeleteSignal)),
    mAsset(std::move(other.mAsset)),
    mTopNodes(std::move(other.mTopNodes)),
    mNodes(std::move(other.mNodes)),
    mMeshes(std::move(other.mMeshes)),
    mSamplers(std::move(other.mSamplers)),
    mImages(std::move(other.mImages)),
    mDescriptorAllocator(std::move(other.mDescriptorAllocator)),
    mMaterials(std::move(other.mMaterials)),
    mMaterialConstantsBuffer(std::move(other.mMaterialConstantsBuffer)),
    mInstances(std::move(other.mInstances)),
    mInstancesBuffer(std::move(other.mInstancesBuffer))
{
    other.mRenderer = nullptr;
    other.mLatestId = 0;
}

GLTFModel& GLTFModel::operator=(GLTFModel&& other) noexcept
{
    if (this != &other)
    {
        mRenderer = other.mRenderer;
        mName = std::move(other.mName);
        mLatestId = other.mLatestId;
        mDeleteSignal = std::move(other.mDeleteSignal),
        mAsset = std::move(other.mAsset);
        mTopNodes = std::move(other.mTopNodes);
        mNodes = std::move(other.mNodes);
        mMeshes = std::move(other.mMeshes);
        mSamplers = std::move(other.mSamplers);
        mImages = std::move(other.mImages);
        mDescriptorAllocator = std::move(other.mDescriptorAllocator);
        mMaterials = std::move(other.mMaterials);
        mMaterialConstantsBuffer = std::move(other.mMaterialConstantsBuffer);
        mInstances = std::move(other.mInstances);
        mInstancesBuffer = std::move(other.mInstancesBuffer);

        other.mRenderer = nullptr;
        other.mLatestId = 0;
    }
    return *this;
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
                newImage = mRenderer->mRendererResources.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true, false, false);
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
                newImage = mRenderer->mRendererResources.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true, false, false);
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
                            newImage = mRenderer->mRendererResources.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true, false, false);
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

void GLTFModel::initDescriptors()
{
    std::vector<DescriptorAllocatorGrowable::DescriptorTypeRatio> sizes = { 
        { vk::DescriptorType::eCombinedImageSampler, 5 },
    };
    mDescriptorAllocator.init(mAsset.materials.size(), sizes);
}

void GLTFModel::initBuffers()
{
    fmt::println("{} Model [Create Buffers]", mName);

    mMaterialConstantsBuffer = mRenderer->mRendererResources.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mInstancesBuffer = mRenderer->mRendererResources.createBuffer(MAX_INSTANCES * sizeof(TransformData),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_MEMORY_USAGE_GPU_ONLY);
}

void GLTFModel::loadSamplers()
{
    fmt::println("{} Model [Load Samplers]", mName);

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
}

void GLTFModel::loadImages()
{
    mImages.reserve(mAsset.images.size());
    for (fastgltf::Image& image : mAsset.images) {
        mImages.emplace_back(loadImage(image));
    }
}

void GLTFModel::loadMaterials()
{
    fmt::println("{} Model [Load Materials]", mName);

    std::vector<MaterialConstants> materialConstants;
    materialConstants.reserve(mAsset.materials.size());

    mMaterials.reserve(mAsset.materials.size());
    int materialIndex = 0;
    for (fastgltf::Material& mat : mAsset.materials) {
        auto newMat = std::make_shared<PbrMaterial>(mRenderer, &mDescriptorAllocator);
        auto matName = std::string(mat.name);
        if (matName.empty()) {
            matName = fmt::format("{}", materialIndex);
        }
        newMat->mName = fmt::format("{}_mat{}", mName, matName);

        newMat->mPbrData.constants.baseFactor = glm::vec4(mat.pbrData.baseColorFactor[0], mat.pbrData.baseColorFactor[1], mat.pbrData.baseColorFactor[2], mat.pbrData.baseColorFactor[3]);
        newMat->mPbrData.constants.emissiveFactor = glm::vec4(mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2], 0);
        newMat->mPbrData.constants.metallicRoughnessFactor.x = mat.pbrData.metallicFactor;
        newMat->mPbrData.constants.metallicRoughnessFactor.y = mat.pbrData.roughnessFactor;

        materialConstants.push_back(newMat->mPbrData.constants);

        newMat->mPbrData.alphaMode = mat.alphaMode;
        newMat->mPbrData.doubleSided = mat.doubleSided;

        newMat->mPbrData.resources.base = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
        newMat->mPbrData.resources.metallicRoughness = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
        newMat->mPbrData.resources.normal = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };
        newMat->mPbrData.resources.occlusion = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], * mRenderer->mRendererResources.mDefaultSampler };
        newMat->mPbrData.resources.emissive = { &mRenderer->mRendererResources.mDefaultImages[DefaultImage::White], *mRenderer->mRendererResources.mDefaultSampler };

        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = mAsset.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = mAsset.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
            newMat->mPbrData.resources.base = { &mImages[img], *mSamplers[sampler] };
        }
        if (mat.pbrData.metallicRoughnessTexture.has_value()) {
            size_t img = mAsset.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = mAsset.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();
            newMat->mPbrData.resources.metallicRoughness = { &mImages[img], *mSamplers[sampler] };
        }
        if (mat.normalTexture.has_value()) {
            size_t img = mAsset.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            size_t sampler = mAsset.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();
            newMat->mPbrData.resources.normal = { &mImages[img], *mSamplers[sampler] };
        }
        if (mat.occlusionTexture.has_value()) {
            size_t img = mAsset.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
            size_t sampler = mAsset.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();
            newMat->mPbrData.resources.occlusion = { &mImages[img], *mSamplers[sampler] };
        }
        if (mat.emissiveTexture.has_value()) {
            size_t img = mAsset.textures[mat.emissiveTexture.value().textureIndex].imageIndex.value();
            size_t sampler = mAsset.textures[mat.emissiveTexture.value().textureIndex].samplerIndex.value();
            newMat->mPbrData.resources.emissive = { &mImages[img], *mSamplers[sampler] };
        }

        newMat->mMaterialIndex = materialIndex;
        newMat->mConstantsBuffer = *mMaterialConstantsBuffer.buffer;
        newMat->mConstantsBufferOffset = materialIndex * sizeof(MaterialConstants);
        newMat->writeMaterialResources();

        mMaterials.push_back(newMat);
        materialIndex++;
    }

    loadMaterialsConstantsBuffer(materialConstants);
}

void GLTFModel::loadMeshes()
{
    fmt::println("{} Model [Load Meshes]", mName);

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : mAsset.meshes) {
        std::shared_ptr<Mesh> newMesh = std::make_shared<Mesh>();
        newMesh->mName = fmt::format("{}_mesh{}", mName, mesh.name);

        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            Primitive newPrimitive;
            newPrimitive.indexStart = static_cast<uint32_t>(indices.size());
            newPrimitive.indexCount = static_cast<uint32_t>(mAsset.accessors[p.indicesAccessor.value()].count);

            size_t initialVerticesSize = vertices.size();
            
            // Load indexes
            fastgltf::Accessor& indexAccessor = mAsset.accessors[p.indicesAccessor.value()];
            indices.reserve(indices.size() + indexAccessor.count);
            fastgltf::iterateAccessor<std::uint32_t>(mAsset, indexAccessor,
                [&](std::uint32_t index) {
                    indices.push_back(initialVerticesSize + index); // Add the vertices vector size so indices would reference vertices of current primitive instead of first set of vertices added
                });
            

            // Load vertex positions           
            fastgltf::Accessor& posAccessor = mAsset.accessors[p.findAttribute("POSITION")->second];
            vertices.resize(vertices.size() + posAccessor.count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(mAsset, posAccessor, // Default all the params
                [&](glm::vec3 v, size_t index) {
                    Vertex newvtx;
                    newvtx.position = v;
                    newvtx.normal = { 1, 0, 0 };
                    newvtx.color = glm::vec4{ 1.f };
                    newvtx.uv_x = 0;
                    newvtx.uv_y = 0;
                    vertices[initialVerticesSize + index] = newvtx;
                });

            // Load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(mAsset, mAsset.accessors[normals->second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initialVerticesSize + index].normal = v;
                    });
            }

            // Load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(mAsset, mAsset.accessors[uv->second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initialVerticesSize + index].uv_x = v.x;
                        vertices[initialVerticesSize + index].uv_y = v.y;
                    });
            }

            // Load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(mAsset, mAsset.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initialVerticesSize + index].color = v;
                    });
            }

            if (p.materialIndex.has_value())
                newPrimitive.material = mMaterials[p.materialIndex.value()];
            else
                newPrimitive.material = mMaterials[0];

            // Find min/max bounds
            glm::vec3 minpos = vertices[initialVerticesSize].position;
            glm::vec3 maxpos = vertices[initialVerticesSize].position;
            for (int i = initialVerticesSize; i < vertices.size(); i++) {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }
            newPrimitive.bounds.origin = (maxpos + minpos) / 2.f;
            newPrimitive.bounds.extents = (maxpos - minpos) / 2.f;
            newPrimitive.bounds.sphereRadius = glm::length(newPrimitive.bounds.extents);

            newMesh->mPrimitives.push_back(newPrimitive);
        }

        loadMeshBuffers(newMesh.get(), indices, vertices);

        mMeshes.push_back(newMesh);
    }
}

void GLTFModel::loadNodes()
{
    fmt::println("{} Model [Load Nodes]", mName);

    for (fastgltf::Node& node : mAsset.nodes) {
        std::shared_ptr<Node> newNode;
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            dynamic_cast<MeshNode*>(newNode.get())->mMesh = mMeshes[*node.meshIndex];
        }
        else {
            newNode = std::make_shared<Node>();
        }
        newNode->mName = fmt::format("{}_node{}", mName, node.name);

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
    }

    // Setup transform hierarchy
    for (int i = 0; i < mAsset.nodes.size(); i++) {
        fastgltf::Node& assetNode = mAsset.nodes[i];
        std::shared_ptr<Node> localNode = mNodes[i];
        for (auto& nodeChildIndex : assetNode.children) {
            localNode->mChildren.push_back(mNodes[nodeChildIndex]);
            mNodes[nodeChildIndex]->mParent = localNode;
        }
    }

    // Find the top nodes, with no parents
    for (auto& node : mNodes) {
        if (node->mParent.lock() == nullptr) {
            mTopNodes.push_back(node);
            node->refreshTransform(glm::mat4{ 1.f });
        }
    }
}

void GLTFModel::loadMaterialsConstantsBuffer(std::vector<MaterialConstants>& materialConstantsVector)
{
    std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mMaterialConstantsStagingBuffer.info.pMappedData), materialConstantsVector.data(), materialConstantsVector.size() * sizeof(MaterialConstants));

    vk::BufferCopy materialConstantsCopy{};
    materialConstantsCopy.dstOffset = 0;
    materialConstantsCopy.srcOffset = 0;
    materialConstantsCopy.size = materialConstantsVector.size() * sizeof(MaterialConstants);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        cmd.copyBuffer(*mRenderer->mRendererResources.mMaterialConstantsStagingBuffer.buffer, *mMaterialConstantsBuffer.buffer, materialConstantsCopy);
    });
}

void GLTFModel::loadInstancesBuffer(std::vector<InstanceData>& instanceDataVector)
{
    std::memcpy(static_cast<char*>(mRenderer->mRendererResources.mInstancesStagingBuffer.info.pMappedData), instanceDataVector.data(), instanceDataVector.size() * sizeof(InstanceData));

    vk::BufferCopy instancesCopy{};
    instancesCopy.dstOffset = 0;
    instancesCopy.srcOffset = 0;
    instancesCopy.size = instanceDataVector.size() * sizeof(InstanceData);

    mRenderer->mImmSubmit.submit([&](vk::raii::CommandBuffer& cmd) {
        cmd.copyBuffer(*mRenderer->mRendererResources.mInstancesStagingBuffer.buffer, *mInstancesBuffer.buffer, instancesCopy);
    });
}

void GLTFModel::loadMeshBuffers(Mesh* mesh, std::vector<uint32_t>& srcIndexVector, std::vector<Vertex>& srcVertexVector)
{
    const vk::DeviceSize srcVertexVectorSize = srcVertexVector.size() * sizeof(Vertex);
    const vk::DeviceSize srcIndexVectorSize = srcIndexVector.size() * sizeof(uint32_t);

    mesh->mVertexBuffer = mRenderer->mRendererResources.createBuffer(srcVertexVectorSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, VMA_MEMORY_USAGE_GPU_ONLY);
    mesh->mIndexBuffer = mRenderer->mRendererResources.createBuffer(srcIndexVectorSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, VMA_MEMORY_USAGE_GPU_ONLY);

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
    });
}

void GLTFModel::createInstance()
{
    mInstances.emplace_back(this);
    mInstances.back().mTransformComponents.translation = mRenderer->mCamera.mPosition + mRenderer->mCamera.getDirectionVector();
    mLatestId++;
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
        InstanceData instanceData { tm * rm * sm };
        instanceDataVector.push_back(instanceData);
    }
    loadInstancesBuffer(instanceDataVector);
}

void GLTFModel::markDelete()
{
    mDeleteSignal = mRenderer->mRendererInfrastructure.mFrameNumber + FRAME_OVERLAP;
}

void GLTFModel::load()
{
    initDescriptors();
    initBuffers();
    loadSamplers();
    loadImages();
	loadMaterials();
	loadMeshes();
	loadNodes();

    fmt::println("{} Model [Finished Loading]", mName);
}

void GLTFModel::generateRenderItems()
{
    if (!mDeleteSignal.has_value()) {
        for (auto& n : mTopNodes) {
            n->generateRenderItems(mRenderer, this, glm::mat4{ 1.f });
        }
    }
}

