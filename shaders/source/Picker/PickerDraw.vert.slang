import PickerDraw;
import Vertex;

struct VSOutput {
    float4 position : SV_Position;
    nointerpolation uint modelIndex;
    nointerpolation uint instanceIndex;
};

VSOutput main(uint vertexIndex: SV_VertexID, uint instanceIndex: SV_InstanceID, uint drawIndex: SV_DrawIndex) 
{
	Vertex v = pickerDrawPushConstants.verticesBuffer[vertexIndex];
	float4 position = float4(v.position, 1.0f);

    float4x4 instanceTransformMatrix = pickerDrawPushConstants.instancesBuffer[instanceIndex].transformMatrix;

    uint mainNodeTransformIndex = pickerDrawPushConstants.visibleRenderItemsBuffer[drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = pickerDrawPushConstants.nodeTransformsBuffer[mainNodeTransformIndex];

    position = mul(mul(position, nodeTransformMatrix * instanceTransformMatrix), perspectiveBuffer.view * perspectiveBuffer.proj);

    VSOutput out;
    out.position = position;
    out.modelIndex = pickerDrawPushConstants.visibleRenderItemsBuffer[drawIndex].modelIndex;
    out.instanceIndex = instanceIndex;
    return out;
}