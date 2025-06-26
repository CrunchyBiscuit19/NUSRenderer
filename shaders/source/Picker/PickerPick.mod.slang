module PickerPick;

[[vk::binding(0, 0)]]
public uniform RWTexture2D<uint2> pickerImage;

public struct PickerData {
    public int2 coords;
    public uint2 read;
};

public struct PickerPickPushConstants {
    public PickerData* pickerBuffer;
};

[[vk::push_constant]]
public PickerPickPushConstants pickerPickPushConstants;