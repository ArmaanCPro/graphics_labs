import TonemappingCommon;

// Procedurally generates a Fullscreen Triangle based on the vertex ID (pass in 3 vertices in draw call)
// https://wallisc.github.io/rendering/2021/04/18/Fullscreen-Pass.html
[shader("vertex")]
VSOutput vertexMain(uint id : SV_VulkanVertexID)
{
    VSOutput output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.position = float4(output.uv * float2(2, 2) + float2(-1, -1), 0, 1);
    return output;
}
