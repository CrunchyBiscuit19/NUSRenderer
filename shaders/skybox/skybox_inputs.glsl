#extension GL_EXT_buffer_reference : require

// Sets of UBOs and Textures
layout (set = 0, binding = 0) uniform SceneData {   
	mat4 view;
	mat4 proj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} scene;
layout (set = 1, binding = 0) uniform samplerCube skyboxCubemap; 

// SSBO addresses and buffer definitions
struct Vertex {
	vec4 position;
}; 
layout (buffer_reference, std430) readonly buffer VertexBuffer { 
	Vertex vertices[];
};

layout( push_constant ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
} pushConstants;