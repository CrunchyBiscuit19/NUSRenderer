#include <Renderer/Renderer.h>
#include <Utils/Helper.h>
#include <Utils/Pipeline.h>
#include <Data/Material.h>

PbrMaterial::PbrMaterial(Renderer* renderer) :
	mRenderer(renderer),
	mPipeline(nullptr)
{
}

void PbrMaterial::getMaterialPipeline()
{
	PipelineOptions options{ mPbrData.doubleSided, mPbrData.alphaMode };
	mPipeline = mRenderer->mRendererInfrastructure.getMaterialPipeline(options);
}