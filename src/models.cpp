#include <renderer.h>
#include <models.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define UUID_SYSTEM_GENERATOR   
#include <uuid.h>
#include <fmt/core.h>

GLTFModel::GLTFModel(Renderer* renderer, std::filesystem::path modelPath):
	mRenderer(renderer),
    mName(modelPath.stem().string()),
	mToDelete(false)
{
    fmt::println("{} Model [Open File]", modelPath.filename().string());

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
                newImage = mRenderer->mResourceManager.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
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
                newImage = mRenderer->mResourceManager.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
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
                            newImage = mRenderer->mResourceManager.createImage(data, imagesize, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, true);
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
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { 
        { vk::DescriptorType::eUniformBuffer, 1 },
        { vk::DescriptorType::eCombinedImageSampler, 5 },
    };
    mDescriptorAllocator.init(mRenderer->mDevice, mAsset.materials.size(), sizes);
}

void GLTFModel::initBuffers()
{
    fmt::println("{} Model [Create Buffers]", mName);

    mMaterialConstantsBuffer = mRenderer->mResourceManager.createBuffer(MAX_MATERIALS * sizeof(MaterialConstants),
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mInstanceBuffer = mRenderer->mResourceManager.createBuffer(MAX_INSTANCES * sizeof(TransformationData),
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
        mSamplers.emplace_back(mRenderer->mDevice.createSampler(sampl));
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

        newMat->mPbrData.resources.base = { &mRenderer->mDefaultImages[DefaultImage::White], *mRenderer->mDefaultSampler };
        newMat->mPbrData.resources.metallicRoughness = { &mRenderer->mDefaultImages[DefaultImage::White], *mRenderer->mDefaultSampler };
        newMat->mPbrData.resources.normal = { &mRenderer->mDefaultImages[DefaultImage::White], *mRenderer->mDefaultSampler };
        newMat->mPbrData.resources.occlusion = { &mRenderer->mDefaultImages[DefaultImage::White], * mRenderer->mDefaultSampler };
        newMat->mPbrData.resources.emissive = { &mRenderer->mDefaultImages[DefaultImage::White], *mRenderer->mDefaultSampler };

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

    mRenderer->mResourceManager.loadMaterialsConstantsBuffer(this, materialConstants);
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

        mRenderer->mResourceManager.loadMeshBuffers(newMesh.get(), indices, vertices);

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

void GLTFModel::load()
{
    initDescriptors();
    initBuffers();
    loadSamplers();
    loadImages();
	loadMaterials();
	loadMeshes();
	loadNodes();

    mLoaded = true;

    fmt::println("{} Model [Finished Loading]", mName);
}

void GLTFModel::generateRenderItems()
{
    for (auto& n : mTopNodes) {
        n->generateRenderItems(mRenderer, glm::mat4 { 1.f });
    }
}

GLTFInstance::GLTFInstance() :
    id(0),
    toDelete(false)
{
    data.transformation.rotation = glm::vec3();
    data.transformation.scale = 1;
    data.transformation.translation = glm::vec3();
}   

