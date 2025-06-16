#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <fastgltf/types.hpp>

struct PipelineBundle {
	int id;
	vk::raii::Pipeline pipeline;
	vk::PipelineLayout layout;

	PipelineBundle() :
		pipeline(nullptr),
		layout(nullptr),
		id(-1)
	{}

	PipelineBundle(int id_, vk::raii::Pipeline pipeline, vk::PipelineLayout layout) :
		pipeline(std::move(pipeline)),
		layout(layout),
		id(id_)
	{}

	~PipelineBundle() 
	{
		cleanup();
	}

	void cleanup()
	{
		pipeline.clear();
	}

	PipelineBundle(PipelineBundle&& other) noexcept :
		pipeline(std::move(other.pipeline)),
		layout(std::move(other.layout)),
		id(other.id)
	{}

	PipelineBundle& operator=(PipelineBundle&& other) noexcept {
		if (this != &other) {
			pipeline = std::move(other.pipeline);
			layout = std::move(other.layout);
			id = other.id;
		}
		return *this;
	}

	PipelineBundle(const PipelineBundle&) = delete;
	PipelineBundle& operator=(const PipelineBundle&) = delete;
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

class PipelineBuilder {
public:
	vk::PipelineLayout mPipelineLayout;

	virtual vk::raii::Pipeline buildPipeline(vk::raii::Device& device) = 0;
};

class GraphicsPipelineBuilder : PipelineBuilder {
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
	vk::raii::Pipeline buildPipeline(vk::raii::Device& device) override;
	void setShaders(vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader);
	void setInputTopology(vk::PrimitiveTopology topology);
	void setPolygonMode(vk::PolygonMode mode);
	void setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace);
	void disableMultisampling();
	void enableMultisampling();
	void disableSampleShading();
	void enableSampleShading();
	void disableBlending();
	void enableBlendingAdditive();
	void enableBlendingAlpha();
	void enableBlendingSkybox();
	void setColorAttachmentFormat(vk::Format format);
	void setDepthFormat(vk::Format format);
	void disableDepthtest();
	void enableDepthTest(bool depthWriteEnable, vk::CompareOp op);
};

class ComputePipelineBuilder : PipelineBuilder {
public:
	vk::PipelineShaderStageCreateInfo mComputeShaderStageCreateInfo;
	vk::PipelineLayout mPipelineLayout;

	ComputePipelineBuilder();

	vk::raii::Pipeline buildPipeline(vk::raii::Device& device) override;
	void setShader(vk::ShaderModule computeShader);
};
