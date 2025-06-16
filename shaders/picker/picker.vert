#version 460

#extension GL_GOOGLE_include_directive : require

#include "picker_inputs.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out uint outModelId;
layout (location = 3) out uint outInstanceIndex;

void main() 
{
	uint mainVertexIndex = gl_VertexIndex;
	Vertex v = scenePushConstants.vertexBuffer.vertices[mainVertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	uint mainInstanceIndex = gl_InstanceIndex;
	InstanceData instance = scenePushConstants.instancesBuffer.instances[mainInstanceIndex];
	mat4 instanceTransformMatrix = instance.transformMatrix;

	uint mainNodeTransformIndex = scenePushConstants.visibleRenderItemsBuffer.visibleRenderItems[gl_DrawID].nodeTransformIndex;
	mat4 nodeTransformMatrix = scenePushConstants.nodeTransformsBuffer.nodeTransforms[mainNodeTransformIndex];

	gl_Position = scene.proj * scene.view * (instanceTransformMatrix * nodeTransformMatrix * position); 

	outNormal = mat3(transpose(inverse(nodeTransformMatrix))) * v.normal;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outModelId = scenePushConstants.visibleRenderItemsBuffer.visibleRenderItems[gl_DrawID].modelId;
	outInstanceIndex = mainInstanceIndex;
}