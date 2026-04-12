struct VSOutput
{
    float4 position : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexId : SV_VulkanVertexID)
{
    const float3 positions[3] = float3[](
        float3(1.0f, 1.0f, 0.0f),
        float3(-1.0f, 1.0f, 0.0f),
        float3(0.0f, -1.0f, 0.0f)
    );

    const float3 colors[3] = float3[](
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    );

    VSOutput output;
    output.position = float4(positions[vertexId], 1.0f);
    output.color = colors[vertexId];
    return output;
};

[shader("fragment")]
float4 fragmentMain(VSOutput input)
{
    return float4(input.color, 1.0f);
}
