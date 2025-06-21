#extension GL_EXT_buffer_reference : require

#include "../Perspective.h.glsl"

layout (set = 0, binding = 0) uniform PerspectiveBuffer {   
	PerspectiveData perspective;
};
layout (set = 1, binding = 0) uniform samplerCube skyboxCubemap;

struct SkyboxVertex {
	vec4 position;
}; 
layout (buffer_reference, std430) readonly buffer SkyboxVertexBuffer { 
	SkyboxVertex vertices[];
};

layout( push_constant ) uniform SkyboxPushConstants
{
	SkyboxVertexBuffer skyboxVertexBuffer;
};