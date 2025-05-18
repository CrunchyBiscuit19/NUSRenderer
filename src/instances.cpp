#include <instances.h>

GLTFInstance::GLTFInstance(GLTFModel* model) :
    mModel(model),
    mId(0),
    mDeleteSignal(false)
{
    mTransformComponents.translation = glm::vec3();
    mTransformComponents.rotation = glm::vec3();
    mTransformComponents.scale = 1;
}   