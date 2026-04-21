
struct PushConstants
{
    float4 data1;
    float4 data2;
    uint textureIndex;
};
[[vk::push_constant]] PushConstants pc;

[format("rgba16f")]
RWTexture2D<float4> textures[];

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID, uint3 localInvocationId : SV_GroupThreadID)
{
    uint width, height;
    RWTexture2D<float4> texture = textures[pc.textureIndex];
    texture.GetDimensions(width, height);

    int2 texelCoord = int2(threadId.xy);
    int2 size = int2(width, height);

    float4 topColor = pc.data1;
    float4 bottomColor = pc.data2;

    if (texelCoord.x < size.x && texelCoord.y < size.y)
    {
        float blend = float(texelCoord.y) / float(size.y);

        texture[texelCoord] = lerp(topColor, bottomColor, blend);
    }
};