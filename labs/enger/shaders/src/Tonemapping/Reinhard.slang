import TonemappingCommon;

[shader("fragment")]
float4 fragmentMain(VSOutput in) : SV_Target
{
    let tex = textures[pc.srcTextureIndex];
    let sampler = samplers[pc.samplerIndex];

    let hdrColor : float3 = tex.Sample(sampler, in.uv).rgb;

    let mapped : float3 = hdrColor / (hdrColor + float3(1.0));

    return float4(mapped, 1.0);
};
