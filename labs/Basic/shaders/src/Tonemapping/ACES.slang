import TonemappingCommon;

[shader("fragment")]
float4 fragmentMain(VSOutput in) : SV_Target
{
    let tex = textures[pc.srcTextureIndex];
    let sampler = samplers[pc.samplerIndex];

    let hdrColor : float3 = tex.Sample(sampler, in.uv).rgb;

    // From: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return float4(saturate((hdrColor * (a * hdrColor + b)) / (hdrColor * (c * hdrColor + d) + e)), 1.0);
};
