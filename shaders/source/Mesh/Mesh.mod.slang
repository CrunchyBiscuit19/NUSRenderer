module Mesh;

import Perspective;
import Vertex;
import MaterialConstant;
import NodeTransform;
import Instance;
import RenderItem;

[[vk::binding(0,0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;
[[vk::binding(1, 0)]]
public uniform Sampler2D materialResources[];

public struct ForwardPushConstants {
    public Vertex* verticesBuffer;
    public MaterialConstant* materialConstantsBuffer;
    public NodeTransform* nodeTransformsBuffer;
    public Instance* instancesBuffer;
    public VisibleRenderItem* visibleRenderItemsBuffer;
};

[[vk::push_constant]]
public ForwardPushConstants forwardPushConstants; 