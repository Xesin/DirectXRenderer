#include "DirectRenderer.h"
#include "../Core/WinApplication.h"
#include "../Core/DirectXHelper.h"
#include <d3dcompiler.h>
#include "d3dx12.h"

DirectRenderer::DirectRenderer(UINT width, UINT height) :
	frameIndex(0),
	viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	rtvDescriptorSize(0),
	aspectRatio(static_cast<float>(width) / static_cast<float>(height))
{
}

void DirectRenderer::Initialize() {
	LoadPipeline();
	LoadAssets();
	//return ref;
}

void DirectRenderer::OnUpdate()
{
}

bool DirectRenderer::OnRender()
{
	//Grabamos todo lo que la GPU tiene que ejecutar en este frame
	PopulateCommandList();

	//Ejecutamos el command list
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//Presentamos el frame
	ThrowIfFailed(swapChain->Present(1, 0));

	WaitForPreviousFrame();

	return true;
}

void DirectRenderer::Release()
{
	WaitForPreviousFrame();

	CloseHandle(fenceEvent);
}

void DirectRenderer::OnResize(UINT width, UINT height)
{

	aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void DirectRenderer::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	//Activamos el debug layer
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}

#endif

	//Creamos el DXGI Factory
	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&device)
		));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&device)
		));
	}

	//Crear el descriptor del command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc;
	ZeroMemory(&queueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));

	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	//Creamos el command queue
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

	//Crear el descriptor del swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC1));
	swapChainDesc.BufferCount = FRAME_COUNT;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	//Creamos el swapChain

	ComPtr<IDXGISwapChain1> tempSwapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.Get(),
		WinApplication::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&tempSwapChain
	));

	//Creamos la asociación con la ventana
	ThrowIfFailed(factory->MakeWindowAssociation(WinApplication::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(tempSwapChain.As(&swapChain));
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	//Crear el descriptor heap
	{
		//Crear el descriptor heap del render target view (RTV)
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		ZeroMemory(&rtvHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
		rtvHeapDesc.NumDescriptors = FRAME_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		//Crear el rtvHeap
		ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

		rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	//Crear los recursos del frame
	{
		
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

		//Crear un RTV por cada frame
		for (UINT n = 0; n < FRAME_COUNT; n++) {
			ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
			device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, rtvDescriptorSize);
		}
	}

	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
}

void DirectRenderer::LoadAssets()
{
	//Crear la firma raíz vacía
	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
	}

	//Crear el estado del pipeline, incluído el compilado de shaders
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(L"Resources/shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(L"Resources/shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
		// Definir el input del vertex shader
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		//Describir y crear el graphics pipeline state object (PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
	}

	// crear el command list.
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&commandList)));

	//Los command list son creados en el estado de grabar, pero no tenemos nada que grabar todavía.
	//El loop principal espera que sea cerrado, así que lo cerramos ahora.
	ThrowIfFailed(commandList->Close());

	//Crear el vertex Buffer
	{
		Vertex triangleVertices[] = 
		{
			{ { 0.0f, 0.25f * aspectRatio, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * aspectRatio, 0.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * aspectRatio, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		//Creamos el recurso que pasaremos a la GPU
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&vertexBuffer)
		));

		UINT8* vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);				//No tenemos intención de leer este recurso desde la CPU
		ThrowIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
		memcpy(vertexDataBegin, triangleVertices, vertexBufferSize);
		vertexBuffer->Unmap(0, nullptr);

		//Inicializamos el vertex buffer view
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	//Creamos los objetos de sincronización y esperamos hasta que todo se haya cargado en la GPU
	{
		ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		fenceValue = 1;

		//Crear el event handler para la sincronización de frames
		fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		if (fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		WaitForPreviousFrame();
	}
}

void DirectRenderer::PopulateCommandList()
{
	//Reseteamos el command allocator
	ThrowIfFailed(commandAllocator->Reset());

	//Reseteamos el command list
	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), pipelineState.Get()));

	//Set los estados necesarios
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	//Indicamos al back buffer que será usado como render target
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	//Grabamos los comandos
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->DrawInstanced(3, 1, 0, 0);
	
	// Indicate that the back buffer will now be used to present.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(commandList->Close());

}

void DirectRenderer::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fenceVal = fenceValue;
	ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceVal));
	fenceValue++;

	// Wait until the previous frame is finished.
	if (fence->GetCompletedValue() < fenceVal)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceVal, fenceEvent));
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void DirectRenderer::GetHardwareAdapter(IDXGIFactory4 * pFactory, IDXGIAdapter1 ** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}