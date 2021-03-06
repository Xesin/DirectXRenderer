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

	extern ComPtr<ID3D12Device> device; //Representa un adaptador virtual. Sirve para crear command lists, queues, fences...
	class GraphicContext {
	public:
		GraphicContext(UINT width, UINT height);
		void Initialize();
		void OnUpdate();
		bool OnRender();
		void Release();
		void OnResize(UINT width, UINT height);

		/*void BeginQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex);
		void EndQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex);
		void ResolveQueryData(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource* DestinationBuffer, UINT64 DestinationBufferOffset);*/

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
		ComPtr<ID3D12GraphicsCommandList> commandList; //Encapsula una lista de comandos que tiene que ejecutar la GPU
	private:
		static const UINT FRAME_COUNT = 2; //Numero de frames que usamos para renderizar (2 = doble buffer)

		CD3DX12_VIEWPORT viewport;
		CD3DX12_RECT scissorRect; //Rect en el que se va a pintar
		ComPtr<IDXGISwapChain3> swapChain; //Objeto que intercala el back buffer para presentarlo por pantalla
		ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT]; //Tenemos un render target por cada frame (Double buffer)
		ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT]; //el command allocator representa el almacenamiento de los GPU commands
		ComPtr<ID3D12CommandQueue> commandQueue; //Provee metodos para enviar command lists, sincronizar la ejecuci�n de command lists...
		ID3D12RootSignature* currRootSignature; //Define que recursos est�n vinculados a la graphic pipeline.
		ID3D12PipelineState* m_CurGraphicsPipelineState;
		ComPtr<ID3D12DescriptorHeap> rtvHeap; //almacena la posici�n de nuestro render target view
		UINT rtvDescriptorSize; //Tama�o de nuestro rtv Descriptor

		ComPtr<ID3D12Resource> depthStencilBuffer; // Es la memoria de nuestro depth stencil.
		ID3D12DescriptorHeap* dsDescriptorHeap; // es nuestro heap para el depth/stencil buffer descriptor

		//Objetos de sincronizaci�n
		UINT frameIndex; //Frame que actualmente estamos renderizando
		HANDLE fenceEvent; //Evento para la sincronizaci�n
		ComPtr<ID3D12Fence> fence; //Objeto que se encarga de crear los eventos de singronicaci�n
		UINT64 fenceValue; //Valor actual de nuestra barrera
		UINT64 fenceValues[FRAME_COUNT]; //Valores seg�n el frame index actual

		XMFLOAT4X4 cameraProjMat; // Projection Matrix de la camara
		XMFLOAT4X4 cameraViewMat; // View matrix de la camera

		XMFLOAT4 cameraPosition;
		XMFLOAT4 cameraTarget; // Vector que describe el punto al que mira la camara
		XMFLOAT4 cameraUp; // El vector hacia arriba del mundo
		XMFLOAT4 cubePosition;
		XMFLOAT4 cube2Pos;
		XMFLOAT4X4 cubeWorldMat; // world matrix de nuestro primer cubo
		XMFLOAT4X4 cubeRotMat; // rotation matrix de nuestro segundo cubo

		XMFLOAT4X4 cube2WorldMat; // world matrix de nuestro segundo cubo
		XMFLOAT4X4 cube2RotMat;  // rotation matrix de nuestro segundo cubo

		int numCubeIndices; //Numero de indices de cada cubo

		UINT width; //Ancho de la pantalla
		UINT height; //Alto de la pantalla

		XMFLOAT4 clearColor = XMFLOAT4(0.3f, 0.2f, 0.4f, 1.0f);

	};

	extern GraphicContext* renderer;
}