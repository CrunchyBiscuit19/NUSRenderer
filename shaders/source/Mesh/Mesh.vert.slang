import Mesh;
import Vertex;
import MaterialConstant;

VSOutput main(VSInput in, uniform ForwardPushConstants forwardPushConstants)
{
    VSOutput out;

    Vertex v = forwardPushConstants.verticesBuffer[in.vertexIndex];
    float4 position = float4(v.position, 1.0f);

    uint firstInstance = forwardPushConstants.postCullRenderItemsBuffer[in.drawIndex].firstInstance;
    float4x4 instanceTransformMatrix =
        forwardPushConstants.instancesBuffer[in.instanceIndex + firstInstance].transformMatrix;

    uint mainNodeTransformIndex = forwardPushConstants.postCullRenderItemsBuffer[in.drawIndex].nodeTransformIndex;
    float4x4 nodeTransformMatrix = forwardPushConstants.nodeTransformsBuffer[mainNodeTransformIndex];

    uint mainMaterialIndex = forwardPushConstants.postCullRenderItemsBuffer[in.drawIndex].materialIndex;
    MaterialConstant materialConstant = forwardPushConstants.materialConstantsBuffer[mainMaterialIndex];

    out.position =
        mul(mul(mul(mul(perspectiveBuffer.proj, perspectiveBuffer.view), instanceTransformMatrix), nodeTransformMatrix),
            position);
    out.normal = v.normal;
    out.uv = float2(v.uv_x, v.uv_y);
    out.color = v.color * materialConstant.baseFactor;
    out.materialIndex = mainMaterialIndex;
    return out;
}
