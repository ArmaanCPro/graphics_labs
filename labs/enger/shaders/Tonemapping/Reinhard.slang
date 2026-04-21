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
    let verts = float2[3](float2(-1, -1), float2(3, -1), float2(-1, 3));
    VSOutput output;
    output.position = float4(verts[id], 0, 1);
    output.uv = 0.5 * output.position.xy + float2(0.5);
    return output;

    //float2 uv = float2((id << 1) & 2, id & 2);
    // xy is the UV of the triangle, zw is irrelevant
    //return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
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
