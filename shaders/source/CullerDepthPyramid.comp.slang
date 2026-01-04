import Culler;

static const uint MAX_2D_WORKGROUP_THREADS = 32;

[[vk::binding(0, 0)]]
public uniform Sampler2D<float> resolvedDepthImage;
[[vk::binding(1, 0)]]
public uniform Sampler2D<float> depthPyramidRead[];
[[vk::binding(2, 0)]]
public uniform RWTexture2D<float> depthPyramidWrite[];

[[vk::push_constant]]
CullerDepthPyramidPushConstants cullerDepthPyramidPc;

[numthreads(MAX_2D_WORKGROUP_THREADS, MAX_2D_WORKGROUP_THREADS, 1)]
void main(CSInput in) {
    // Calculate final writing position
    uint2 readLevelPos = in.dispatchThreadID.xy;
    uint2 writeLevelPos = uint2(
        (uint)(cullerDepthPyramidPc.writeLevelExtentRatio.x * (float)readLevelPos.x),
        (uint)(cullerDepthPyramidPc.writeLevelExtentRatio.y * (float)readLevelPos.y)
    );

    // Calculate texel positions surrounding the current read position
    uint3 topLeft = uint3(readLevelPos, 0) + uint3(-1, 1, 0);
    uint3 topCenter = uint3(readLevelPos, 0) + uint3(0, 1, 0);
    uint3 topRight = uint3(readLevelPos, 0) + uint3(1, 1, 0);
    uint3 midLeft = uint3(readLevelPos, 0) + uint3(1, 0, 0);
    uint3 midCentre = uint3(readLevelPos, 0) + uint3(0, 0, 0);
    uint3 midRight = uint3(readLevelPos, 0) + uint3(-1, 0, 0);
    uint3 bottomLeft = uint3(readLevelPos, 0) + uint3(1, -1, 0);
    uint3 bottomCenter = uint3(readLevelPos, 0) + uint3(0, -1, 0);
    uint3 bottomRight = uint3(readLevelPos, 0) + uint3(-1, -1, 0);

    // Read Image
    Sampler2D<float> readImage = (cullerDepthPyramidPc.level == 0) ? resolvedDepthImage : depthPyramidRead[cullerDepthPyramidPc.level - 1];

    // Read Level Extent is POT
    float minDepth = min(min(resolvedDepthImage.Load(topLeft), resolvedDepthImage.Load(topCenter)), min(resolvedDepthImage.Load(midLeft), resolvedDepthImage.Load(midCentre)));

    // Read Level Extent has odd width (read extra column)
    if (cullerDepthPyramidPc.readLevelExtent.x % 2 != 0) {
        minDepth = min(minDepth, min(resolvedDepthImage.Load(topRight), resolvedDepthImage.Load(midRight)));
    }
    // Read Level Extent has odd height (read extra row)
    if (cullerDepthPyramidPc.readLevelExtent.y % 2 != 0) {
        minDepth = min(minDepth, min(resolvedDepthImage.Load(bottomLeft), resolvedDepthImage.Load(bottomCenter)));
    }
    // Read Level Extent has odd width and odd height (read extra corner)
    if (cullerDepthPyramidPc.readLevelExtent.x % 2 != 0 && cullerDepthPyramidPc.readLevelExtent.y % 2 != 0) {
        minDepth = min(minDepth, resolvedDepthImage.Load(bottomRight));
    }

    // Write into next level
    if (cullerDepthPyramidPc.level == 0) {
        depthPyramidWrite[cullerDepthPyramidPc.level].Store(writeLevelPos, minDepth * 10);
    } else {
        depthPyramidWrite[cullerDepthPyramidPc.level].Store(writeLevelPos, minDepth * 10);
    }
}

// Sample level use non-normalized coordinates
// Write level use non-normalized coordinates
// Load seems to work fine but sampling doesn't

// Read from previous depth pyramid mip level (read from depth image instead if at mip level 0)
// The sampler for depth image uses minimum reduction mode
// It takes the minimum depth value from a 2x2 block of texels, which is the furthest distance in reverse-z buffer
// Record this furthest distance for occlusion culling later

/*
void main(CSInput in) {
    // Get the current texel position, centralize it, and normalize it
    uint2 rawPos = in.dispatchThreadID.xy;
    float2 centrePos = (float2)rawPos + float2(0.5f, 0.5f);
    uint2 writeLevelPos = uint2(
        (uint)(cullerDepthPyramidPc.writeLevelExtentRatio.x * (float)centrePos.x),
        (uint)(cullerDepthPyramidPc.writeLevelExtentRatio.y * (float)centrePos.y)
    );

    
    float minDepth = 0;
    if (cullerDepthPyramidPc.level == 0) {
        // minDepth = resolvedDepthImage.GatherRed(centrePos).x;
        minDepth = resolvedDepthImage.Load(int3(rawPos, 0)).x;
        // minDepth = resolvedDepthImage.Sample(rawPos).x;
    } else {
        // minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].GatherRed(centrePos).x;
        minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].Load(int3(rawPos, 0)).x;
        // minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].Sample(rawPos).x;
    }
    depthPyramidWrite[cullerDepthPyramidPc.level].Store(writeLevelPos, minDepth);
}
*/

/*
if (cullerDepthPyramidPc.level == 0) {
        // minDepth = resolvedDepthImage.GatherRed(centrePos).x;
        minDepth = resolvedDepthImage.Load(int3(rawPos, 0)).x;
        // minDepth = resolvedDepthImage.Sample(rawPos).x;
} else {
    // minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].GatherRed(centrePos).x;
    minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].Load(int3(rawPos, 0)).x;
    // minDepth = depthPyramidRead[cullerDepthPyramidPc.level - 1].Sample(rawPos).x;
}
*/

/*
    uint3 topLeft = uint3(readLevelPos, 0) + uint3(0, 0, 0);
    uint3 topCenter = uint3(readLevelPos, 0) + uint3(1, 0, 0);
    uint3 topRight = uint3(readLevelPos, 0) + uint3(2, 0, 0);
    uint3 midLeft = uint3(readLevelPos, 0) + uint3(0, 1, 0);
    uint3 midCentre = uint3(readLevelPos, 0) + uint3(1, 1, 0);
    uint3 midRight = uint3(readLevelPos, 0) + uint3(2, 1, 0);
    uint3 bottomLeft = uint3(readLevelPos, 0) + uint3(0, 2, 0);
    uint3 bottomCenter = uint3(readLevelPos, 0) + uint3(1, 2, 0);
    uint3 bottomRight = uint3(readLevelPos, 0) + uint3(2, 2, 0);
*/
