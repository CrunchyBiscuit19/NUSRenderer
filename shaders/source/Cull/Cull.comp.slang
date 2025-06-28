import Cull;

static const int MAX_CULL_LOCAL_SIZE = 256;

[numthreads(MAX_CULL_LOCAL_SIZE)]

void main(CSInput in, uniform CullPushConstants cullPushConstants)
{
    uint renderItemsBufferIndex = in.dispatchThreadID.x;

    if (true) { // TODO frustum / occulsion cull
        uint visibleRenderItemsBufferIndex;
        InterlockedAdd(cullPushConstants.countBuffer[0], 1, visibleRenderItemsBufferIndex);

        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].indexCount = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].indexCount; 
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].instanceCount = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].instanceCount;
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].firstIndex = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].firstIndex;
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].vertexOffset = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].vertexOffset;
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].firstInstance = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].firstInstance;
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].materialIndex = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].materialIndex;
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].nodeTransformIndex = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].nodeTransformIndex;
        cullPushConstants.visibleRenderItemsBuffer[visibleRenderItemsBufferIndex].modelIndex = cullPushConstants.renderItemsBuffer[renderItemsBufferIndex].modelIndex;
    }
}