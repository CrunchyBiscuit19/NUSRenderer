import PickerPick;

[numthreads(1)]

void main(uniform PickerPickPushConstants pickerPickPushConstants)
{
    uint2 read = pickerImage.Load(pickerPickPushConstants.pickerBuffer[0].coords);
    pickerPickPushConstants.pickerBuffer[0].read.x = read.x - 1;
    pickerPickPushConstants.pickerBuffer[0].read.y = read.y - 1;
}
