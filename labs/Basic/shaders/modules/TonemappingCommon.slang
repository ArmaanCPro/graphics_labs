module TonemappingCommon;

[[vk::binding(1, 0)]] public Texture2D textures[];
[[vk::binding(2, 0)]] public SamplerState samplers[];

public struct PushConstants
{
    public uint srcTextureIndex;
    public uint samplerIndex;
};
[[vk::push_constant]] public PushConstants pc;

public struct VSOutput
{
    public float4 position : SV_Position;
    public float2 uv : TEXCOORD0;
};
