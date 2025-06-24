#pragma once

#include <glm/glm.hpp>

class GLTFModel;

struct InstanceData
{
	glm::mat4 transformMatrix;

	InstanceData() :
		transformMatrix(glm::mat4(1.f))
	{}

	InstanceData(glm::mat4&& transform) :
		transformMatrix(transform)
	{}
};

class GLTFInstance
{
public:
	GLTFModel* mModel;
	int mId;
	bool mDeleteSignal;

	InstanceData mData;

	GLTFInstance(GLTFModel* model, int id, InstanceData data = InstanceData());

	void markDelete();
};
