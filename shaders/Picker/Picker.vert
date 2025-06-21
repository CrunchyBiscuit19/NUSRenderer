#version 460

#extension GL_GOOGLE_include_directive : require

#include "Picker.h.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out uint outModelIndex;
layout (location = 3) out uint outInstanceIndex;

void main() 
{
	Vertex v = vertexBuffer.d[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 instanceTransformMatrix = instancesBuffer.d[gl_InstanceIndex].transformMatrix;

	uint mainNodeTransformIndex = visibleRenderItemsBuffer.d[gl_DrawID].nodeTransformIndex;
	mat4 nodeTransformMatrix = nodeTransformsBuffer.d[mainNodeTransformIndex];

	gl_Position = perspective.proj * perspective.view * (instanceTransformMatrix * nodeTransformMatrix * position); 

	outNormal = mat3(transpose(inverse(nodeTransformMatrix))) * v.normal;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outModelIndex = visibleRenderItemsBuffer.d[gl_DrawID].modelIndex;
	outInstanceIndex = gl_InstanceIndex;
}