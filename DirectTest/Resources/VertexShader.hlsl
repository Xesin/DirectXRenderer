
/*cbuffer SceneConstantBuffer : register(b0)
{
    float4 offset;
};*/


cbuffer SceneConstantBuffer : register(b0)
{
    float4 offset;
}

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position = position + offset;
    result.uv = uv;
    return result;
}