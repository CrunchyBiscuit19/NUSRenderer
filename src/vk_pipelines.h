#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <fastgltf/types.hpp>

namespace vkutil {
vk::raii::ShaderModule loadShaderModule(std::filesystem::path filePath, vk::raii::Device& device);
};

struct PipelineBundle {
    vk::raii::Pipeline pipeline;
    vk::raii::PipelineLayout layout;

    PipelineBundle() :
		pipeline(nullptr),
		layout(nullptr)
    {}

	PipelineBundle(vk::raii::Pipeline pipeline, vk::raii::PipelineLayout layout) :
		pipeline(std::move(pipeline)),
		layout(std::move(layout))
	{}

    ~PipelineBundle() {
        cleanup();
    }

    PipelineBundle(PipelineBundle&& other) noexcept : 
        pipeline(std::move(other.pipeline)), 
        layout(std::move(other.layout)) 
    {}

    PipelineBundle& operator=(PipelineBundle&& other) noexcept {
        if (this != &other) {
            pipeline = std::move(other.pipeline);
            layout = std::move(other.layout);
        }
        return *this;
    }

    PipelineBundle(const PipelineBundle&) = delete;
    PipelineBundle& operator=(const PipelineBundle&) = delete;

    void cleanup() {
        layout.clear();
        pipeline.clear();
    }
};

struct PipelineOptions {
    bool doubleSided;
    fastgltf::AlphaMode alphaMode;

    inline bool operator==(const PipelineOptions& other) const
    {
        return (doubleSided == other.doubleSided && alphaMode == other.alphaMode);
    }
};

template <>
struct std::hash<PipelineOptions> {
    // Compute individual hash values for strings
    // Combine them using XOR and bit shifting
    inline std::size_t operator()(const PipelineOptions& k) const
    {
        return ((std::hash<bool>()(k.doubleSided) ^ (std::hash<fastgltf::AlphaMode>()(k.alphaMode) << 1)) >> 1);
    }
};

class GraphicsPipelineBuilder {
public:
    std::vector<vk::PipelineShaderStageCreateInfo> mShaderStages;
    vk::PipelineInputAssemblyStateCreateInfo mInputAssembly;
    vk::PipelineRasterizationStateCreateInfo mRasterizer;
    vk::PipelineColorBlendAttachmentState mColorBlendAttachment;
    vk::PipelineMultisampleStateCreateInfo mMultisampling;
    vk::PipelineDepthStencilStateCreateInfo mDepthStencil;
    vk::PipelineRenderingCreateInfo mRenderInfo;
    vk::Format mColorAttachmentformat;
	vk::PipelineLayout mPipelineLayout;

    GraphicsPipelineBuilder();

    void clear();
    vk::raii::Pipeline buildPipeline(vk::raii::Device& device) const;
    void setShaders(vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader);
    void setInputTopology(vk::PrimitiveTopology topology);
    void setPolygonMode(vk::PolygonMode mode);
    void setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace);
    void setMultisamplingNone();
    void disableBlending();
    void enableBlendingAdditive();
    void enableBlendingAlphablend();
    void setColorAttachmentFormat(vk::Format format);
    void setDepthFormat(vk::Format format);
    void disableDepthtest();
    void enableDepthtest(bool depthWriteEnable, vk::CompareOp op);
};

class ComputePipelineBuilder {
public:
    vk::PipelineShaderStageCreateInfo mComputeShaderStageCreateInfo;
    vk::PipelineLayout mPipelineLayout;

    ComputePipelineBuilder();

    void setShader(vk::ShaderModule computeShader);
    vk::raii::Pipeline buildPipeline(vk::raii::Device& device) const;
};
