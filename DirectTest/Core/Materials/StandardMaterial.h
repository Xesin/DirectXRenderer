#pragma once
#include "Material.h"
#include "../DirectX/PipelineState.h"
#include "../DirectX/RootSignature.h"

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
	ComPtr<ID3D12DescriptorHeap> srvHeap; //almacena la posición de nuestro Shader Resource view
	ID3D12Resource* textureBuffer; //El buffer encargado de cargar la textura a la GPU
	ComPtr<ID3D12Resource> textureUploadHeap;
	UINT8* textureBufferGPUAddress; //Posición de memoria de nuestro texture buffer

	static UINT8* pVertexShaderData;
	static UINT vertexShaderDataLength;
	static UINT8* pPixelShaderData;
	static UINT pixelShaderDataLength;
};