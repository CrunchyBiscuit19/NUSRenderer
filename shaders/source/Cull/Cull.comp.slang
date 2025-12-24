import Cull;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const int MAX_CULL_LOCAL_SIZE = 256;

[numthreads(MAX_CULL_LOCAL_SIZE,1,1)]

void main(CSInput in, uniform CullPushConstants cullPushConstants) {
    uint renderInstancesIndex = in.dispatchThreadID.x;
    if(renderInstancesIndex >= cullPushConstants.renderInstancesCount) return;

    bool visible = true;

    /*RenderItem renderItem = cullPushConstants.preCullRenderItemsBuffer[preCullIndex];
    NodeTransform nodeTransform = cullPushConstants.nodeTransformsBuffer[renderItem.nodeTransformIndex];
    Bounds bounds = cullPushConstants.boundsBuffer[renderItem.boundsIndex];
    Instance firstInstance = cullPushConstants.instancesBuffer[renderItem.firstInstance]; // Problem child
    Perspective perspective = *cullPushConstants.perspectiveBuffer;
    Frustum frustum = *cullPushConstants.frustumBuffer;*/

    if(!visible) return;

    uint _;
    InterlockedAdd(cullPushConstants.postCullRenderInstancesCountBuffer[0], 1, _);

    uint writeInstanceIndex;

    /*cullPushConstants.postCullRenderItemsBuffer[postCullIndex] = cullPushConstants.preCullRenderItemsBuffer[preCullIndex];*/
}