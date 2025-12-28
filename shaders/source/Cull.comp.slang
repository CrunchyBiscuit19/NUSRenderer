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

    // Grab representative data for this thread
    bool visible = true;
    RenderInstance renderInstance = cullPc.renderInstancesBuffer[renderInstancesIndex];
    RenderItem* renderItem = &cullPc.renderItemsBuffer[renderInstance.renderItemIndex];

    // Grab data for culling test
    Instance currInstance = cullPc.preCullInstancesBuffer[renderInstance.instanceIndex];
    NodeTransform nodeTransform = cullPc.nodeTransformsBuffer[renderInstance.nodeTransformIndex];
    Perspective perspective = cullPc.perspectiveBuffer[0];
    Bounds bounds = cullPc.boundsBuffer[renderInstance.boundsIndex];
    Frustum frustum = cullPc.frustumBuffer[0];

    // Culling test
    /*TODO culling, always unculled for now*/

    // Increment counter of unculled render instances
    if(!visible) return;
    uint _; InterlockedAdd(cullPc.postCullRenderInstancesCountBuffer[0], 1, _);

    // Write unculled render instance data to another instance buffers
    uint writeInstanceIndex;
    InterlockedAdd(renderItem->instanceCount, 1, writeInstanceIndex); // Do another pass to zero out after drawing?
    cullPc.postCullInstancesBuffer[renderInstance.firstInstance + writeInstanceIndex] = currInstance;
}