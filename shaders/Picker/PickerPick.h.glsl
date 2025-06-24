#extension GL_EXT_buffer_reference : require

layout(rg32ui, set = 0, binding = 0) uniform uimage2D pickerImage;

struct PickerData
{
	ivec2 uv;
	uvec2 read;
};
layout (buffer_reference, std430) buffer PickerBuffer {   
	PickerData d;
};

layout( push_constant ) uniform PickerPickPushConstants
{
	PickerBuffer pickerBuffer;
};