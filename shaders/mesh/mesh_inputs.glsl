#extension GL_EXT_buffer_reference : require

layout (set = 0, binding = 0) uniform SceneData {   
	mat4 view;
	mat4 proj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} scene;
layout (set = 1, binding = 0) uniform sampler2D materialResources[];

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
layout (buffer_reference, std430) readonly buffer MaterialConstantsBuffer {
	MaterialConstant materialConstants[];
};

layout (buffer_reference, std430) readonly buffer NodeTransformsBuffer {
	mat4 nodeTransforms[];
};

struct InstanceData {
	mat4 transformMatrix;
};
layout (buffer_reference, std430) readonly buffer InstanceBuffer {   
	InstanceData instances[];
};

struct VisibleRenderItem {
	int indexCount;
	int instanceCount;
	int firstIndex;
	int vertexOffset;
	int firstInstance;
	int materialIndex;
	int nodeTransformIndex;
};
layout (buffer_reference, std430) buffer VisibleRenderItemsBuffer { 
	VisibleRenderItem visibleRenderItems[];
};

layout( push_constant ) uniform ScenePushConstants
{
	VertexBuffer vertexBuffer;
	MaterialConstantsBuffer materialConstantsBuffer;
	NodeTransformsBuffer nodeTransformsBuffer;
	InstanceBuffer instancesBuffer;
	VisibleRenderItemsBuffer visibleRenderItemsBuffer;
} scenePushConstants;