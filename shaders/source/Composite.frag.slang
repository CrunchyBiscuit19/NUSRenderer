import Composite;

[[vk::binding(0, 0)]]
public uniform Texture2D<float4> accumImage;
[[vk::binding(1, 0)]]
public uniform Texture2D<float> rvlImage;

const static float EPSILON = 0.00001f;

bool isApproximatelyEqual(float a, float b) {
    return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
}

float maxOf3(float3 v) {
    return max(max(v.x, v.y), v.z);
}

FSOutput main(FSInput in) : SV_Target {
    FSOutput output;

    uint3 coords = uint3(uint2(in.position.xy), 0);

    float4 accum = accumImage.Load(coords);
    float reveal = saturate(rvlImage.Load(coords).r);

    output.color = float4(accum.rgb / max(accum.a, 1e-5), 1.f - reveal);

    return output;
}