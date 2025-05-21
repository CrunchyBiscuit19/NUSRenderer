#version 460

#extension GL_GOOGLE_include_directive : require

#include "skybox_inputs.glsl"

layout (location = 0) out vec3 outUVW;

void main() 
{
	Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = v.position;
	gl_Position = scene.proj * scene.view * position; 

	outUVW = v.position.xyz;
} 