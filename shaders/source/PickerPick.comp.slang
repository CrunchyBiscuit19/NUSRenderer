import PickerPick;

[[vk::push_constant]]
PickerPickPushConstants pickerPickPc;

[numthreads(1)]
void main(CSInput in) {
    uint2 read = pickerImage.Load(pickerPickPc.pickerBuffer->coords);
    pickerPickPc.pickerBuffer->read = uint2(read.x - 1, read.y - 1);
}
