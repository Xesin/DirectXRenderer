struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

//cbuffer AppConstBuffer : register(b0)
//{
//    float4 offset;
//}

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = input.position;
    result.uv = input.uv;
    return result;
}