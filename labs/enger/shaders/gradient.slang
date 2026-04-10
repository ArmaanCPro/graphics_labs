
[format("rgba16f")]
RWTexture2D<float4> texture;

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID, uint3 localInvocationId : SV_GroupThreadID)
{
    uint width, height;
    texture.GetDimensions(width, height);

    int2 texelCoord = int2(threadId.xy);
    int2 size = int2(width, height);

    if (texelCoord.x < size.x && texelCoord.y < size.y)
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        if (threadId.x != 0 && threadId.y != 0)
        {
            color.x = float(texelCoord.x) / float(size.x);
            color.y = float(texelCoord.y) / float(size.y);
        }

        texture[texelCoord] = color;
    }
};