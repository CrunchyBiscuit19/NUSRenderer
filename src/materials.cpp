#include <renderer.h>
#include <vk_initializers.h>
#include <vk_pipelines.h>
#include <materials.h>

vk::raii::DescriptorSetLayout PbrMaterial::mResourcesDescriptorSetLayout = nullptr;

PbrMaterial::PbrMaterial(Renderer* renderer, DescriptorAllocatorGrowable* descriptorAllocator) :
    mRenderer(renderer),
    mDescriptorAllocator(descriptorAllocator),
    mPipeline(nullptr),
    mResourcesDescriptorSet(nullptr)
{
}

void PbrMaterial::createResourcesDescriptorSetLayout(Renderer* renderer)
{
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler);
    builder.addBinding(1, vk::DescriptorType::eCombinedImageSampler);
    builder.addBinding(2, vk::DescriptorType::eCombinedImageSampler);
    builder.addBinding(3, vk::DescriptorType::eCombinedImageSampler);
    builder.addBinding(4, vk::DescriptorType::eCombinedImageSampler);
    mResourcesDescriptorSetLayout = builder.build(renderer->mRendererCore.mDevice, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
}

void PbrMaterial::getMaterialPipeline()
{
    PipelineOptions options{ mPbrData.doubleSided, mPbrData.alphaMode };
    mPipeline = mRenderer->mRendererInfrastructure.getMaterialPipeline(options);
}

void PbrMaterial::writeMaterialResources()
{
    getMaterialPipeline();
    mResourcesDescriptorSet = mDescriptorAllocator->allocate(*mResourcesDescriptorSetLayout);

    DescriptorWriter writer;
    writer.writeImage(0, *mPbrData.resources.base.image->imageView, mPbrData.resources.base.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.writeImage(1, *mPbrData.resources.metallicRoughness.image->imageView, mPbrData.resources.metallicRoughness.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.writeImage(2, *mPbrData.resources.normal.image->imageView, mPbrData.resources.normal.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.writeImage(3, *mPbrData.resources.occlusion.image->imageView, mPbrData.resources.occlusion.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.writeImage(4, *mPbrData.resources.emissive.image->imageView, mPbrData.resources.emissive.sampler, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
    writer.updateSet(mRenderer->mRendererCore.mDevice, *mResourcesDescriptorSet);
}
 