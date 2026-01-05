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

bool frustumCull(Plane* frustum, inout float4[NUM_AABB_CORNERS] aabbCorners, AABB* bounds, Instance* currInstance, NodeTransform* nodeTransform) {
    bool visible = true;

    // Transform all corners of the AABB to world space
    // Get the min and max corners of the AABB 
    float4 aabbMin = float4(1.f);
    float4 aabbMax = float4(0.f);
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        aabbCorners[j] = mul(currInstance->transformMatrix, mul(*nodeTransform, aabbCorners[j]));
        aabbMin = min(aabbMin, aabbCorners[j]);
        aabbMax = max(aabbMax, aabbCorners[j]);
    }

    // For each plane build the corner which will produce max result in point-normal plane equation
    // Point-normal plane equation is positive if it's on the side of the plane the normal is pointing
    // Frustum plane normals point inward => non-negative result means at least 1 corner is on correct side of the plane
    // At least 1 corner on correct side of all 6 planes means render instance is within frustum and therefore visible
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

    // Transform AABB corners to NDC / Screen space and find min/max XY and closest Z
    // Vulkan NDC is [0, 1] for Z and [-1, 1] for XY
    // Reverse-Z in effect (1 == near, 0 == far)
    bool anyVisibleCorner = false;
    for (uint j = 0; j < NUM_AABB_CORNERS; j++) {
        float4 clipPos = mul(perspective->proj, mul(perspective->view, aabbCorners[j]));
        
        if (clipPos.w <= 0.0f) continue; // No idea what for

        anyVisibleCorner = true;

        float4 ndc = clipPos / clipPos.w;
        aabbCorners[j] = ndc;
        aabbMinNdc = min(aabbMinNdc, ndc);
        aabbMaxNdc = max(aabbMaxNdc, ndc);

        closestDepthInRenderInstance = max(closestDepthInRenderInstance, ndc.z);
    }

    if (!anyVisibleCorner) return true;

    // Convert NDC XY from [-1,1] to [0, drawExtents], and clamp it
    // Clamping because AABB spills outside of screen?
    float2 aabbMin = ((aabbMinNdc.xy * 0.5f) + 0.5f) * drawExtents;
    float2 aabbMax = ((aabbMaxNdc.xy * 0.5f) + 0.5f) * drawExtents;
    aabbMin = clamp(aabbMin, float2(0.0f, 0.0f), drawExtents);
    aabbMax = clamp(aabbMax, float2(0.0f, 0.0f), drawExtents);

    // Compute width, height, and center
    uint width = max(uint(aabbMax.x - aabbMin.x), 1u);
    uint height = max(uint(aabbMax.y - aabbMin.y), 1u);
    uint centreX = uint(aabbMin.x + float(width) * 0.5f);
    uint centreY = uint(aabbMin.y + float(height) * 0.5f);

    // Sample the pyramid using the center coordinates adjusted for mip level
    // Not perfect but whatever
    uint level = uint(floor(log2(float(max(width, height)))));
    float furthestDepthInPyramid = depthPyramidImage.Load(uint3(centreX >> level, centreY >> level, level));

    // Visible if closest is closer (greater than because of reverse-Z) than furthest depth
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
    //if (visible) visible = occlusionCull(aabbCorners, perspective, depthPyramidImage, cullerCullPc.drawExtents);

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