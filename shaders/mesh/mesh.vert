#version 460

#extension GL_GOOGLE_include_directive : require

#include "Mesh.h.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out uint outMaterialIndex;

void main() 
{
	Vertex v = vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	mat4 instanceTransformMatrix = instancesBuffer.instances[gl_InstanceIndex].transformMatrix;

	uint mainNodeTransformIndex = visibleRenderItemsBuffer.visibleRenderItems[gl_DrawID].nodeTransformIndex;
	mat4 nodeTransformMatrix = nodeTransformsBuffer.nodeTransforms[mainNodeTransformIndex];

	gl_Position = perspective.proj * perspective.view * (instanceTransformMatrix * nodeTransformMatrix * position); 

	uint mainMaterialIndex = visibleRenderItemsBuffer.visibleRenderItems[gl_DrawID].materialIndex;
	MaterialConstant materialConstant = materialConstantsBuffer.materialConstants[mainMaterialIndex];

	outNormal = mat3(transpose(inverse(nodeTransformMatrix))) * v.normal;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outColor = v.color.xyz * materialConstant.baseFactor.xyz;
	outMaterialIndex = mainMaterialIndex;
}