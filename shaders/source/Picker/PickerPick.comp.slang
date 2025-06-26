import PickerPick;

[numthreads(1)]

void main()
{
    uint2 read = pickerImage.Load(pickerPickPushConstants.pickerBuffer[0].coords);
    pickerPickPushConstants.pickerBuffer[0].read.x = read.x;
    pickerPickPushConstants.pickerBuffer[0].read.x = read.y;
}
