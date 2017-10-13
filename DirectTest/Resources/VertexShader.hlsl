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
    float4x4 wvpMat;
    
    // now pad the constant buffer to be 256 byte aligned
    float4 padding[48];
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = mul(input.position, wvpMat);
    result.uv = input.uv;
    result.color = input.color;
    return result;
}