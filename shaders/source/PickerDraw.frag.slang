import PickerDraw;

uint2 main(FSInput in) : SV_Target
{
    return uint2(in.modelIndex + 1, in.instanceIndex + 1);
}
