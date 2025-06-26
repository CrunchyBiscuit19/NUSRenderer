import Skybox;

struct VSOutput {
    float3 uvw;
};

float4 main(VSOutput in) : SV_TARGET {
    return skyboxCubemap.Sample(in.uvw);
}
