module Cull;

import RenderItem;

public struct CullPushConstants
{
    public RenderItem *preCullRenderItemsBuffer;
    public RenderItem *postCullRenderItemsBuffer;
    public uint *countBuffer;
};

public struct CSInput
{
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};