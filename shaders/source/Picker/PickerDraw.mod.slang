module PickerDraw;

import Perspective;
import Vertex;
import NodeTransform;
import Instance;
import RenderItem;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;

public struct PickerDrawPushConstants
{
    public Vertex *verticesBuffer;
    public NodeTransform *nodeTransformsBuffer;
    public Instance *instancesBuffer;
    public VisibleRenderItem *visibleRenderItemsBuffer;
};

public struct VSInput
{
    public uint vertexIndex : SV_VertexID;
    public uint instanceIndex : SV_InstanceID;
    public uint drawIndex : SV_DrawIndex;
};

public struct VSOutput
{
    public float4 position : SV_Position;
    public nointerpolation uint modelIndex;
    public nointerpolation uint instanceIndex;
};

public typedef VSOutput FSInput;
