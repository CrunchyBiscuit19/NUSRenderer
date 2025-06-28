import PickerDraw;
import Vertex;

VSOutput main(VSInput in, uniform PickerDrawPushConstants pickerDrawPushConstants)
{
    VSOutput out;

    Vertex v = pickerDrawPushConstants.verticesBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    uint firstInstance = pickerDrawPushConstants.visibleRenderItemsBuffer[in.drawIndex].firstInstance;
    uint instanceIndex = in.instanceIndex + firstInstance;
    float4x4 instanceTransformMatrix = pickerDrawPushConstants.instancesBuffer[instanceIndex].transformMatrix;

    uint mainNodeTransformIndex = pickerDrawPushConstants.visibleRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = pickerDrawPushConstants.nodeTransformsBuffer[mainNodeTransformIndex];

    out.position =
        mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.modelIndex = pickerDrawPushConstants.visibleRenderItemsBuffer[in.drawIndex].modelIndex;
    out.instanceIndex = instanceIndex;
    return out;
}
