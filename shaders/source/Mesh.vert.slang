import Mesh;
import Vertex;
import MaterialConstant;
import RenderItem;

[[vk::push_constant]]
ForwardPushConstants forwardPc;

VSOutput main(VSInput in)
{
    VSOutput out;

    Vertex v = forwardPc.mainVertexBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    RenderItem renderItem = forwardPc.postCullRenderItemsBuffer[in.drawIndex];

    uint firstRenderInstance = renderItem.firstRenderInstance;
    uint mainInstanceIndex = forwardPc.mainVisibleRenderInstancesInstanceIndexBuffer[in.instanceIndex + firstRenderInstance];
    float4x4 instanceTransformMatrix = forwardPc.mainInstancesBuffer[mainInstanceIndex].transformMatrix;

    uint mainNodeTransformIndex = forwardPc.postCullRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = forwardPc.mainNodeTransformsBuffer[mainNodeTransformIndex];

    uint mainMaterialIndex = forwardPc.postCullRenderItemsBuffer[in.drawIndex].materialIndex;
    MaterialConstant materialConstant = forwardPc.mainMaterialConstantsBuffer[mainMaterialIndex];

    out.position =
        mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.normal = v.normal;
    out.uv = float2(v.uv_x, v.uv_y);
    out.color = v.color * materialConstant.baseFactor;
    out.materialIndex = mainMaterialIndex;
    return out;
}