module Culler;

import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

public struct Plane {
    public float3 normal;
    public float d;
}

public struct AABB {
    public float3 min;
    public float3 max;
}

public struct CullerResetPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public uint preCullRenderItemsLimit;
};

public struct CullerCullPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public RenderInstance* renderInstancesBuffer;
    public uint* renderInstancesCountBuffer;
    public Plane* frustumBuffer;
    public Perspective* perspectiveBuffer;
    public AABB* mainBoundsBuffer;
    public NodeTransform* mainNodeTransformsBuffer;
    public Instance* mainInstancesBuffer;
    public uint *mainVisibleRenderInstancesInstanceIndexBuffer;
    public uint renderInstancesLimit;
};

public struct CullerCompactPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public RenderItem* postCullRenderItemsBuffer;
    public uint* postCullRenderItemsCountBuffer;
    public uint preCullRenderItemsLimit; 
};

public struct CullerDepthPyramidPushConstants {
    public uint2 depthPyramidExtent;
    public uint2 resolvedDepthExtent;
    public float2 resolvedDepthRatio;
    public uint level;
    public bool readFromResolved;
};

public struct CSInput {
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};