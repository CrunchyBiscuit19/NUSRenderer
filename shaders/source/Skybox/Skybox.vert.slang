import Skybox;

VSOutput main(VSInput in, uniform SkyboxPushConstants skyboxPushConstants)
{
    VSOutput out;

    float4 position = skyboxPushConstants.skyboxVertexBuffer[in.vertexIndex].position;
    float4x4 viewWithoutTranslation =
        float4x4(float4(perspectiveBuffer.view[0].xyz, 0), float4(perspectiveBuffer.view[1].xyz, 0),
                 float4(perspectiveBuffer.view[2].xyz, 0), float4(0, 0, 0, 1));

    out.position = mul(perspectiveBuffer.proj, mul(viewWithoutTranslation, position));
    out.position = float4(out.position.xy, out.position.ww);
    out.uvw = position.xyz;
    return out;
} 