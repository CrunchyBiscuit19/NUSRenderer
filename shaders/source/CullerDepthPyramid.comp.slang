import Culler;

static const uint MAX_2D_WORKGROUP_THREADS = 32;

[[vk::binding(0, 0)]]
public uniform Sampler2DMS<float> depthImage;
[[vk::binding(1, 0)]]
public uniform Sampler2D<float> depthPyramidRead[];
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float> depthPyramidWrite[];
[[vk::binding(3, 0)]]
public uniform SamplerState minReductionSampler;

[[vk::push_constant]]
CullerDepthPyramidPushConstants cullerDepthPyramidPc;

[numthreads(MAX_2D_WORKGROUP_THREADS, MAX_2D_WORKGROUP_THREADS, 1)]
void main(CSInput in) {
    // Get the current texel position, centralize it, and normalize it
    uint2 rawPos = in.dispatchThreadID.xy;
    float2 centrePos = (float2)rawPos + float2(0.5f, 0.5f);
    uint2 normalizedCentrePos = uint2(centrePos / (float2)cullerDepthPyramidPc.levelExtent);

    // Read from previous depth pyramid mip level (depth image stands in at mip level 0)
    // The sampler for depth image uses minimum reduction mode
    // It takes the minimum depth value from a 2x2 block of texels, which is the furthest distance in reverse-z buffer
    // Record this furthest distance for occlusion culling later
    float minDepth = 0.f;
    if (cullerDepthPyramidPc.level == 0) {
        minDepth = depthImage.Sample(normalizedCentrePos).x;
    } else {
        minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].Sample(normalizedCentrePos).x;
    }

    // Write this min depth to next depth pyramid mip level
    depthPyramidWrite[cullerDepthPyramidPc.level].Store(rawPos, minDepth);
}