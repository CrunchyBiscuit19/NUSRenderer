#version 460

#extension GL_GOOGLE_include_directive : require

#include "Picker.h.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) flat in uint inModelIndex;
layout (location = 3) flat in uint inInstanceIndex;

layout (location = 0) out uvec2 outData;


void main() 
{
    outData = uvec2(inModelIndex + 100, inInstanceIndex + 100);
}