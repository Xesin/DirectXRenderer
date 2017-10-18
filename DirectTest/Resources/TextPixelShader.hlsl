struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD;
};

Texture2D t1 : register(t0);
sampler s1 : register(s0);

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return float4(input.color.rgb, input.color.a * t1.Sample(s1, input.texCoord).a);
}