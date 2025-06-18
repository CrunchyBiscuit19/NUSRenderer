#include <Data/Instance.h>

GLTFInstance::GLTFInstance(GLTFModel* model, int id, TransformData initialTransform) :
	mModel(model),
	mId(id),
	mDeleteSignal(false),
	mTransformComponents(initialTransform)
{
}
