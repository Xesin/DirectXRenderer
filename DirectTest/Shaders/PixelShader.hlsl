//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
	float3 normal : NORMAL;
};

Texture2D g_texture : register(t0);
sampler g_sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    float light = dot(float3(0.5, 0.0, -1.0), input.normal.xyz);

    float4 texColor = g_texture.Sample(g_sampler, input.uv);
    //float4 texColor = input.color;

    return texColor * light;
}
