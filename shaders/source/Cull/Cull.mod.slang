module Cull;

import RenderItem;

public struct CullPushConstants {
    public RenderItem* renderItemsBuffer;
    public VisibleRenderItem* visibleRenderItemsBuffer;
    public uint* countBuffer;
};

[[vk::push_constant]]
public CullPushConstants cullPushConstants;