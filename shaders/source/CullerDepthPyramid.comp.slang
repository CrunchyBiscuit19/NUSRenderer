import Culler;

static const uint MAX_2D_WORKGROUP_THREADS = 32;

[[vk::binding(0, 0)]]
public uniform Sampler2D<float> depthImage;
[[vk::binding(1, 0)]]
public uniform Sampler2D<float> depthPyramidImagePrev;
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float> depthPyramidImageNext;

[[vk::push_constant]]
CullerDepthPyramidPushConstants cullerDepthPyramidPc;

[numthreads(MAX_2D_WORKGROUP_THREADS, MAX_2D_WORKGROUP_THREADS, 1)]
void main(CSInput in) {
    // Get the current texel position, centralize it, and normalize it
    uint2 rawPos = in.dispatchThreadID.xy;
    float2 centrePos = (float2)rawPos + float2(0.5f, 0.5f);
    uint2 normalizedCentrePos = uint2(centrePos / (float2)cullerDepthPyramidPc.levelExtent);

    // Read from depth image at level 0, else from previous depth pyramid mip level
    // The sampler for depth image uses minimum reduction mode
    // It takes the minimum depth value from a 2x2 block of texels, which is the furthest distance in reverse-z buffer
    // Record this furthest distance for occlusion culling later
    float minDepth = 0;
    if (cullerDepthPyramidPc.level == 0) {
        minDepth = depthImage.Sample(normalizedCentrePos).x;
    } else {
        minDepth = depthPyramidImagePrev.Sample(normalizedCentrePos).x;;
    }

    // Write this min depth to next depth pyramid mip level
    depthPyramidImageNext.Store(rawPos, minDepth);
}