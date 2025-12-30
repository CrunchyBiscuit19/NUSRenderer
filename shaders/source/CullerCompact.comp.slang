import Culler;
import RenderItem;

static const uint MAX_CULL_LOCAL_SIZE = 256;

[[vk::push_constant]]
CullerCompactPushConstants cullerCompactPc;

[numthreads(MAX_CULL_LOCAL_SIZE, 1, 1)]
void main(CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.dispatchThreadID.x;
    if (renderItemsIndex >= cullerCompactPc.preCullRenderItemsLimit) return;

    // Copy over render items with more than zero instances, and increment counter
    RenderItem* renderItem = &cullerCompactPc.preCullRenderItemsBuffer[renderItemsIndex];
    
    if (renderItem->instanceCount == 0) return;
    
    uint writeRenderItemsIndex;
    InterlockedAdd(cullerCompactPc.postCullRenderItemsCountBuffer[0], 1, writeRenderItemsIndex);
    cullerCompactPc.postCullRenderItemsBuffer[writeRenderItemsIndex] = *renderItem;
}