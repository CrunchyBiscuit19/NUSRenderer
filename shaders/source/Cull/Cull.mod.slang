module Cull;

import RenderItem;

public struct CullPushConstants
{
    public RenderItem *renderItemsBuffer;
    public VisibleRenderItem *visibleRenderItemsBuffer;
    public uint *countBuffer;
};

public struct CSInput
{
    public uint3 dispatchThreadID : SV_DispatchThreadID;
};
