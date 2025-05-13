#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require
#extension GL_ARB_shader_draw_parameters : require

layout (set = 0, binding = 0) uniform sampler2D materialTextures[];

// Buffer device addresses
struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 
struct SceneData {   
	mat4 view;
	mat4 proj;
	vec4 ambientColor;
	vec4 sunlightDirection; // w for sun power
	vec4 sunlightColor;
}; 
struct Material {
    vec4 baseFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    vec2 padding;
};
struct Instance {
	mat4 transformation;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};
layout(buffer_reference, std430) readonly buffer SceneBuffer{ 
	SceneData sceneData;
};
layout(buffer_reference, std430) readonly buffer MaterialBuffer{ 
	Material materials;
};
layout(buffer_reference, std430) readonly buffer InstanceBuffer{ 
	Instance instances[];
};

// Push constants block
layout( push_constant, std430 ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
	SceneBuffer sceneBuffer;
	MaterialBuffer materialBuffer; 
	InstanceBuffer instanceBuffer;
} constants;