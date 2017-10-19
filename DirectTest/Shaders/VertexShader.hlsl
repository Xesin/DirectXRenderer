struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
	float3 normal : NORMAL;
};

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
	float3 normal : NORMAL;
};

cbuffer PerVertexData : register(b0)
{
    float4x4 wvpMat;
	float4x4 worldMat;
}

PSInput VSMain(VSInput input)
{
    PSInput result;

    result.position = mul(input.position,  wvpMat);
    result.uv = input.uv;
    result.color = input.color;
    result.normal = mul(input.normal, (float3x3) worldMat);
	result.normal = normalize(result.normal);
    return result;
}