import Mesh;

struct VSOutput {
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 color : COLOR0;
    nointerpolation uint materialIndex;
};

float4 main(VSOutput in) : SV_TARGET {
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float3 color = materialResources[in.materialIndex * 5 + 0].Sample(in.uv).rgb * in.color;
    float diffuse = max(dot(in.normal, sunlightDir), 0.0);
    float3 sunlight = sunlightPower * perspectiveBuffer.sunlightColor.rgb;

    float3 lightColor = sunlight * diffuse * color;
    float3 ambient = perspectiveBuffer.ambientColor.rgb * color;
    
    float3 finalColor = lightColor + ambient;
    return float4(finalColor, 1.0);
}