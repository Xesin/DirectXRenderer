#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgi1_4.h>
#include <wrl\client.h>
#include <vector>
#include "d3dx12.h"

using namespace Microsoft::WRL;
using namespace DirectX;

const bool FULL_SCREEN = false;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.1f;

class DirectRenderer {
public:
	DirectRenderer(UINT width, UINT height);
	void Initialize();
	void OnUpdate();
	bool OnRender();
	void Release();
	void OnResize(UINT width, UINT height);
	
private:
	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void MoveToNextFrame();
	void WaitForGpu();
	std::vector<UINT8> GenerateTextureData();

	void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter);

	float aspectRatio;
public:
	bool useWarpDevice;

private:
	static const UINT FRAME_COUNT = 2;

	CD3DX12_VIEWPORT viewport;
	CD3DX12_RECT scissorRect;
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];
	ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT];
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	ComPtr<ID3D12DescriptorHeap> cbvHeap;
	ComPtr<ID3D12PipelineState> pipelineState;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	UINT rtvDescriptorSize;

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT2 uv;
		XMFLOAT4 color;
	};

	struct AppBuffer
	{
		XMFLOAT4 offset;
	};

	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	AppBuffer constBuffer;
	ComPtr<ID3D12Resource> constBufferUploadHeap;
	UINT8* constBufferGPUAddress;

	//Objetos de sincronización
	UINT frameIndex;
	HANDLE fenceEvent;
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;
	UINT64 fenceValues[FRAME_COUNT];

	UINT width;
	UINT height;

	
};