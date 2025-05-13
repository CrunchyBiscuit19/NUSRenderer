#include <renderer.h>
#include <vk_initializers.h>
#include <vk_pipelines.h>
#include <materials.h>

PbrMaterial::PbrMaterial(Renderer* renderer) :
    mRenderer(renderer),
    mPipeline(nullptr)
{}

void PbrMaterial::getMaterialPipeline()
{
    PipelineOptions options{ mPbrData.doubleSided, mPbrData.alphaMode };
    mPipeline = mRenderer->mRendererInfrastructure.getMaterialPipeline(options);
}
