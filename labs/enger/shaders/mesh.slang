struct SceneData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 ambientColor;
    float4 sunlightDirection; // w for sun power
    float4 sunlightColor;
};

struct GLTFMaterialData
{
    float4 colorFactors;
    float4 metallicRoughnessFactors;
};

[[vk::binding(1, 0)]] Texture2D textures[];
[[vk::binding(2, 0)]] SamplerState samplers[];

struct Vertex
{
    float3 position;
    float uv_x;
    float3 normal;
    float uv_y;
    float4 color;
};

struct PushConstants
{
    float4x4 renderMatrix;
    Vertex* vertex;
    SceneData* scene;
    GLTFMaterialData* material;
    uint colorTextureIndex;
    uint metallicRoughnessTextureIndex;
    uint samplerIndex;
};
[[vk::push_constant]] PushConstants pc;

// VERTEX SHADER
// -------------

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
}

[shader("vertex")]
VSOutput vertexMain(uint vertexId : SV_VulkanVertexID)
{
    Vertex v = pc.vertex[vertexId];

    float4 position = float4(v.position, 1.0);

    VSOutput output;
    output.position = mul(pc.scene->viewProj, mul(pc.renderMatrix, position));

    output.normal = mul(pc.renderMatrix, float4(v.normal, 0.0)).xyz;
    output.color = v.color.rgb * pc.material->colorFactors.rgb;
    output.uv = float2(v.uv_x, v.uv_y);

    return output;
}

// FRAGMENT
// --------

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float lightValue = max(dot(input.normal, pc.scene->sunlightDirection.xyz), 0.1);

    // diffuse
    float3 color = input.color * textures[pc.colorTextureIndex].Sample(samplers[pc.samplerIndex], input.uv).rgb;
    float3 ambient = color * pc.scene->ambientColor.rgb;

    return float4(color * lightValue * pc.scene->sunlightColor.w + ambient, 1.0);
}

