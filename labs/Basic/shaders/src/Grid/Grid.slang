import GridCommon;

struct PushConstants
{
    float4x4 MVP;
    float4 cameraPos;
    float4 origin;
};
[[vk::push_constant]] PushConstants pc;

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float2 cameraPos;
};


[shader("vertex")]
VSOutput vertexMain(uint vertId : SV_VulkanVertexID)
{
    let vertices = float3[4](
        float3(-1.0, 0.0, -1.0),
        float3(1.0, 0.0, -1.0),
        float3(1.0, 0.0, 1.0),
        float3(-1.0, 0.0, 1.0),
    );

    let indices = uint[6](
        0, 1, 2, 2, 3, 0
    );

    int index = indices[vertId];
    float3 pos = vertices[index] * gridSize;
    pos.x += pc.cameraPos.x;
    pos.z += pc.cameraPos.z;
    pos += pc.origin.xyz;

    VSOutput output;
    output.pos = mul(pc.MVP, float4(pos, 1.0));
    output.uv = pos.xz;
    output.cameraPos = pc.cameraPos.xz;
    return output;
}

// FRAGMENT

float max2(float2 v) { return max(v.x, v.y); }

float glslMod(float x, float y)
{
    return x - y * floor(x / y);
}
float2 glslMod(float2 x, float y)
{
    return float2(glslMod(x.x, y), glslMod(x.y, y));
}

float4 gridColor(float2 uv, float2 camPos)
{
    var dudv : float2 = fwidth(uv);
    let lodLevel : float = max(0.0, log10((length(dudv) *
        gridMinPixelsBetweenCells) / gridCellSize) + 1.0);
    let lodFade : float = frac(lodLevel);
    let lod0 = gridCellSize * pow(10.0, floor(lodLevel));
    let lod1 = lod0 * 10.0;
    let lod2 = lod1 * 10.0;

    dudv *= 4.0;
    uv += dudv * 0.5;

    let lod0a = max2(float2(1.0) - abs(saturate(glslMod(uv, lod0) / dudv) * 2.0 - float2(1.0)) );
    let lod1a = max2(float2(1.0) - abs(saturate(glslMod(uv, lod1) / dudv) * 2.0 - float2(1.0)) );
    let lod2a = max2(float2(1.0) - abs(saturate(glslMod(uv, lod2) / dudv) * 2.0 - float2(1.0)) );

    float4 c = lod2a > 0.0 ?
        gridColorThick :
        lod1a > 0.0 ?
            lerp(gridColorThick, gridColorThin, lodFade) : gridColorThin;

    uv -= camPos;
    let opacityFalloff = (1.0 - saturate(length(uv) / gridSize));
    c.a *= lod2a > 0.0 ?
        lod2a : lod1a > 0.0 ? lod1a : (lod0a * (1.0 - lodFade));
    c.a *= opacityFalloff;
    return c;
};

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return gridColor(input.uv, input.cameraPos);
};
