module Cull;

import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

public struct Plane {
    public float3 normal;
    public float d;
}

public struct CullPushConstants
{
    public RenderItem *preCullRenderItemsBuffer;
    public RenderItem *postCullRenderItemsBuffer;
    public uint *postCullCountBuffer;
    public uint *totalPostCullCountBuffer;
    public float4* boundsBuffer;
    public Plane* frustumBuffer;  // (vec4 planes[6]) -> 96 bytes
    public NodeTransform* nodeTransformsBuffer;
    public Instance* instancesBuffer;
    public Perspective* perspectiveBuffer;
    public uint preCullRenderItemsCount;
};

public struct CSInput
{
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};