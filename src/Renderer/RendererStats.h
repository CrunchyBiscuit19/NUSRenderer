#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class RendererStats {
	Renderer* mRenderer;

public:
	float mFrameTime;
	float mDrawTime;
	uint32_t mDrawCallCount;
	uint32_t mPreCullMeshesCount;
	uint32_t mPostCullMeshesCount;
	float mSceneUpdateTime;

	AddressedBuffer mTotalPostCullCountBuffer;

	RendererStats(Renderer* renderer);

	void initBuffers();
	void reset();
	void cleanup();
};
