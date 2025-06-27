import Skybox;

float4 main(VSOutput in) : SV_TARGET
{
    return skyboxCubemap.Sample(in.uvw);
    //return float4(1, 1, 1, 0);
}
