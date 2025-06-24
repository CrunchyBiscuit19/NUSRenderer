#extension GL_EXT_buffer_reference : require

#include "../RenderItems.h.glsl"

layout (buffer_reference, std430) buffer CountBuffer { 
	int count;
};

layout( push_constant ) uniform CullPushConstants 
{
	RenderItemsBuffer renderItemsBuffer;
	VisibleRenderItemsBuffer visibleRenderItemsBuffer;
	CountBuffer countBuffer;
};