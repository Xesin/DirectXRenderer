#pragma once
#include "../Core/DirectX/RootSignature.h"
#include "../Core/DirectX/PipelineState.h"
#include <DirectXMath.h>
#include <dxgi1_4.h>

using namespace DirectX;

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
	static ComPtr<ID3D12Device> device; //Representa un adaptador virtual. Sirve para crear command lists, queues, fences...

private:
	static const UINT FRAME_COUNT = 2; //Numero de frames que usamos para renderizar (2 = doble buffer)

	CD3DX12_VIEWPORT viewport;
	CD3DX12_RECT scissorRect; //Rect en el que se va a pintar
	ComPtr<IDXGISwapChain3> swapChain; //Objeto que intercala el back buffer para presentarlo por pantalla
	ComPtr<ID3D12Resource> renderTargets[FRAME_COUNT]; //Tenemos un render target por cada frame (Double buffer)
	ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT]; //el command allocator representa el almacenamiento de los GPU commands
	ComPtr<ID3D12CommandQueue> commandQueue; //Provee metodos para enviar command lists, sincronizar la ejecución de command lists...
	ComPtr<ID3D12GraphicsCommandList> commandList; //Encapsula una lista de comandos que tiene que ejecutar la GPU
	RootSignature rootSignature; //Define que recursos están vinculados a la graphic pipeline.
	ComPtr<ID3D12DescriptorHeap> rtvHeap; //almacena la posición de nuestro render target view
	UINT rtvDescriptorSize; //Tamaño de nuestro rtv Descriptor
	ComPtr<ID3D12DescriptorHeap> cbvHeap; //almacena la posición de nuestro constant buffer view
	ComPtr<ID3D12DescriptorHeap> srvHeap; //almacena la posición de nuestro Shader Resource view
	GraphicsPSO pipelineState; //Representa el estado de todos los shaders que se han asignado

	ComPtr<ID3D12Resource> vertexBuffer; //El buffer encargado de cargar los vertices en la GPU
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView; //Una estructura que almacena la información de los vertices
	ComPtr<ID3D12Resource> indexBuffer; // El buffer encargado de cargar los indices en la GPU
	D3D12_INDEX_BUFFER_VIEW indexBufferView; //Una estructura que almacena la información de los indices
	ComPtr<ID3D12Resource> depthStencilBuffer; // Es la memoria de nuestro depth stencil.
	ID3D12DescriptorHeap* dsDescriptorHeap; // es nuestro heap para el depth/stencil buffer descriptor
	AppBuffer constantBufferData; //Objeto que usamos para asignar los datos de nuestro constant buffer
	int constBufferAlignedSize = (sizeof(constantBufferData) + 255) & ~255; // Necesitamos que el buffer sea de bloques alineados de 256 bytes
	ComPtr<ID3D12Resource> constBufferUploadHeap;  //El buffer encargado de cargar el constant buffer a la GPU
	UINT8* constBufferGPUAddress; //Posición de memoria de nuestro Constant Buffer
	ComPtr<ID3D12Resource> textureBuffer; //El buffer encargado de cargar la textura a la GPU
	UINT8* textureBufferGPUAddress; //Posición de memoria de nuestro texture buffer

	//Objetos de sincronización
	UINT frameIndex; //Frame que actualmente estamos renderizando
	HANDLE fenceEvent; //Evento para la sincronización
	ComPtr<ID3D12Fence> fence; //Objeto que se encarga de crear los eventos de singronicación
	UINT64 fenceValue; //Valor actual de nuestra barrera
	UINT64 fenceValues[FRAME_COUNT]; //Valores según el frame index actual

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

	
};