#version 460

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec4 outColor;

void main() 
{
	Vertex v = constants.vertexBuffer.vertices[gl_VertexIndex];
	vec4 position = vec4(v.position, 1.0f);

	//mat4 instanceMatrix = constants.instanceBuffer.instances[gl_InstanceIndex].transformation;
	mat4 instanceMatrix = mat4(1.0f);
	mat4 transformationMatrix = mat4(1.0f);
	// [TODO] Instancing support, and feed transformation matrix in

	mat4 proj = constants.sceneBuffer.sceneData.proj; 
	mat4 view = constants.sceneBuffer.sceneData.view; 

	vec4 baseFactor = constants.materialBuffer.materials.baseFactor;

	gl_Position =  proj * view * (instanceMatrix * transformationMatrix) * position; 

	outColor = v.color * baseFactor;
	outNormal = normalize(v.normal);
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}