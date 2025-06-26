import Mesh;
import Vertex;
import MaterialConstant;

struct VSOutput {
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 color : COLOR0;
    nointerpolation uint materialIndex;
};

VSOutput main(uint vertexIndex: SV_VertexID, uint instanceIndex: SV_InstanceID, uint drawIndex: SV_DrawIndex)
{
    Vertex v = forwardPushConstants.verticesBuffer[vertexIndex];
    float4 position = float4(v.position, 1.0f);

    float4x4 instanceTransformMatrix = forwardPushConstants.instancesBuffer[instanceIndex].transformMatrix;

	uint mainNodeTransformIndex = forwardPushConstants.visibleRenderItemsBuffer[drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = forwardPushConstants.nodeTransformsBuffer[mainNodeTransformIndex];

    position = mul(mul(position, nodeTransformMatrix * instanceTransformMatrix), perspectiveBuffer.view * perspectiveBuffer.proj);

    uint mainMaterialIndex = forwardPushConstants.visibleRenderItemsBuffer[drawIndex].materialIndex;
    MaterialConstant materialConstant = forwardPushConstants.materialConstantsBuffer[mainMaterialIndex];

    VSOutput out;
    out.position = position;
    out.normal = v.normal;
    out.uv = float2(v.uv_x, v.uv_y);
    out.color = materialConstant.baseFactor.xyz * v.color.xyz;
    out.materialIndex = mainMaterialIndex;
    return out;
}