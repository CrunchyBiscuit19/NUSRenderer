module Mesh;

import Perspective;
import Vertex;
import MaterialConstant;
import NodeTransform;
import Instance;
import RenderItem;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D materialResources[];

public struct ForwardPushConstants
{
    public Vertex* verticesBuffer;
    public MaterialConstant* materialConstantsBuffer;
    public NodeTransform* nodeTransformsBuffer;
    public Instance* instancesBuffer;
    public VisibleRenderItem* visibleRenderItemsBuffer;
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
    public float3 normal : NORMAL;
    public float2 uv : TEXCOORD0;
    public float3 color : COLOR0;
    public nointerpolation uint materialIndex;
};