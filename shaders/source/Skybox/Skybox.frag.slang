import Skybox;

float4 main(FSInput in) : SV_Target
{
    return skyboxCubemap.Sample(in.uvw);
}
