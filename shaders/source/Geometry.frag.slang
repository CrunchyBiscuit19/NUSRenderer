import Geometry;
import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(0, 1)]]
public uniform Sampler2D materialResources[];

FSOutput main(FSInput in) : SV_Target {
    float3 normal = normalize(in.normal);
    float3 sunlightDir = normalize(perspectiveBuffer.sunlightDirection.xyz);
    float sunlightPower = perspectiveBuffer.sunlightDirection.w;

    float4 diffuseColor = materialResources[NonUniformResourceIndex(in.materialIndex * 5 + 0)].Sample(in.uv);
    if (diffuseColor.a < 0.0001)
        discard;

    float4 color = in.color * diffuseColor;
    float diffuse = max(dot(normal, sunlightDir), 0.0);
    float4 sunlight = perspectiveBuffer.sunlightColor * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * perspectiveBuffer.ambientColor;

    float4 finalColor = lightColor + ambient;

    //float weight = pow(color.a, 2);
    float weight = max(min(1.0, max(max(color.r, color.g), color.b) * color.a), color.a) * clamp(0.03 / (1e-5 + pow(in.position.z / 200, 4.0)), 1e-2, 3e3);

    return FSOutput(finalColor, float4(finalColor.rgb * finalColor.a, finalColor.a) * weight, finalColor.a);
}
