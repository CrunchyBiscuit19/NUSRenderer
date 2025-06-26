module PickerDraw;

import Perspective;
import Vertex;
import NodeTransform;
import Instance;
import RenderItem;

[[vk::binding(0, 0)]]
public uniform ConstantBuffer<Perspective> perspectiveBuffer;

public struct PickerDrawPushConstants {
    public Vertex* verticesBuffer;
    public NodeTransform* nodeTransformsBuffer;
    public Instance* instancesBuffer;
    public VisibleRenderItem* visibleRenderItemsBuffer;
};

[[vk::push_constant]]
public PickerDrawPushConstants pickerDrawPushConstants; 