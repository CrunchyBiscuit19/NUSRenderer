#extension GL_EXT_buffer_reference : require

struct RenderItem {
	int indexCount;
	int instanceCount;
	int firstIndex;
	int vertexOffset;
	int firstInstance;
	int materialIndex;
	int nodeTransformIndex;
	//int boundsIndex;
};
layout (buffer_reference, std430) buffer RenderItemsBuffer { 
	RenderItem renderItems[];
};

layout (buffer_reference, std430) buffer CountBuffer { 
	int count;
};

layout( push_constant ) uniform CullPushConstants 
{
	RenderItemsBuffer renderItemsBuffer;
	RenderItemsBuffer visibleRenderItemsBuffer;
	CountBuffer countBuffer;
} cullPushConstants;