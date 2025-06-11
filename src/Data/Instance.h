#pragma once

#include <glm/glm.hpp>

class GLTFModel;

struct TransformData {
	glm::vec3 translation;
	glm::vec3 rotation;
	glm::f32 scale;

	TransformData(): 
		translation(glm::vec3()),
		rotation(glm::vec3()),
		scale(1.f)
	{
	}

	TransformData(glm::vec3 translation, glm::vec3 rotation, glm::f32 scale) :
		translation(translation),
		rotation(rotation),
		scale(scale)
	{
	}
};

struct InstanceData {
	glm::mat4 transformMatrix;
};

class GLTFInstance {
public:
	GLTFModel* mModel;
	int mId;
	bool mDeleteSignal;
	TransformData mTransformComponents;

	GLTFInstance(GLTFModel* model, int id, TransformData initialTransform = TransformData());
};