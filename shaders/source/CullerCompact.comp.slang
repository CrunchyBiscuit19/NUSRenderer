import Culler;
import RenderItem;

static const int MAX_CULL_LOCAL_SIZE = 256;

[[vk::push_constant]]
CullerCompactPushConstants cullerCompactPc;

[numthreads(MAX_CULL_LOCAL_SIZE, 1, 1)]
void main(CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.dispatchThreadID.x;
    if (renderItemsIndex >= cullerCompactPc.preCullRenderItemsCount) return;

    // Copy over render items with more than zero instances, and increment counter
    RenderItem* currRenderItem = &cullerCompactPc.preCullRenderItemsBuffer[renderItemsIndex];
    
    if (currRenderItem->instanceCount == 0) return;
    
    uint writeRenderItemsIndex;
    InterlockedAdd(cullerCompactPc.postCullRenderItemsCountBuffer[0], 1, writeRenderItemsIndex);
    cullerCompactPc.postCullRenderItemsBuffer[writeRenderItemsIndex] = *currRenderItem;
}