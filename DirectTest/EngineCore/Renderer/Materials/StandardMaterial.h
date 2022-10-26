#pragma once
#include "Material.h"
#include "../Graphics/PipelineState.h"
#include "../Graphics/RootSignature.h"

class StandardMaterial : public Material {
public:
	StandardMaterial(GraphicContext* context);
	~StandardMaterial();
	virtual void BeginRender();
	virtual void OnRender() {}
	virtual void OnEndRender() {}
public:
	RootSignature rootSignature;
	GraphicsPSO graphicPSO;
	ComPtr<ID3D12DescriptorHeap> srvHeap;
	ID3D12Resource* textureBuffer; 
	ComPtr<ID3D12Resource> textureUploadHeap;
	UINT8* textureBufferGPUAddress;

	static UINT8* pVertexShaderData;
	static UINT vertexShaderDataLength;
	static UINT8* pPixelShaderData;
	static UINT pixelShaderDataLength;
};