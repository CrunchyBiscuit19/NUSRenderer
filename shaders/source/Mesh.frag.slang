import Mesh;

float4 main(FSInput in) : SV_Target
{
    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv);
    if (diffuseColor.a < 0.0001) discard;

    float4 color = in.color * diffuseColor;
    float diffuse = max(dot(normal, sunlightDir), 0.0);
    float4 sunlight = perspectiveBuffer.sunlightColor * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * perspectiveBuffer.ambientColor;

    float4 finalColor = lightColor + ambient;
    return finalColor;
}
