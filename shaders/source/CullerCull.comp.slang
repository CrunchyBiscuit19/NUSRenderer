import Culler;
import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

static const uint MAX_1D_WORKGROUP_THREADS = 1024;
static const uint NUM_FRUSTUM_PLANES = 6;
static const uint NUM_AABB_CORNERS = 8;

[[vk::binding(0, 0)]]
public uniform Texture2D<float> depthPyramidImage;

[[vk::push_constant]]
CullerCullPushConstants cullerCullPc;

/*
Transform all corners of the AABB to world space
Get the corner of the AABB which will give us the maximum result in the point-normal plane equation
Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
And since frustum plane normals point inward, a positive (or zero) result means that corrner of the AABB is on the correct side of the plane
If there is at least one corner inside all 6 planes, it is inside the frustum, the current render instance is considered visible
*/
bool frustumCull(Plane* frustum, inout float4[NUM_AABB_CORNERS] aabbCorners, AABB* bounds, Instance* currInstance, NodeTransform* nodeTransform) {
    bool visible = true;

    float4 aabbMin = float4(1.f);
    float4 aabbMax = float4(0.f);
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(currInstance->transformMatrix, mul(*nodeTransform, aabbCorners[j]));
        aabbMin = min(aabbMin, aabbCorners[j]);
        aabbMax = max(aabbMax, aabbCorners[j]);
    }

    for (uint i = 0; i < NUM_FRUSTUM_PLANES; i++) {
        Plane p = frustum[i];
        float4 chosen;
        chosen.x = (p.normal.x >= 0) ? aabbMax.x : aabbMin.x;
        chosen.y = (p.normal.y >= 0) ? aabbMax.y : aabbMin.y;
        chosen.z = (p.normal.z >= 0) ? aabbMax.z : aabbMin.z;
        chosen.w = 1.0;
        float result = dot(p.normal, chosen.xyz) - p.d;
        if (result < 0) {
            visible = false;
            break;
        }
    }
    return visible;
}

bool occlusionCull(inout float4[NUM_AABB_CORNERS] aabbCorners, Perspective *perspective, Texture2D<float> depthPyramidImage, float2 drawExtents) {
    float4 aabbMinNdc = float4(1.0f, 1.0f, 1.0f, 0.0f);
    float4 aabbMaxNdc = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float closestDepthInRenderInstance = 0.0f;

    bool anyVisibleCorner = false;
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        float4 clipPos = mul(perspective->proj, mul(perspective->view, aabbCorners[j]));
        
        if (clipPos.w <= 0.0f) continue;

        anyVisibleCorner = true;

        float4 ndc = clipPos / clipPos.w;
        aabbCorners[j] = ndc;
        aabbMinNdc = min(aabbMinNdc, ndc);
        aabbMaxNdc = max(aabbMaxNdc, ndc);

        closestDepthInRenderInstance = max(closestDepthInRenderInstance, ndc.z);
    }

    if (!anyVisibleCorner) return true;

    // Convert NDC XY [-1,1] -> screen coordinates [0, drawExtents]
    float2 aabbMin = ((aabbMinNdc.xy * 0.5f) + 0.5f) * drawExtents;
    float2 aabbMax = ((aabbMaxNdc.xy * 0.5f) + 0.5f) * drawExtents;

    // Clamp to screen extents
    aabbMin = clamp(aabbMin, float2(0.0f, 0.0f), drawExtents);
    aabbMax = clamp(aabbMax, float2(0.0f, 0.0f), drawExtents);

    // Compute width, height, and center
    uint width = max(uint(aabbMax.x - aabbMin.x), 1u);
    uint height = max(uint(aabbMax.y - aabbMin.y), 1u);
    uint centreX = uint(aabbMin.x + float(width) * 0.5f);
    uint centreY = uint(aabbMin.y + float(height) * 0.5f);

    // Determine mip level
    uint level = uint(floor(log2(float(max(width, height)))));

    // Sample the pyramid at the center of the tile (less conservative)
    float furthestDepthInPyramid = depthPyramidImage.Load(uint3(centreX >> level, centreY >> level, level));

    // Reverse-Z: visible if closest is closer than furthest depth
    return closestDepthInRenderInstance > furthestDepthInPyramid;
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
    Perspective* perspective = cullerCullPc.perspectiveBuffer;

    // Establish AABB corners
    float4[NUM_AABB_CORNERS] aabbCorners = {
        float4(bounds->min.x, bounds->min.y, bounds->min.z, 1.f),
        float4(bounds->min.x, bounds->min.y, bounds->max.z, 1.f),
        float4(bounds->min.x, bounds->max.y, bounds->min.z, 1.f),
        float4(bounds->min.x, bounds->max.y, bounds->max.z, 1.f),
        float4(bounds->max.x, bounds->min.y, bounds->min.z, 1.f),
        float4(bounds->max.x, bounds->min.y, bounds->max.z, 1.f),
        float4(bounds->max.x, bounds->max.y, bounds->min.z, 1.f),
        float4(bounds->max.x, bounds->max.y, bounds->max.z, 1.f),
    };

    // Culling tests
    bool visible = frustumCull(frustum, aabbCorners, bounds, instance, nodeTransform);
    if (visible) visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, cullerCullPc.drawExtents);

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

/*
bool occlusionCull(inout float4[NUM_AABB_CORNERS] aabbCorners, Perspective *perspective, Texture2D<float> depthPyramidImage, float2 drawExtents) {
    bool visible = false;

    float4 aabbMinNdc = float4(1.f);
    float4 aabbMaxNdc = float4(0.f);
    float closestDepthInRenderInstance = 0.f;
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(perspective->proj, mul(perspective->view, aabbCorners[j]));
        aabbCorners[j] = aabbCorners[j] / aabbCorners[j].w;
        aabbMinNdc = min(aabbMinNdc, aabbCorners[j]);
        aabbMaxNdc = max(aabbMaxNdc, aabbCorners[j]);
        closestDepthInRenderInstance = max(closestDepthInRenderInstance, aabbCorners[j].z);
    }

    float2 aabbMin = ((aabbMinNdc.xy * 0.5) + 0.5) * drawExtents;
    float2 aabbMax = ((aabbMaxNdc.xy * 0.5) + 0.5) * drawExtents;
    uint width = uint(aabbMax.x - aabbMin.x);
    width = clamp(width, 1, uint(drawExtents.x));
    uint centreX = uint(aabbMin.x + width * 0.5);
    uint height = uint(aabbMax.y - aabbMin.y);
    height = clamp(height, 1, uint(drawExtents.y));
    uint centreY = uint(aabbMin.y + height * 0.5);

    uint level = uint(floor(log2(max(width, height))));

    float furthestDepthInPyramid = depthPyramidImage.Load(uint3(centreX >> level, centreY >> level, level));
    if (closestDepthInRenderInstance > furthestDepthInPyramid) visible = true; // Closest > Furthest is correct because reverse-Z

    return visible;
}
*/