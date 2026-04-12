struct Vertex
{
    float3 position;
    float uv_x;
    float3 normal;
    float uv_y;
    float4 color;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color;
    float2 uv;
};

struct PushConstant
{
    float4x4 renderMatrix;
    Vertex* vertexBuffer;
};
[[vk::push_constant]] PushConstant pc;

[shader("vertex")]
VSOutput vertexMain(uint vertexId : SV_VulkanVertexID)
{
    Vertex v = pc.vertexBuffer[vertexId];

    VSOutput output;
    output.position = mul(pc.renderMatrix, float4(v.position, 1.0f));
    output.color = v.color.xyz;
    output.uv = float2(v.uv_x, v.uv_y);
    return output;
};

[shader("fragment")]
float4 fragmentMain(VSOutput input)
{
    return float4(input.color, 1.0f);
}
