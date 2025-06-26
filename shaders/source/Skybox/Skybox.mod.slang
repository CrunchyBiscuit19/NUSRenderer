module Skybox;

import Perspective;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(1, 0)]]
public uniform SamplerCube skyboxCubemap;

public struct SkyboxVertex {
	public float4 position;
};

public struct SkyboxPushConstants {
    public SkyboxVertex* skyboxVertexBuffer;
};

[[vk::push_constant]]
public SkyboxPushConstants skyboxPushConstants;