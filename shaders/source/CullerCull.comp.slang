import Culler;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const int MAX_CULL_LOCAL_SIZE = 256;

[[vk::push_constant]]
CullerCullPushConstants cullerCullPc;

[numthreads(MAX_CULL_LOCAL_SIZE,1,1)]
void main(CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.dispatchThreadID.x;
    if (renderInstancesIndex >= cullerCullPc.renderInstancesCount) return;

    // Grab representative data for this thread
    bool visible = true;
    RenderInstance renderInstance = cullerCullPc.renderInstancesBuffer[renderInstancesIndex];
    RenderItem *renderItem = &cullerCullPc.renderItemsBuffer[renderInstance.renderItemsIndex];

    // Grab data for culling test
    Instance currInstance = cullerCullPc.preCullInstancesBuffer[renderInstance.instanceIndex];
    NodeTransform nodeTransform = cullerCullPc.nodeTransformsBuffer[renderInstance.nodeTransformIndex];
    Perspective perspective = cullerCullPc.perspectiveBuffer[0];
    Bounds bounds = cullerCullPc.boundsBuffer[renderInstance.boundsIndex];
    Frustum frustum = cullerCullPc.frustumBuffer[0];

    // Culling test
    /*TODO culling, always unculled for now*/

    // Increment counter of unculled render instances
    if (!visible) return;
    uint _; InterlockedAdd(cullerCullPc.postCullRenderInstancesCountBuffer[0], 1, _);

    // Write unculled render instance data to another instance buffers
    uint writeInstanceIndex;
    InterlockedAdd(renderItem->instanceCount, 1, writeInstanceIndex); // Do another pass to zero out after drawing?
    cullerCullPc.postCullInstancesBuffer[renderInstance.firstInstance + writeInstanceIndex] = currInstance;
}