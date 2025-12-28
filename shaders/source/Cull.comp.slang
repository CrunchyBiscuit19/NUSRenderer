import Cull;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const int MAX_CULL_LOCAL_SIZE = 256;

[[vk::push_constant]]
CullPushConstants cullPc;

[numthreads(MAX_CULL_LOCAL_SIZE,1,1)]

void main(CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.dispatchThreadID.x;
    if(renderInstancesIndex >= cullPc.renderInstancesCount) return;

    // Setup necessary data for culling test
    bool visible = true;
    RenderItem renderItem = cullPc.renderItemsBuffer[renderInstancesIndex];
    NodeTransform nodeTransform = cullPc.nodeTransformsBuffer[renderItem.nodeTransformIndex];
    Bounds bounds = cullPc.boundsBuffer[renderItem.boundsIndex];
    Instance firstInstance = cullPc.instancesBuffer[renderItem.firstInstance];
    Perspective perspective = cullPc.perspectiveBuffer[0];
    Frustum frustum = cullPc.frustumBuffer[0];

    // Culling test
    /*TODO culling*/

    // Increment counter of unculled render instances
    if(!visible) return;
    uint _; InterlockedAdd(cullPc.postCullRenderInstancesCountBuffer[0], 1, _);

    // Write unculled render instance data to another instance buffers
    uint writeInstanceIndex;
    //InterlockedAdd(renderItem.instanceCount, 1, writeInstanceIndex);

    /*cullPushConstants.postCullRenderItemsBuffer[postCullIndex] = cullPushConstants.preCullRenderItemsBuffer[preCullIndex];*/
}