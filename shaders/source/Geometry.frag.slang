import Geometry;
import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D materialResources[];

FSOutput main(FSInput in) : SV_Target {
    FSOutput output;
    
    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv);
    if (diffuseColor.a < 0.0001)
        discard;
    float4 color = in.color * diffuseColor;
    float finalAlpha = color.a;

    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float diffuse = max(dot(normal, sunlightDir), 0.0);
    float4 sunlight = perspectiveBuffer.sunlightColor * sunlightPower;


    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * perspectiveBuffer.ambientColor;

    float4 finalColor = lightColor + ambient;
    finalColor.a = finalAlpha;

    float weight = finalColor.a * clamp(0.03 / (1e-5 + pow((1.f - in.position.z) / 200.0, 4.0)), 1e-2, 3e3);
    output.color = finalColor;
    output.accum = float4(finalColor.rgb * finalColor.a, finalColor.a) * weight;
    output.rvl = finalColor.a;

    return output;
}
