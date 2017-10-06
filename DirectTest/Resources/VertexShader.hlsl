
cbuffer SceneConstantBuffer : register(b0)
{
    float4 offset;
};


struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = position + offset.xyzw;
    result.color = color;

    return result;
}