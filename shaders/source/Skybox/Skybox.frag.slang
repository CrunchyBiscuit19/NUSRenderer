import Skybox;

float4 main(VSOutput in) : SV_TARGET
{
    return skyboxCubemap.Sample(in.uvw);
}
