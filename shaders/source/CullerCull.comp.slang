import Culler;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const uint MAX_1D_WORKGROUP_THREADS = 1024;
static const uint NUM_FRUSTUM_PLANES = 6;
static const uint NUM_AABB_CORNERS = 8;

[[vk::push_constant]]
CullerCullPushConstants cullerCullPc;

/*
Transform all corners of the AABB to world space
Get the corner of the AABB which will give us the maximum result in the point-normal plane equation
Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
And since frustum plane normals point inward, a positive (or zero) result means that corrner of the AABB is on the correct side of the plane
If there is at least one corner inside all 6 planes, it is inside the frustum, the current render instance is considered visible
*/
bool frustumCull(Plane* frustum, AABB* bounds, Instance* currInstance, NodeTransform* nodeTransform) {
    bool visible = true;
    float4[NUM_AABB_CORNERS] newCorners = {
        float4(bounds->min.x, bounds->min.y, bounds->min.z, 1.f),
        float4(bounds->min.x, bounds->min.y, bounds->max.z, 1.f),
        float4(bounds->min.x, bounds->max.y, bounds->min.z, 1.f),
        float4(bounds->min.x, bounds->max.y, bounds->max.z, 1.f),
        float4(bounds->max.x, bounds->min.y, bounds->min.z, 1.f),
        float4(bounds->max.x, bounds->min.y, bounds->max.z, 1.f),
        float4(bounds->max.x, bounds->max.y, bounds->min.z, 1.f),
        float4(bounds->max.x, bounds->max.y, bounds->max.z, 1.f),
    };
    float4 newMin = float4(1.f);
    float4 newMax = float4(0.f);
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        newCorners[j] = mul(currInstance->transformMatrix, mul(*nodeTransform, newCorners[j]));
        newMin = min(newMin, newCorners[j]);
        newMax = max(newMax, newCorners[j]);
    }

    for (uint i = 0; i < NUM_FRUSTUM_PLANES; i++) {
        Plane p = frustum[i];
        float4 chosen;
        chosen.x = (p.normal.x >= 0) ? newMax.x : newMin.x;
        chosen.y = (p.normal.y >= 0) ? newMax.y : newMin.y;
        chosen.z = (p.normal.z >= 0) ? newMax.z : newMin.z;
        chosen.w = 1.0;
        float result = dot(p.normal, chosen.xyz) - p.d;
        if (result < 0) {
            visible = false;
            break;
        }
    }
    return visible;
}

[numthreads(MAX_1D_WORKGROUP_THREADS,1,1)]
void main(CSInput in) {
    // Get thread ID as index into which render instance for culling check
    uint renderInstancesIndex = in.dispatchThreadID.x;
    if (renderInstancesIndex >= cullerCullPc.renderInstancesLimit) return;

    // Grab representative data for this thread
    RenderInstance renderInstance = cullerCullPc.renderInstancesBuffer[renderInstancesIndex];
    RenderItem *renderItem = &cullerCullPc.preCullRenderItemsBuffer[renderInstance.renderItemsIndex];

    // Grab data for culling test
    Instance* instance = &cullerCullPc.mainInstancesBuffer[renderInstance.mainInstanceIndex];
    NodeTransform* nodeTransform = &cullerCullPc.mainNodeTransformsBuffer[renderItem->nodeTransformIndex];
    AABB* bounds = &cullerCullPc.mainBoundsBuffer[renderItem->boundsIndex];
    Plane* frustum = cullerCullPc.frustumBuffer;

    // Culling tests
    bool visible = frustumCull(frustum, bounds, instance, nodeTransform);

    // Increment counter of unculled render instances
    if (!visible) return;
    uint _; InterlockedAdd(cullerCullPc.renderInstancesCountBuffer[0], 1, _);

    // Write the indices of visible render instances to the visible instances indices buffer
    // Index into visible instances indices buffer with (in.instanceIndex + firstInstance) and get the index into the instances buffer
    // Then index into the instances buffer with that index to get the actual instance data
    uint offsetVisibleRenderInstanceIndex;
    InterlockedAdd(renderItem->renderInstanceCount, 1, offsetVisibleRenderInstanceIndex);
    uint finalVisibleRenderInstanceIndex = renderItem->firstRenderInstance + offsetVisibleRenderInstanceIndex;
    cullerCullPc.mainVisibleRenderInstancesInstanceIndexBuffer[finalVisibleRenderInstanceIndex] = renderInstance.mainInstanceIndex;
}