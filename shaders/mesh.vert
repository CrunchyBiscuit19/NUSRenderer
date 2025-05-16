#version 460

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;

void main() 
{
	Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);
	InstanceData instance = instanceBuffer.instances[gl_InstanceIndex];

	gl_Position = scene.proj * scene.view * (instance.transformMatrix * pushConstants.worldMatrix * position); 

	MaterialConstant materialConstant = pushConstants.materialConstantsBuffer.materialConstants[pushConstants.materialIndex];

	outNormal = mat3(transpose(inverse(pushConstants.worldMatrix))) * v.normal;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outColor = v.color.xyz * materialConstant.baseFactor.xyz;
} 