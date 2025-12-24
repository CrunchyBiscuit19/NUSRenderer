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
	AddressedBuffer mPostCullRenderInstancesCountBuffer;
	float mSceneUpdateTime;

	RendererStats(Renderer* renderer);

	void initBuffers();
	void reset();
	void cleanup();
};
