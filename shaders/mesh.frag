#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	float lightValue = max(dot(inNormal, scene.sunlightDirection.xyz), 0.1f);

	vec3 color = inColor * texture(baseTex,inUV).xyz;
	vec3 ambient = color *  scene.ambientColor.xyz;

	outFragColor = vec4(color * lightValue *  scene.sunlightColor.w + ambient , 1.0f);
}
