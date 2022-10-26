#pragma once
#include <dxgi1_4.h>
#include "../Graphics/PipelineState.h"
#include "../Graphics/RootSignature.h"

using namespace DirectX;
using namespace Microsoft::WRL;

const bool FULL_SCREEN = false;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.1f;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT2 uv;
	XMFLOAT4 color;
	XMFLOAT3 normal;
};

struct AppBuffer
{
	XMFLOAT4X4 wvpMat;
	XMFLOAT4X4 worldMat;
};

namespace Renderer {

	extern ComPtr<ID3D12Device> device;
	class GraphicContext {
	public:
		GraphicContext(UINT width, UINT height);
		void Initialize();
		void OnUpdate();
		bool OnRender();
		void Release();
		void OnResize(UINT width, UINT height);

		void SetRootSignature(const RootSignature& RootSig);

		void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[]);
		void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
		void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets(1, &RTV); }
		void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(1, &RTV, DSV); }
		void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(0, nullptr, DSV); }

		void SetViewport(const D3D12_VIEWPORT& vp);
		void SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f);
		void SetScissor(const D3D12_RECT& rect);
		void SetScissor(UINT left, UINT top, UINT right, UINT bottom);
		void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
		void SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h);
		void SetStencilRef(UINT StencilRef);
		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);

		void SetPipelineState(const GraphicsPSO& PSO);
		void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV, UINT Offset = 0);
		void SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData);
		void SetBufferSRV(UINT RootIndex, const D3D12_GPU_VIRTUAL_ADDRESS vAddress, UINT Offset);
		void SetBufferUAV(UINT RootIndex, const D3D12_GPU_VIRTUAL_ADDRESS vAddress, UINT Offset);
		void SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle);

		void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
		void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
		void SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[]);

		void Draw(UINT VertexCount, UINT VertexStartOffset = 0);
		void DrawIndexed(UINT IndexCount, UINT StartIndexLocation = 0, INT BaseVertexLocation = 0);
		void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
			UINT StartVertexLocation = 0, UINT StartInstanceLocation = 0);
		void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
			INT BaseVertexLocation, UINT StartInstanceLocation);

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
		ComPtr<ID3D12GraphicsCommandList> commandList; 
	private:
		static const UINT FRAME_COUNT = 2;

		CD3DX12_VIEWPORT viewport;
		CD3DX12_RECT scissorRect;
		ComPtr<IDXGISwapChain3> swapChain;
		ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT];
		ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT];
		ComPtr<ID3D12CommandQueue> commandQueue;
		ID3D12RootSignature* currRootSignature;
		ID3D12PipelineState* m_CurGraphicsPipelineState;
		ComPtr<ID3D12DescriptorHeap> rtvHeap;
		UINT rtvDescriptorSize;

		ComPtr<ID3D12Resource> depthStencilBuffer;
		ID3D12DescriptorHeap* dsDescriptorHeap;
		
		UINT frameIndex; 
		HANDLE fenceEvent; 
		ComPtr<ID3D12Fence> fence; 
		UINT64 fenceValue;
		UINT64 fenceValues[FRAME_COUNT]; 

		XMFLOAT4X4 cameraProjMat; 
		XMFLOAT4X4 cameraViewMat; 

		XMFLOAT4 cameraPosition;
		XMFLOAT4 cameraTarget; 
		XMFLOAT4 cameraUp; 
		XMFLOAT4 cubePosition;
		XMFLOAT4 cube2Pos;
		XMFLOAT4X4 cubeWorldMat; 
		XMFLOAT4X4 cubeRotMat; 

		XMFLOAT4X4 cube2WorldMat; 
		XMFLOAT4X4 cube2RotMat;  

		int numCubeIndices; 

		UINT width; 
		UINT height; 

		XMFLOAT4 clearColor = XMFLOAT4(0.3f, 0.2f, 0.4f, 1.0f);

	};

	extern GraphicContext* renderer;
}