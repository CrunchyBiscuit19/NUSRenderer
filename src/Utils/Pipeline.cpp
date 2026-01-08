#include <Renderer/RendererInfrastructure.h>
#include <Utils/Helper.h>
#include <Utils/Pipeline.h>
#include <fmt/core.h>

#include <fstream>
#include <vulkan/vulkan.hpp>

GraphicsPipelineBuilder::GraphicsPipelineBuilder() { clear(); }

void GraphicsPipelineBuilder::clear() {
    // Clear all of the structs we need back to 0 with their correct sType
    mInputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};
    mRasterizer = vk::PipelineRasterizationStateCreateInfo{};
    mMultisampling = vk::PipelineMultisampleStateCreateInfo{};
    mDepthStencil = vk::PipelineDepthStencilStateCreateInfo{};
    mRenderInfo = vk::PipelineRenderingCreateInfo{};
    mPipelineLayout = nullptr;
    mShaderStages.clear();
}

vk::raii::Pipeline GraphicsPipelineBuilder::buildPipeline(vk::raii::Device& device) {
    vk::PipelineViewportStateCreateInfo viewportState = {};
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineDynamicStateCreateInfo dynamicInfo = {};
    constexpr vk::DynamicState state[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    vk::PipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = mPipelineColorBlendAttachmentStates.size();
    colorBlending.pAttachments = mPipelineColorBlendAttachmentStates.data();

    mRenderInfo.colorAttachmentCount = static_cast<u32>(mColorAttachmentFormats.size());
    mRenderInfo.pColorAttachmentFormats = mColorAttachmentFormats.data();

    constexpr vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo = {};
    graphicsPipelineInfo.pNext = &mRenderInfo;
    graphicsPipelineInfo.stageCount = static_cast<u32>(mShaderStages.size());
    graphicsPipelineInfo.pStages = mShaderStages.data();
    graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
    graphicsPipelineInfo.pInputAssemblyState = &mInputAssembly;
    graphicsPipelineInfo.pViewportState = &viewportState;
    graphicsPipelineInfo.pRasterizationState = &mRasterizer;
    graphicsPipelineInfo.pMultisampleState = &mMultisampling;
    graphicsPipelineInfo.pColorBlendState = &colorBlending;
    graphicsPipelineInfo.pDepthStencilState = &mDepthStencil;
    graphicsPipelineInfo.layout = mPipelineLayout;
    graphicsPipelineInfo.pDynamicState = &dynamicInfo;

    return vk::raii::Pipeline(device, nullptr, graphicsPipelineInfo);
}

void GraphicsPipelineBuilder::setShaders(vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader) {
    mShaderStages.clear();
    mShaderStages.push_back(vkhelper::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits::eVertex, vertexShader, "main"));
    mShaderStages.push_back(vkhelper::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits::eFragment, fragmentShader, "main"));
}

void GraphicsPipelineBuilder::setInputTopology(vk::PrimitiveTopology topology) {
    mInputAssembly.topology = topology;
    mInputAssembly.primitiveRestartEnable = VK_FALSE;  // For strips
}

void GraphicsPipelineBuilder::setPolygonMode(vk::PolygonMode mode) {
    mRasterizer.polygonMode = mode;
    mRasterizer.lineWidth = 1.f;
}

void GraphicsPipelineBuilder::setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace) {
    mRasterizer.cullMode = cullMode;
    mRasterizer.frontFace = frontFace;
}

void GraphicsPipelineBuilder::disableMultisampling() {
    mMultisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    mMultisampling.pSampleMask = nullptr;
    mMultisampling.alphaToCoverageEnable = VK_FALSE;
    mMultisampling.alphaToOneEnable = VK_FALSE;
}

void GraphicsPipelineBuilder::enableMultisampling() { mMultisampling.rasterizationSamples = MSAA_LEVEL; }

void GraphicsPipelineBuilder::disableSampleShading() {
    mMultisampling.sampleShadingEnable = VK_FALSE;
    mMultisampling.minSampleShading = 1.0f;
}

void GraphicsPipelineBuilder::enableSampleShading() {
    mMultisampling.sampleShadingEnable = VK_TRUE;
    mMultisampling.minSampleShading = 1.0f;
}

void GraphicsPipelineBuilder::addColorAttachment(vk::Format format, vk::PipelineColorBlendAttachmentState blendState) {
    mColorAttachmentFormats.emplace_back(format);
    mPipelineColorBlendAttachmentStates.emplace_back(blendState);
}

void GraphicsPipelineBuilder::setDepthFormat(vk::Format format) { mRenderInfo.depthAttachmentFormat = format; }

void GraphicsPipelineBuilder::disableDepthTest() {
    mDepthStencil.depthTestEnable = VK_FALSE;
    mDepthStencil.depthWriteEnable = VK_FALSE;
    mDepthStencil.depthCompareOp = vk::CompareOp::eNever;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = vk::StencilOpState{};
    mDepthStencil.back = vk::StencilOpState{};
    mDepthStencil.minDepthBounds = 0.f;
    mDepthStencil.maxDepthBounds = 1.f;
}

void GraphicsPipelineBuilder::enableDepthTest(bool depthWriteEnable, vk::CompareOp op) {
    mDepthStencil.depthTestEnable = VK_TRUE;
    mDepthStencil.depthWriteEnable = depthWriteEnable;
    mDepthStencil.depthCompareOp = op;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = vk::StencilOpState{};
    mDepthStencil.back = vk::StencilOpState{};
    mDepthStencil.minDepthBounds = 0.f;
    mDepthStencil.maxDepthBounds = 1.f;
}

ComputePipelineBuilder::ComputePipelineBuilder() {}

void ComputePipelineBuilder::setShader(vk::ShaderModule computeShader) {
    mComputeShaderStageCreateInfo = vkhelper::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits::eCompute, computeShader, "main");
}

vk::raii::Pipeline ComputePipelineBuilder::buildPipeline(vk::raii::Device& device) {
    vk::ComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.layout = mPipelineLayout;
    computePipelineInfo.stage = mComputeShaderStageCreateInfo;
    computePipelineInfo.pNext = nullptr;

    return vk::raii::Pipeline(device, nullptr, computePipelineInfo);
}
