#include <Data/Instance.h>
#include <Data/Model.h>

GLTFInstance::GLTFInstance(GLTFModel* model, int id, TransformData initialTransform) :
	mModel(model),
	mId(id),
	mTransformComponents(initialTransform),
	mDeleteSignal(false)
{
}