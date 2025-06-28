import Mesh;

float4 main(VSOutput in) : SV_TARGET
{
    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;
    
    float3 color = in.color * materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv).rgb;
    float diffuse = max(dot(normal, sunlightDir), 0.0);
    float3 sunlight = perspectiveBuffer.sunlightColor.rgb * sunlightPower;
    
    float3 lightColor = color * diffuse * sunlight;
    float3 ambient = color * perspectiveBuffer.ambientColor.rgb;
    
    float3 finalColor = lightColor + ambient;
    return float4(finalColor, 1.0);
}