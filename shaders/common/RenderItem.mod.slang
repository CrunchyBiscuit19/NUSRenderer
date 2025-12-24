module RenderItem;

public struct RenderInstance {
    public uint renderItemIndex;
    public uint instanceIndex;
}

public struct RenderItem
{
    public uint indexCount;
    public uint instanceCount;
    public uint firstIndex;
    public uint vertexOffset;
    public uint firstInstance;
    public uint materialIndex;
    public uint nodeTransformIndex;
    public uint modelIndex;
    public uint boundsIndex;
};
