#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class RendererStats {
	Renderer* mRenderer;

public:
	float mFrameTime;
	float mDrawTime;
	uint32_t mDrawCallCount;
	uint32_t mPreCullRenderInstancesCount;
	uint32_t mPostCullMeshesCount;
	float mSceneUpdateTime;

	AddressedBuffer mTotalPostCullRenderInstancesCountBuffer;

	RendererStats(Renderer* renderer);

	void initBuffers();
	void reset();
	void cleanup();
};
