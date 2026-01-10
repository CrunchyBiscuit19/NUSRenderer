#pragma once

#include <Renderer/RendererResources.h>

class Renderer;

class RendererStats {
    Renderer* mRenderer;

   public:
    float mFrameTime;
    float mDrawTime;
    u32 mDrawCallCount;
    u32 mPreCullRenderInstancesCount;
    AllocatedBuffer mRenderInstancesCountBuffer;
    float mSceneUpdateTime;

    RendererStats(Renderer* renderer);

    void initBuffers();
    void reset();
    void cleanup();
};
