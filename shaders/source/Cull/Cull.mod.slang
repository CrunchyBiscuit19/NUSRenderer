module Cull;

import RenderItem;

public struct CullPushConstants
{
    public RenderItem *preCullRenderItemsBuffer;
    public RenderItem *postCullRenderItemsBuffer;
    public uint *postCullCountBuffer;
    public uint *totalPostCullCountBuffer;
    public uint preCullRenderItemsCount;
};

public struct CSInput
{
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};