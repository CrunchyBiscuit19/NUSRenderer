import Culler;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const uint MAX_CULL_LOCAL_SIZE = 256;
static const uint NUM_FRUSTUM_PLANES = 6;

[[vk::push_constant]]
CullerCullPushConstants cullerCullPc;

/*
Get the corner of the AABB which will give us the maximum result in the point-normal plane equation
Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
And since frustum plane normals point inward, a positive (or zero) result means that corrner of the AABB is "inside" the plane
If at least that chosen corner is inside all 6 planes, it is inside the frustum, the current render instance is considered visible
*/
bool frustumCull(Plane* frustum, Bounds* bounds, Instance* currInstance, NodeTransform* nodeTransform) {
    bool visible = true;
    for (uint i = 0; i < NUM_FRUSTUM_PLANES; i++) {
        Plane p = frustum[i];
        float4 chosen;
        chosen.x = (p.normal.x >= 0) ? bounds->max.x : bounds->min.x;
        chosen.y = (p.normal.y >= 0) ? bounds->max.y : bounds->min.y;
        chosen.z = (p.normal.z >= 0) ? bounds->max.z : bounds->min.z;
        chosen.w = 1.0;
        chosen = mul(currInstance->transformMatrix, mul(*nodeTransform, chosen));
        float result = dot(p.normal, chosen.xyz) - p.d;
        if (result < 0) {
            visible = false;
            break;
        }
    }
    return visible;
}

[numthreads(MAX_CULL_LOCAL_SIZE,1,1)]
void main(CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.dispatchThreadID.x;
    if (renderInstancesIndex >= cullerCullPc.renderInstancesLimit) return;

    // Grab representative data for this thread
    RenderInstance renderInstance = cullerCullPc.renderInstancesBuffer[renderInstancesIndex];
    RenderItem *renderItem = &cullerCullPc.preCullRenderItemsBuffer[renderInstance.renderItemsIndex];

    // Grab data for culling test
    Instance* instance = &cullerCullPc.instancesBuffer[renderInstance.instanceIndex];
    NodeTransform* nodeTransform = &cullerCullPc.nodeTransformsBuffer[renderInstance.nodeTransformIndex];
    Bounds* bounds = &cullerCullPc.boundsBuffer[renderInstance.boundsIndex];
    Plane* frustum = cullerCullPc.frustumBuffer;

    // Culling tests
    bool visible = frustumCull(frustum, bounds, instance, nodeTransform);
    //visible = true;

    // Increment counter of unculled render instances
    if (!visible) return;
    uint _; InterlockedAdd(cullerCullPc.renderInstancesCountBuffer[0], 1, _);

    // Write the indices of visible render instances to the visible instances indices buffer
    // Index into visible instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetInstanceIndicesBufferIndex;
    InterlockedAdd(renderItem->instanceCount, 1, offsetInstanceIndicesBufferIndex);
    uint finalInstanceIndicesBufferIndex = renderInstance.firstInstance + offsetInstanceIndicesBufferIndex;
    uint finalInstanceBufferIndex = renderInstance.firstInstance + renderInstance.instanceIndex;
    cullerCullPc.visibleRenderInstancesInstanceIndexBuffer[finalInstanceIndicesBufferIndex] = finalInstanceBufferIndex;
}