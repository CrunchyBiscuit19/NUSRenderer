module RenderItem;

public struct VisibleRenderItem
{
    public uint indexCount;
    public uint instanceCount;
    public uint firstIndex;
    public uint vertexOffset;
    public uint firstInstance;
    public uint materialIndex;
    public uint nodeTransformIndex;
    public uint modelIndex;
};

public struct RenderItem
{
    public uint indexCount;
    public uint instanceCount;
    public uint firstIndex;
    public uint vertexOffset;
    public uint firstInstance;
    public uint materialIndex;
    public uint nodeTransformIndex;
    // public uint boundsIndex;
    public uint modelIndex;
};
