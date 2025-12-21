import Cull;

static const int MAX_CULL_LOCAL_SIZE = 256;

[numthreads(MAX_CULL_LOCAL_SIZE)]

void main(CSInput in, uniform CullPushConstants cullPushConstants) {
    uint preCullIndex = in.dispatchThreadID.x;

    if (true) { // TODO frustum / occulsion cull
        uint postCullIndex;
        InterlockedAdd(cullPushConstants.countBuffer[0], 1, postCullIndex);

        cullPushConstants.postCullRenderItemsBuffer[postCullIndex] = cullPushConstants.preCullRenderItemsBuffer[preCullIndex];
    }
}