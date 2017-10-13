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
		XMFLOAT4X4 wvpMat;
	};

	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	ComPtr<ID3D12Resource> indexBuffer; // a default buffer in GPU memory that we will load index data for our triangle into
	D3D12_INDEX_BUFFER_VIEW indexBufferView; // a structure holding information about the index buffer
	ComPtr<ID3D12Resource> depthStencilBuffer; // This is the memory for our depth buffer. it will also be used for a stencil buffer in a later tutorial
	ID3D12DescriptorHeap* dsDescriptorHeap; // This is a heap for our depth/stencil buffer descriptor
	AppBuffer constBuffer;
	int constBufferAlignedSize = (sizeof(constBuffer) + 255) & ~255;
	ComPtr<ID3D12Resource> constBufferUploadHeap;
	UINT8* constBufferGPUAddress;

	//Objetos de sincronización
	UINT frameIndex;
	HANDLE fenceEvent;
	ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;
	UINT64 fenceValues[FRAME_COUNT];

	XMFLOAT4X4 cameraProjMat; // this will store our projection matrix
	XMFLOAT4X4 cameraViewMat; // this will store our view matrix

	XMFLOAT4 cameraPosition; // this is our cameras position vector
	XMFLOAT4 cameraTarget; // a vector describing the point in space our camera is looking at
	XMFLOAT4 cameraUp; // the worlds up vector
	XMFLOAT4 cubePos;
	XMFLOAT4X4 cubeWorldMat; // our first cubes world matrix (transformation matrix)
	XMFLOAT4X4 cubeRotMat; // this will keep track of our rotation for the first cube

	XMFLOAT4 cube2Pos;
	XMFLOAT4X4 cube2WorldMat; // our first cubes world matrix (transformation matrix)
	XMFLOAT4X4 cube2RotMat; // this will keep track of our rotation for the first cube

	int numCubeIndices;

	UINT width;
	UINT height;

	
};