#version 460

#extension GL_GOOGLE_include_directive : require

#include "skybox.h.glsl"

layout (location = 0) out vec3 outUVW;

void main() 
{
	vec4 position = skyboxVertexBuffer.d[gl_VertexIndex].position;
	gl_Position = perspective.proj * mat4(mat3(perspective.view)) * position; 
	gl_Position = gl_Position.xyww;

	outUVW = position.xyz;
} 