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
    if (diffuseColor.a < 0.0001f)
        discard;

    float4 color = in.color * diffuseColor;
    float diffuse = max(dot(normal, sunlightDir), 0.f);
    float4 sunlight = perspectiveBuffer.sunlightColor * sunlightPower;

    float4 lightColor = color * diffuse * sunlight;
    float4 ambient = color * perspectiveBuffer.ambientColor;

    float4 finalColor = lightColor + ambient;

    float4 opaque = finalColor;

    float4 modulate;
    finalColor.rgb *= finalColor.a;
    float preModifiedAlpha = finalColor.a;
    modulate = preModifiedAlpha * (float4(1.f) - opaque);

    finalColor.a *= 1.f - (finalColor.r + finalColor.g + finalColor.b) * (1.f / 3.f);
    float alphaScaled = (finalColor.a * 8.f + 0.01f) * (-in.position.z * 0.95f + 1.f);
    float luminance = dot(pow(finalColor.rgb, 2.22f), float3(0.2126, 0.7152, 0.0722));
    float weight = clamp(alphaScaled * alphaScaled * alphaScaled * 1e3, 1e-2, 3e2);
    float4 accum = finalColor * weight;
    float rvl = finalColor.a;

    return FSOutput(
        opaque,
        modulate, 
        accum, 
        rvl
    );
}

/*
float4 modulate;
float preModifiedAlpha = finalColor.a;
float4 modulatedFinalColor = finalColor * finalColor.a;
modulate = preModifiedAlpha * (float4(1.f) - modulatedFinalColor);

finalColor.a *= 1.f - (finalColor.r + finalColor.g + finalColor.b) * (1.f / 3.f);
float alphaScaled = (finalColor.a * 8.f + 0.01f) * (-in.position.z * 0.95f + 1.f);
float luminance = dot(pow(finalColor.rgb, 2.22f), float3(0.2126, 0.7152, 0.0722));
float weight = clamp(alphaScaled * alphaScaled * alphaScaled * 1e3 * luminance * 4.f, 1e-2, 3e2);
float4 accum = finalColor * weight;
float rvl = finalColor.a;

return FSOutput(
    finalColor,
    modulate, 
    accum, 
    rvl
);
*/

/*
float preModifiedAlpha = finalColor.a;
finalColor.rgb *= finalColor.a;
finalColor.a *= 1.f - (finalColor.r + finalColor.g + finalColor.b) * (1.f / 3.f);
float tmp = (finalColor.a * 8.f + 0.01f) * (-in.position.z * 0.95f + 1.f);
float luminance = dot(pow(finalColor.rgb, 2.22f), float3(0.2126, 0.7152, 0.0722));
float weight = clamp(tmp * tmp * tmp * 1e3 * luminance * 4.f, 1e-2, 3e2);

return FSOutput(
    finalColor,
    preModifiedAlpha * (float4(1.f) - finalColor),
    finalColor * weight,
    finalColor.a
);
*/