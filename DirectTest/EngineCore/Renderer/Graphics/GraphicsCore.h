#pragma once

#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"

namespace Graphics
{
#ifndef RELEASE
	extern const GUID WKPDID_D3DDebugObjectName;
#endif

	using namespace Microsoft::WRL;

	extern ID3D12Device* g_Device;

	extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;
	extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
	extern bool g_bEnableHDROutput;

	extern DescriptorAllocator g_DescriptorAllocator[];
	inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
	{
		return g_DescriptorAllocator[Type].Allocate(Count);
	}

	extern RootSignature g_GenerateMipsRS;
	extern ComputePSO g_GenerateMipsLinearPSO[4];
	extern ComputePSO g_GenerateMipsGammaPSO[4];
}