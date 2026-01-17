#include <Data/Instance.h>
#include <Data/Model.h>
#include <Renderer/Renderer.h>
#include <Utils/Types.h>

GLTFInstance::GLTFInstance(GLTFModel* model, u32 id, InstanceData data) : mModel(model), mId(id), mToDelete(false), mData(data) {}

void GLTFInstance::markDelete() {
    mToDelete = true;
    mModel->mReloadInstances = true;
    mModel->getRenderer()->mScene.mFlags.instanceDestroyedFlag = true;

    if (this == mModel->getRenderer()->mScene.mPicker.mClickedInstance) {
        mModel->getRenderer()->mScene.mPicker.mClickedInstance = nullptr;
    }
}
