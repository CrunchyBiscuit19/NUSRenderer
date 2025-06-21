struct MaterialConstant {
	vec4 baseFactor;
	vec4 emissiveFactor;
	vec2 metallicRoughnessFactor;
    float normalScale;
    float occlusionStrength;
};
layout (buffer_reference, std430) readonly buffer MaterialConstantsBuffer {
	MaterialConstant materialConstants[];
};