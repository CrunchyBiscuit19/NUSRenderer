#extension GL_EXT_buffer_reference : require

#include "../Perspective.h.glsl"
#include "../Vertex.h.glsl"
#include "../MaterialConstants.h.glsl"
#include "../NodeTransforms.h.glsl"
#include "../Instances.h.glsl"
#include "../RenderItems.h.glsl"

layout (set = 0, binding = 0) uniform PerspectiveBuffer {   
	PerspectiveData perspective;
};
layout (set = 1, binding = 0) uniform sampler2D materialResources[];

layout( push_constant ) uniform MeshPushConstants
{
	VertexBuffer vertexBuffer;
	MaterialConstantsBuffer materialConstantsBuffer;
	NodeTransformsBuffer nodeTransformsBuffer;
	InstanceBuffer instancesBuffer;
	VisibleRenderItemsBuffer visibleRenderItemsBuffer;
};