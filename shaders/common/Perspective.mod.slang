module Perspective;

public struct Perspective {
    public float4x4 view;
    public float4x4 proj;
    public float4 ambientColor;
    public float4 sunlightDirection; // w for sun power
    public float4 sunlightColor;
};