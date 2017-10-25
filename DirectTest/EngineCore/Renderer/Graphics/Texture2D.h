#pragma once
#include "GpuResource.h"

class Texture2D : public GpuResource
{
	friend class GraphicContext;

public:
	Texture2D() { m_hCpuDescriptorHandle.ptr = (D3D12_GPU_VIRTUAL_ADDRESS)-1; }
	Texture2D(D3D12_CPU_DESCRIPTOR_HANDLE Handle) : m_hCpuDescriptorHandle(Handle) {}

	// Create a 1-level 2D texture
	void Create(size_t Pitch, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData, UINT bytesPerPixel);
	void Create(size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData, UINT bytesPerPixel)
	{
		Create(Width, Width, Height, Format, InitData, bytesPerPixel);
	}

	void CreateTGAFromMemory(const void* memBuffer, size_t fileSize, bool sRGB);
	bool CreateDDSFromMemory(const void* memBuffer, size_t fileSize, bool sRGB);
	void CreatePIXImageFromMemory(const void* memBuffer, size_t fileSize);

	virtual void Destroy() override
	{
		GpuResource::Destroy();
		m_hCpuDescriptorHandle.ptr = 0;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_hCpuDescriptorHandle; }

	bool operator!() { return m_hCpuDescriptorHandle.ptr == 0; }

protected:
	D3D12_CPU_DESCRIPTOR_HANDLE m_hCpuDescriptorHandle;
};