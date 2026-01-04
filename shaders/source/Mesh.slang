module Mesh;

import Perspective;
import Vertex;
import MaterialConstant;
import NodeTransform;
import Instance;
import RenderItem;

public struct ForwardPushConstants
{
    public Vertex *mainVertexBuffer;
    public MaterialConstant *mainMaterialConstantsBuffer;
    public NodeTransform *mainNodeTransformsBuffer;
    public Instance *mainInstancesBuffer;
    public uint* mainVisibleRenderInstancesInstanceIndexBuffer;
    public RenderItem* postCullRenderItemsBuffer;
};

public struct VSInput
{
    public uint vertexIndex : SV_VulkanVertexID;
    public uint instanceIndex : SV_VulkanInstanceID;
    public uint drawIndex : SV_DrawIndex;
};

public struct VSOutput
{
    public float4 position : SV_Position;
    public float3 normal : NORMAL;
    public float2 uv : TEXCOORD0;
    public float4 color : COLOR0;
    public nointerpolation uint materialIndex;
};

public typedef VSOutput FSInput;
