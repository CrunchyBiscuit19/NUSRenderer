
#extension GL_EXT_buffer_reference : require

layout (set = 0, binding = 0) uniform sampler2D baseTex;
layout (set = 0, binding = 1) uniform sampler2D metalRoughTex;
layout (set = 0, binding = 2) uniform sampler2D normalTex;
layout (set = 0, binding = 3) uniform sampler2D occlusiveTex;
layout (set = 0, binding = 4) uniform sampler2D emissiveTex;
layout (set = 1, binding = 0) uniform SceneData {   
	mat4 view;
	mat4 proj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} scene;

struct Vertex {
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 
layout (buffer_reference, std430) readonly buffer VertexBuffer { 
	Vertex vertices[];
};

struct MaterialConstant {
    vec4 baseFactor;
    vec4 emissiveFactor;
    vec4 metallicRoughnessFactor;
};
layout (buffer_reference, std430) readonly buffer MaterialConstantBuffer {
	MaterialConstant materialConstants[];
};

layout( push_constant ) uniform PushConstants
{
	VertexBuffer vertexBuffer;
	MaterialConstantBuffer materialConstantsBuffer;
	mat4 worldMatrix;
    int materialIndex;
} pushConstants;