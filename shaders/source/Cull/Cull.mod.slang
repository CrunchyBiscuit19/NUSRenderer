module Cull;

import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

public struct Plane {
    public float3 normal;
    public float d;
}

public struct Frustum {
    public Plane near;
    public Plane far;
    public Plane left;
    public Plane right;
    public Plane top;
    public Plane bottom;
}

public struct Bounds {
    public float3 min;
    public float3 max;
}

public struct CullPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public RenderItem* postCullRenderItemsBuffer;
    public uint* preCullRenderInstancesBuffer;
    public uint* postCullRenderInstancesCountBuffer;
    public uint* totalPostCullRenderInstancesCountBuffer;
    public Bounds* boundsBuffer;
    public Frustum* frustumBuffer;
    public NodeTransform* nodeTransformsBuffer;
    public Instance* instancesBuffer;
    public Perspective* perspectiveBuffer;
    public uint preCullRenderInstancesCount;
};

public struct CSInput {
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};