#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;

void main() 
{
	Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 instanceMatrix = mat4(1.0f);

	gl_Position = scene.proj * scene.view * pushConstants.worldMatrix * position; 

	outNormal = mat3(transpose(inverse(pushConstants.worldMatrix))) * v.normal;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outColor = v.color.xyz * materialConstants.baseFactor.xyz;
}