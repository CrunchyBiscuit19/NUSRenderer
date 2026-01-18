import Geometry;
import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D materialResources[];

FSOutputTransparent main(FSInput in) : SV_Target {
    FSOutputTransparent output;

    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv);
    if (diffuseColor.a < 0.0001f)
        discard;

    float4 color = in.color * diffuseColor;
    float finalAlpha = color.a;
    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float4 sunlight = float4(perspectiveBuffer.sunlightColor, 1.f) * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * float4(perspectiveBuffer.ambientColor, 1.f);

    float4 finalColor = lightColor + ambient;
    finalColor.a = finalAlpha;

    float weight = clamp(pow(min(1.f, finalColor.a * 10.f) + 0.01f, 3.f) * 1e8 * pow(1.f - (1.f - in.position.z) * 0.9f, 3.f), 1e-2, 3e3);
    output.accum = float4(finalColor.rgb * finalColor.a, finalColor.a) * weight;
    output.rvl = finalColor.a;

    return output;
}
