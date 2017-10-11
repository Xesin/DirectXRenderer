struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

cbuffer AppConstBuffer : register(b0)
{
    float4 offset;
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = input.position + offset;
    result.uv = input.uv;
    result.color = input.color;
    return result;
}