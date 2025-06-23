#include <Data/Instance.h>
#include <Data/Model.h>
#include <Renderer/Renderer.h>

GLTFInstance::GLTFInstance(GLTFModel* model, int id, InstanceData data) :
	mModel(model),
	mId(id),
	mDeleteSignal(false),
	mData(data)
{}

void GLTFInstance::markDelete()
{
	mDeleteSignal = true;
	mModel->mReloadLocalInstancesBuffer = true;
	mModel->getRenderer()->mScene.mFlags.instanceDestroyedFlag = true;
}
