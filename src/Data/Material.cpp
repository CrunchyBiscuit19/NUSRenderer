#include <Renderer/Renderer.h>
#include <Utils/Helper.h>
#include <Utils/Pipeline.h>
#include <Data/Material.h>

PbrMaterial::PbrMaterial(Renderer* renderer) :
	mRenderer(renderer),
	mPipelineBundle(nullptr)
{
}

void PbrMaterial::getMaterialPipeline()
{
	PipelineOptions options{ mPbrData.doubleSided, mPbrData.alphaMode };
	mPipelineBundle = mRenderer->mRendererInfrastructure.getMaterialPipeline(options);
}