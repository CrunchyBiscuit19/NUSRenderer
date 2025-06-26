import Skybox;

struct VSOutput {
    float4 position : SV_Position;
    float3 uvw;
};

VSOutput main(uint vertexIndex: SV_VertexID) 
{
    float4 position = skyboxPushConstants.skyboxVertexBuffer[vertexIndex].position;
    float4x4 viewWithoutTranslation = float4x4(
        float4(perspectiveBuffer.view[0].xyz, 0),
        float4(perspectiveBuffer.view[1].xyz, 0),
        float4(perspectiveBuffer.view[2].xyz, 0),
        float4(0, 0, 0, 1));
    position = mul(position, viewWithoutTranslation * perspectiveBuffer.proj); 
	position = position.xyww;

    VSOutput out;
    out.position = position;
    out.uvw = position.xyz;
    return out;
} 