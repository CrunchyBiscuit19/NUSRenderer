#pragma once

#include <glm/glm.hpp>

class GLTFModel;

struct TransformData {
	glm::vec3 translation;
	glm::vec3 rotation;
	glm::f32 scale;
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

	GLTFInstance(GLTFModel* model);
};