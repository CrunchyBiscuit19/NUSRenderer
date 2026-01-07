import Composite;

[[vk::binding(0, 0)]]
public uniform Texture2D<float4> accumImage;
[[vk::binding(1, 0)]]
public uniform Texture2D<float> revealageImage;

FSOutput main(FSInput in) : SV_Target {
    FSOutput out;

    out.color = float4(1.f);

    return out;
}