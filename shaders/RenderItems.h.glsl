struct RenderItem {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;
	uint materialIndex;
	uint nodeTransformIndex;
	//uint boundsIndex;
	uint modelIndex;
};
layout (buffer_reference, std430) buffer RenderItemsBuffer { 
	RenderItem renderItems[];
};

struct VisibleRenderItem {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;
	uint materialIndex;
	uint nodeTransformIndex;
	uint modelIndex;
};
layout (buffer_reference, std430) buffer VisibleRenderItemsBuffer { 
	VisibleRenderItem visibleRenderItems[];
};