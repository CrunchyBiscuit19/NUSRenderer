module RenderItem;

public struct RenderInstance {
    public uint renderItemIndex;
    public uint instanceIndex;
    public uint instanceCount;
    public uint firstInstance;
    public uint nodeTransformIndex;
    public uint boundsIndex;
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
    public uint _pad0;
    public uint _pad1;
    public uint _pad2;
};
