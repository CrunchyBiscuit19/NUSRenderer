import PickerDraw;

struct VSOutput {
    float2 uv : TEXCOORD_0;
    nointerpolation uint modelIndex;
    nointerpolation uint instanceIndex;
};

uint2 main(VSOutput in) : SV_Target {
    return uint2(in.modelIndex, in.instanceIndex);
}