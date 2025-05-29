#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "mesh_inputs.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;


void main() 
{
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(scene.sunlightDirection.xyz);
    float sunlightPower = scene.sunlightDirection.w;

    vec3 color = inColor * texture(baseTex, inUV).rgb;
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 sunlight = scene.sunlightColor.rgb * sunlightPower;

    vec3 lightColor = color * diffuse * sunlight;
    vec3 ambient = color * scene.ambientColor.rgb;
    
    vec3 finalColor = lightColor + ambient;
    outFragColor = vec4(finalColor, 1.0);
}