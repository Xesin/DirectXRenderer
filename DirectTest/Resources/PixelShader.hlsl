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

//Texture2D g_texture;
//SamplerState g_sampler;

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
	float3 normal : NORMAL;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    //return g_texture.Sample(g_sampler, input.uv);
    //return input.color;
	float light = max(0.0, (dot(float3(1.0, 0.0, 0.5), input.normal)));
    return float4(input.normal, 1.0);
}
