module Skybox;

import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform SamplerCube skyboxCubemap;

public struct SkyboxVertex
{
    public float4 position;
};
public struct SkyboxPushConstants
{
    public SkyboxVertex *skyboxVertexBuffer;
};

public struct VSInput
{
    public uint vertexIndex : SV_VertexID;
}
public struct VSOutput
{
    public float4 position : SV_Position;
    public float3 uvw : TEXCOORD0;
};

public typedef VSOutput FSInput;
