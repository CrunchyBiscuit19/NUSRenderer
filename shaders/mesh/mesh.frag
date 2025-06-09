#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "mesh_inputs.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) flat in uint inMaterialIndex;

layout (location = 0) out vec4 outFragColor;


void main() 
{
    vec3 normal = normalize(inNormal);
    vec3 sunlightDir = normalize(scene.sunlightDirection.xyz);
    float sunlightPower = scene.sunlightDirection.w;

    uint mainMaterialIndex = inMaterialIndex;

    vec3 color = inColor * texture(materialResources[mainMaterialIndex * 5 + 0], inUV).rgb;
    float diffuse = max(dot(normal, sunlightDir), 0.0);
    vec3 sunlight = scene.sunlightColor.rgb * sunlightPower;

    vec3 lightColor = color * diffuse * sunlight;
    vec3 ambient = color * scene.ambientColor.rgb;
    
    vec3 finalColor = color * diffuse;
    outFragColor = vec4(finalColor, 1.0);
}