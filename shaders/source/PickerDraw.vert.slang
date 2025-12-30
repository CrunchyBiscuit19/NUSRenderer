import PickerDraw;
import Vertex;

[[vk::push_constant]]
PickerDrawPushConstants pickerDrawPc;

VSOutput main(VSInput in)
{
    VSOutput out;

    Vertex v = pickerDrawPc.verticesBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    uint firstRenderInstance = pickerDrawPc.postCullRenderItemsBuffer[in.drawIndex].firstRenderInstance;
    uint instanceIndex = in.instanceIndex + firstRenderInstance;
    float4x4 instanceTransformMatrix = pickerDrawPc.instancesBuffer[instanceIndex].transformMatrix;

    uint mainNodeTransformIndex = pickerDrawPc.postCullRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = pickerDrawPc.nodeTransformsBuffer[mainNodeTransformIndex];

    out.position =
        mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.modelIndex = pickerDrawPc.postCullRenderItemsBuffer[in.drawIndex].modelIndex;
    out.instanceIndex = instanceIndex;
    return out;
}
