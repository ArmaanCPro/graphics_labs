[[vk::binding(1, 0)]] Texture2D textures[];
[[vk::binding(2, 0)]] SamplerState samplers[];

struct PushConstants
{
    uint srcTextureIndex;
    uint samplerIndex;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Procedurally generates a Fullscreen Triangle based on the vertex ID (pass in 3 vertices in draw call)
[shader("vertex")]
VSOutput vertexMain(uint id : SV_VulkanVertexID)
{
    VSOutput output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.position = float4(output.uv * float2(2, 2) + float2(-1, -1), 0, 1);
    return output;
}

[[vk::push_constant]]
PushConstants pc;

[shader("fragment")]
float4 fragmentMain(VSOutput in) : SV_Target
{
    let tex = textures[pc.srcTextureIndex];
    let sampler = samplers[pc.samplerIndex];

    let hdrColor : float3 = tex.Sample(sampler, in.uv).rgb;

    let mapped : float3 = hdrColor / (hdrColor + float3(1.0));

    return float4(mapped, 1.0);
};
