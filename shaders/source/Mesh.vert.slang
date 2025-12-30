import Mesh;
import Vertex;
import MaterialConstant;

[[vk::push_constant]]
ForwardPushConstants forwardPc;

VSOutput main(VSInput in)
{
    VSOutput out;

    Vertex v = forwardPc.verticesBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    uint firstRenderInstance = forwardPc.postCullRenderItemsBuffer[in.drawIndex].firstRenderInstance;
    float4x4 instanceTransformMatrix =
        forwardPc.instancesBuffer[in.instanceIndex + firstRenderInstance].transformMatrix;

    uint mainNodeTransformIndex = forwardPc.postCullRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = forwardPc.nodeTransformsBuffer[mainNodeTransformIndex];

    uint mainMaterialIndex = forwardPc.postCullRenderItemsBuffer[in.drawIndex].materialIndex;
    MaterialConstant materialConstant = forwardPc.materialConstantsBuffer[mainMaterialIndex];

    out.position =
        mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.normal = v.normal;
    out.uv = float2(v.uv_x, v.uv_y);
    out.color = v.color * materialConstant.baseFactor;
    out.materialIndex = mainMaterialIndex;
    return out;
}
