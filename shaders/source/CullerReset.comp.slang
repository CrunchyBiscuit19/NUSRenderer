import Culler;
import RenderItem;

static const uint MAX_CULL_LOCAL_SIZE = 256;

[[vk::push_constant]]
CullerResetPushConstants cullerResetPc;

[numthreads(MAX_CULL_LOCAL_SIZE, 1, 1)]
void main(CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.dispatchThreadID.x;
    if (renderItemsIndex >= cullerResetPc.preCullRenderItemsCount) return;

    // Reset render item instance count to zero
    cullerResetPc.preCullRenderItemsBuffer[renderItemsIndex].instanceCount = 0;
}