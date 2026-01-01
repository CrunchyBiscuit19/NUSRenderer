import Culler;

static const uint MAX_CULL_LOCAL_SIZE = 256;

[[vk::binding(0, 0)]]
public uniform RWTexture2D<uint> depthImage;
[[vk::binding(1, 0)]]
public uniform RWTexture2D<uint> depthPyramidImagePrev;
[[vk::binding(2, 0)]]
public uniform RWTexture2D<uint> depthPyramidImageNext;

[[vk::push_constant]]
CullerDepthPyramidPushConstants cullerDepthPyramidPc;

[numthreads(MAX_CULL_LOCAL_SIZE, 1, 1)]
void main(CSInput in) {

}