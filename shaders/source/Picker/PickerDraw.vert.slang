import PickerDraw;
import Vertex;

[[vk::push_constant]]
PickerDrawPushConstants pickerDrawPc;

VSOutput main(VSInput in)
{
    VSOutput out;

    Vertex v = pickerDrawPc.verticesBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    uint firstInstance = pickerDrawPc.renderItemsBuffer[in.drawIndex].firstInstance;
    uint instanceIndex = in.instanceIndex + firstInstance;
    float4x4 instanceTransformMatrix = pickerDrawPc.instancesBuffer[instanceIndex].transformMatrix;

    uint mainNodeTransformIndex = pickerDrawPc.renderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = pickerDrawPc.nodeTransformsBuffer[mainNodeTransformIndex];

    out.position =
        mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.modelIndex = pickerDrawPc.renderItemsBuffer[in.drawIndex].modelIndex;
    out.instanceIndex = instanceIndex;
    return out;
}
