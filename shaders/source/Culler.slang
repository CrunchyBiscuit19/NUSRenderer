module Culler;

import Perspective;
import NodeTransform;
import Instance;
import RenderItem;

public struct Plane {
    public float3 normal;
    public float d;
}

public struct Bounds {
    public float4 min;
    public float4 max;
}

public struct CullerResetPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public uint preCullRenderItemsLimit;
};

public struct CullerCullPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public RenderInstance* renderInstancesBuffer;
    public uint* renderInstancesCountBuffer;
    public Bounds* boundsBuffer;
    public Plane* frustumBuffer;
    public NodeTransform* nodeTransformsBuffer;
    public Instance* instancesBuffer;
    public uint* visibleRenderInstancesInstanceIndexBuffer;
    public uint renderInstancesLimit;
};

public struct CullerCompactPushConstants {
    public RenderItem* preCullRenderItemsBuffer;
    public RenderItem* postCullRenderItemsBuffer;
    public uint* postCullRenderItemsCountBuffer;
    public uint preCullRenderItemsLimit; 
};

public struct CSInput {
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};