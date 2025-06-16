#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "picker_inputs.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) flat in uint inModelId;
layout (location = 3) flat in uint inInstanceIndex;

layout (location = 0) out uvec4 outData;


void main() 
{
    outData = uvec4(inModelId, 0, inInstanceIndex, 0);
}