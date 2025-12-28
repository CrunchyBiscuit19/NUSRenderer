import Skybox;

[[vk::push_constant]]
SkyboxPushConstants skyboxPc;

VSOutput main(VSInput in)
{
    VSOutput out;

    float4 position = skyboxPc.skyboxVertexBuffer[in.vertexIndex].position;
    float4x4 viewWithoutTranslation =
        float4x4(float4(perspectiveBuffer.view[0].xyz, 0), float4(perspectiveBuffer.view[1].xyz, 0),
                 float4(perspectiveBuffer.view[2].xyz, 0), float4(0, 0, 0, 1));

    out.position = mul(mul(perspectiveBuffer.proj, viewWithoutTranslation), position);
    out.uvw = position.xyz;
    return out;
}
