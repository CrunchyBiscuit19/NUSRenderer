#extension GL_EXT_buffer_reference : require

struct RenderItem {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;
	uint materialIndex;
	uint nodeTransformIndex;
	//uint boundsIndex;
	uint modelId;
};
layout (buffer_reference, std430) buffer RenderItemsBuffer { 
	RenderItem renderItems[];
};

struct visibleRenderItem {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;
	uint materialIndex;
	uint nodeTransformIndex;
	uint modelId;
};
layout (buffer_reference, std430) buffer VisibleRenderItemsBuffer { 
	visibleRenderItem vRenderItems[];
};

layout (buffer_reference, std430) buffer CountBuffer { 
	int count;
};

layout( push_constant ) uniform CullPushConstants 
{
	RenderItemsBuffer renderItemsBuffer;
	VisibleRenderItemsBuffer vRenderItemsBuffer;
	CountBuffer countBuffer;
} cullPushConstants;