#include <Data/Instance.h>
#include <Data/Model.h>

GLTFInstance::GLTFInstance(GLTFModel* model) :
	mModel(model),
	mId(model->mLatestId),
	mDeleteSignal(false)
{
	mTransformComponents.translation = glm::vec3();
	mTransformComponents.rotation = glm::vec3();
	mTransformComponents.scale = 1;
}