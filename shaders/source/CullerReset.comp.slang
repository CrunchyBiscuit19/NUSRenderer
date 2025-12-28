import Culler;
import RenderItem;

static const int MAX_CULL_LOCAL_SIZE = 256;

[[vk::push_constant]]
CullerResetPushConstants cullerResetPc;

[numthreads(MAX_CULL_LOCAL_SIZE, 1, 1)]
void main(CSInput in) {
    // Get thread ID as index into render item
    uint renderItemsIndex = in.dispatchThreadID.x;
    if (renderItemsIndex >= cullerResetPc.renderItemsCount) return;

    // Reset render item instance count to zero
    cullerResetPc.renderItemsBuffer[renderItemsIndex].instanceCount = 0;
}