#include "DirectRenderer.h"
#include "../Objects/Mesh.h"
#include "../Core/Core.h"
#include <d3dcompiler.h>

Mesh* newMesh;
using namespace DirectX;

namespace Renderer {
	ComPtr<ID3D12Device> device;
	DirectRenderer* renderer;

	DirectRenderer::DirectRenderer(UINT width, UINT height) :
		frameIndex(0),
		viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
		scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
		aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
		width(width),
		height(height),
		fenceValues{},
		rtvDescriptorSize(0)
	{
	}

	void DirectRenderer::Initialize()
	{
		LoadPipeline();
		LoadAssets();
	}

	// Cargar las dependencias de la rendering pipeline
	void DirectRenderer::LoadPipeline()
	{
		UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
		// Activamos el debug layer 
		// NOTE: Activar el debug layer después de la creación del device invalidará el device activo.
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Activar capas de debug adicionales.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif
		//Permite crear objetos de la infraestructura DirectX
		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		//Info sobre warp adapter: https://msdn.microsoft.com/en-us/library/windows/desktop/gg615082(v=vs.85).aspx
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
			//Optenemos el adaptador de hardware (GPU)
			ComPtr<IDXGIAdapter1> hardwareAdapter;
			GetHardwareAdapter(factory.Get(), &hardwareAdapter);

			//Creamos el device a partir del adaptador
			ThrowIfFailed(D3D12CreateDevice(
				hardwareAdapter.Get(),
				D3D_FEATURE_LEVEL_12_0,
				IID_PPV_ARGS(&device)
			));
		}

		// Describir y crear la command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

		// Describir y crear la swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FRAME_COUNT;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChainTmp;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(
			commandQueue.Get(),
			WinApplication::GetHwnd(),
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChainTmp
		));

		//Hacemos la asociación a la ventana para poder representar lo que pintemos
		ThrowIfFailed(factory->MakeWindowAssociation(WinApplication::GetHwnd(), DXGI_MWA_VALID));

		//Almacenamos el swap chain temporal en nuestro objeto definitivo
		ThrowIfFailed(swapChainTmp.As(&swapChain));

		//Guardamos el indice del back buffer
		frameIndex = swapChain->GetCurrentBackBufferIndex();

		// Creamos los descriptor heaps.
		{
			// Describir y crear el render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = FRAME_COUNT;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

			//Obtenemos su tamaño
			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			// Describir y crear el shader resource view (SRV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			srvHeapDesc.NumDescriptors = 1;
			srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)));
		}

		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Crear un RenderTarget y un command allocator por cada frame
			for (UINT n = 0; n < FRAME_COUNT; n++)
			{
				//Obtenemos el buffer
				ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));

				//Creamos el render target sobre y lo almacenamos en el buffer
				device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
				//Hacemos un offset de la memoria
				rtvHandle.Offset(1, rtvDescriptorSize);

				//Creamos el command allocator
				ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
			}
		}
	}

	// Cargar los assets para el ejemplo.
	void DirectRenderer::LoadAssets()
	{
		rootSignature.Reset(2, 1);

		//Creamos un sampler desc para nuestro sampler estatico
		D3D12_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;

		rootSignature.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
		rootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

		rootSignature.Finalize(L"Cube render", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


		//Cargamos los shaders previamente compilados por el compilador de HLSL
		UINT8* pVertexShaderData;
		UINT vertexShaderDataLength;
		UINT8* pPixelShaderData;
		UINT pixelShaderDataLength;

		//Cargamos el vertex shader
		ThrowIfFailed(ReadDataFromFile(L"Resources\\ShaderLib\\VertexShader.cso", &pVertexShaderData, &vertexShaderDataLength));
		//Cargamos el pixel shader
		ThrowIfFailed(ReadDataFromFile(L"Resources\\ShaderLib\\PixelShader.cso", &pPixelShaderData, &pixelShaderDataLength));

		// Creamos el input layout, esto describe el input que espera tener el vertex shader

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 12 + 8 + 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Creamos un pipeline state object (PSO)
		CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);
		CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
		CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
		blendDesc.AlphaToCoverageEnable = FALSE;

		pipelineState.SetInputLayout(4, inputLayout);
		pipelineState.SetRootSignature(rootSignature);
		pipelineState.SetVertexShader(CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength));
		pipelineState.SetPixelShader(CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength));
		pipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		pipelineState.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
		pipelineState.SetRasterizerState(rasterDesc);
		pipelineState.SetBlendState(blendDesc);
		pipelineState.SetDepthStencilState(depthStencilDesc);
		pipelineState.Finalize();

		// Crear el command list
		ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), nullptr, IID_PPV_ARGS(&commandList)));

		// Crear los objetos de sincronización
		{
			//Evento de la barrera
			ThrowIfFailed(device->CreateFence(fenceValues[frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			fenceValues[frameIndex]++;

			//Creamos el evento que se usa para la sincronización.
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// Crear el vertex buffer y el index buffer
		{
			// Creamos la lista de vertices que necesita el cubo
			std::vector<Vertex> vecVList;
			Vertex* vList;
			// Cara delantera (cerca de la camara)
			//{posX, posY, poxZ}, {u,v}, {r, g, b, a}, {x, y, z} (posicion, UVs, color, normales (Smooth))
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f,  0.0f, -1.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },	{  0.0f, 0.0f, -1.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },	{ 0.0f, 0.0f, -1.0f } });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  0.0f,  0.0f, -1.0f } });

			// Cara derecha
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },	{  1.0f, 0.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } ,		{  1.0f,  0.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  1.0f, 0.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  1.0f,  0.0f, 0.0f} });


			// cara izquierda
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },		{ -1.0f,  0.0f,  0.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },	{ -1.0f, 0.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ -1.0f, 0.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ -1.0f,  0.0f, 0.0f} });


			// Cara trasera
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } ,		{  0.0f,  0.0f,  1.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f,  1.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{  0.0f, 0.0f,  1.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ 0.0f,  0.0f,  1.0f } });


			// Cara de arriba
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f,  1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } ,		{  0.0f,  1.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  0.0f,  1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{ 0.0f,  1.0f,  0.0f} });


			// Cara de abajo
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },		{  0.0f, -1.0f,  0.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },	{ 0.0f, -1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{  0.0f, -1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ 0.0f, -1.0f,  0.0f} });

			vList = vecVList.data();

			// Lista de indices, indica que vertices van unidos
			DWORD iList[] = {
				// Cara delantera
				0, 1, 2, // primer triangulo
				0, 3, 1, // segundo triangulo

				// Cara izquierda
				4, 5, 6, // primer triangulo
				4, 7, 5, // segundo triangulo

				// Cara derecha
				8, 9, 10, // primer triangulo
				8, 11, 9, // segundo triangulo

				// Cara trasera
				12, 13, 14, // primer triangulo
				12, 15, 13, // segundo triangulo

				// Cara de arriba
				16, 17, 18, // primer triangulo
				16, 19, 17, // segundo triangulo

				// Cara de abajo
				20, 21, 22, // primer triangulo
				20, 23, 21, // segundo triangulo
			};

			//Tamaño de los buffers
			int vBufferSize = sizeof(vecVList);
			int iBufferSize = sizeof(iList);

			//Cantidad de indices de cada cubo
			numCubeIndices = sizeof(iList) / sizeof(DWORD);

			newMesh = new Mesh(this);
			newMesh->SetVertices(vList, vecVList.size());
			newMesh->SetIndices(iList, sizeof(iList) / sizeof(DWORD));

			newMesh->Initialize(device.Get(), commandList.Get());

			newMesh->pos = XMFLOAT3(0.0f, 0.0f, -2.f);
		}

		//Crear el depth/stencil
		{
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap)));

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
			depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

			//Solo creamos un heap de tipo default ya que sólo vamos a escribir en el
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(&depthStencilBuffer)
			);
			dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

			//Creamos el stencil view
			device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		}

		ComPtr<ID3D12Resource> textureUploadHeap;
		//Creamos la textura
		{
			int imageBytesPerRow;
			Image imageData;
			ZeroMemory(&imageData, sizeof(Image));
			ImageLoader::LoadImageFromFile(L"Resources/woodTexture.jpg", imageBytesPerRow, &imageData);


			// Creamos la descripcion
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = imageData.dxgiFormat;
			textureDesc.Width = imageData.textureWidth;
			textureDesc.Height = imageData.textureHeight;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			//Mismo proceso que antes, primero un heap de tipo default
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&textureBuffer)));


			UINT64 textureUploadBufferSize;

			//Esta funcion obtiene el tamaño del buffer que necesitamos
			device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

			// Ahora podemos crear el upload heap
			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap)));
			textureUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = &imageData.imageData[0];
			textureData.RowPitch = imageBytesPerRow;
			textureData.SlicePitch = textureData.RowPitch * imageData.textureHeight;

			//Copiamos la textura al default heap
			UpdateSubresources(commandList.Get(), textureBuffer.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			// Describimos y creamos el SRV para la textura
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(textureBuffer.Get(), &srvDesc, srvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		//Crear los view y projection matrix
		{
			//Matrix de perspectiva
			XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(60.0f*(3.14f / 180.0f), (float)width / (float)height, 0.1f, 1000.0f);

			//Asignamos el projection matrix
			XMStoreFloat4x4(&cameraProjMat, tmpMat);

			// Set de la camara
			cameraPosition = XMFLOAT4(0.0f, 0.0f, -4.0f, 0.0f);
			cameraTarget = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			cameraUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

			// Creamos el view matrix
			XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
			XMVECTOR cTarg = XMLoadFloat4(&cameraTarget);
			XMVECTOR cUp = XMLoadFloat4(&cameraUp);

			tmpMat = XMMatrixLookAtLH(cPos, cTarg, cUp);

			//Asignamos el view matrix
			XMStoreFloat4x4(&cameraViewMat, tmpMat);


			cubePosition = XMFLOAT4(0.0f, -0.f, 0.0f, 0.0f); // asignamos la posición del cubo 1
			XMVECTOR posVec = XMLoadFloat4(&cubePosition); // Creamos un XM Vector

			tmpMat = XMMatrixTranslationFromVector(posVec); // crear el matrix de translacion
			XMStoreFloat4x4(&cubeRotMat, XMMatrixIdentity()); // inicializar el matrix de rotación
			XMStoreFloat4x4(&cubeWorldMat, tmpMat); // guardar el world matrix del cubo 1

			cube2Pos = XMFLOAT4(1.5f, -1.f, 0.0f, 0.0f); // asignamos la posición del cubo 2
			XMVECTOR pos2Vec = XMLoadFloat4(&cube2Pos); // Creamos un XM Vector

			tmpMat = XMMatrixTranslationFromVector(pos2Vec);  // crear el matrix de translacion
			XMStoreFloat4x4(&cube2RotMat, XMMatrixIdentity()); // inicializar el matrix de rotación
			XMStoreFloat4x4(&cube2WorldMat, tmpMat); // guardar el world matrix del cubo 1
		}

		// Ahora ejecutamos el command list para cargar los assets
		commandList->Close();
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		//Esperamos a que todos los comandos acaben de ejecutarse para continuar
		WaitForGpu();

	}

	void DirectRenderer::OnUpdate()
	{
		XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat); // load view matrix
		XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat); // load projection matrix
		
		newMesh->Update(viewMat, projMat);

		newMesh->rotation.x += 0.001f;
		newMesh->rotation.y += 0.002f;
		newMesh->rotation.z += 0.003f;
	}

	bool DirectRenderer::OnRender()
	{
		// Grabamos todos los comandos
		PopulateCommandList();

		// Ejecutamos los comandos
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Presentamos el frame
		ThrowIfFailed(swapChain->Present(1, 0));

		//Nos movemos al siguiente frame si ya ha acabado todo
		MoveToNextFrame();

		return true;
	}

	void DirectRenderer::Release()
	{
		MoveToNextFrame();

		CloseHandle(fenceEvent);
	}

	void DirectRenderer::OnResize(UINT width, UINT height)
	{
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void DirectRenderer::SetRootSignature(const RootSignature & RootSig)
	{
		if (RootSig.GetSignature() == currRootSignature)
			return;

		commandList->SetGraphicsRootSignature(currRootSignature = RootSig.GetSignature());
	}

	void DirectRenderer::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
	{
		commandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, nullptr);
	}


	void DirectRenderer::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
	{
		commandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
	}

	void DirectRenderer::SetViewport(const D3D12_VIEWPORT & vp)
	{
		commandList->RSSetViewports(1, &vp);
	}

	void DirectRenderer::SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth, FLOAT maxDepth)
	{
		D3D12_VIEWPORT vp;
		vp.Width = w;
		vp.Height = h;
		vp.MinDepth = minDepth;
		vp.MaxDepth = maxDepth;
		vp.TopLeftX = x;
		vp.TopLeftY = y;
		commandList->RSSetViewports(1, &vp);
	}

	void DirectRenderer::SetScissor(const D3D12_RECT & rect)
	{
		ASSERT(rect.left < rect.right && rect.top < rect.bottom);
		commandList->RSSetScissorRects(1, &rect);
	}

	void DirectRenderer::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
	{
		SetScissor(CD3DX12_RECT(left, top, right, bottom));
	}

	void DirectRenderer::SetViewportAndScissor(const D3D12_VIEWPORT & vp, const D3D12_RECT & rect)
	{
		ASSERT(rect.left < rect.right && rect.top < rect.bottom);
		commandList->RSSetViewports(1, &vp);
		commandList->RSSetScissorRects(1, &rect);
	}

	void DirectRenderer::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
	{
		SetViewport((float)x, (float)y, (float)w, (float)h);
		SetScissor(x, y, x + w, y + h);
	}

	void DirectRenderer::SetStencilRef(UINT StencilRef)
	{
		commandList->OMSetStencilRef(StencilRef);
	}

	void DirectRenderer::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
	{
		commandList->IASetPrimitiveTopology(Topology);
	}

	void DirectRenderer::SetPipelineState(const GraphicsPSO & PSO)
	{
		ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
		if (PipelineState == m_CurGraphicsPipelineState)
			return;

		commandList->SetPipelineState(PipelineState);
		m_CurGraphicsPipelineState = PipelineState;
	}

	void DirectRenderer::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV, UINT Offset)
	{
		commandList->SetGraphicsRootConstantBufferView(RootIndex, CBV + Offset);
	}

	void DirectRenderer::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void * BufferData)
	{
		//TODO
	}

	void DirectRenderer::SetBufferSRV(UINT RootIndex, const D3D12_GPU_VIRTUAL_ADDRESS vAddress, UINT Offset)
	{
		commandList->SetGraphicsRootShaderResourceView(RootIndex, vAddress + Offset);
	}

	void DirectRenderer::SetBufferUAV(UINT RootIndex, const D3D12_GPU_VIRTUAL_ADDRESS vAddress, UINT Offset)
	{
		commandList->SetGraphicsRootUnorderedAccessView(RootIndex, vAddress + Offset);
	}

	void DirectRenderer::SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle)
	{
		commandList->SetGraphicsRootDescriptorTable(RootIndex, FirstHandle);
	}

	void DirectRenderer::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW & IBView)
	{
		commandList->IASetIndexBuffer(&IBView);
	}

	void DirectRenderer::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW & VBView)
	{
		SetVertexBuffers(Slot, 1, &VBView);
	}

	void DirectRenderer::SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[])
	{
		commandList->IASetVertexBuffers(StartSlot, Count, VBViews);
	}

	void DirectRenderer::Draw(UINT VertexCount, UINT VertexStartOffset)
	{
		DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
	}

	void DirectRenderer::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
	{
		DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
	}

	void DirectRenderer::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
	{
		commandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	void DirectRenderer::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
	{
		commandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	void DirectRenderer::PopulateCommandList()
	{
		//Reiniciamos el command allocator
		ThrowIfFailed(commandAllocators[frameIndex]->Reset());

		//Reiniciamos el commandlist
		ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr));

		//Asignamos el PSO
		SetPipelineState(pipelineState);
		// Asignamos el root signature
		SetRootSignature(rootSignature);

		// Asignamos el SRV Heap
		ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);


		// Asignamos el descriptor table
		SetDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());

		//Asignamos el viewport y el scissorRect
		SetViewportAndScissor(viewport, scissorRect);

		// Indicamos que el backbuffer se va a usar como render target
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		//Obtenemos los CPU Descriptor handle
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// Asignamos el render target para el output merger stage (the output of the pipeline)
		SetRenderTargets(1, &rtvHandle, dsvHandle);

		// Grabar los comandos
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr); //Limpiamos el canvas

		//Limpiamos el depth stencil
		commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		
		newMesh->Begin();

		newMesh->Draw();

		// Indicamos que el backbuffer va a ser usado para presentar
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		currRootSignature = nullptr;
		m_CurGraphicsPipelineState = nullptr;
		//Finalmente cerramos el command list para dejar de grabar
		ThrowIfFailed(commandList->Close());
	}

	std::vector<UINT8> DirectRenderer::GenerateTextureData()
	{
		const UINT rowPitch = 512 * 4;
		const UINT cellPitch = rowPitch >> 3;
		const UINT cellHeight = 512 >> 3;
		const UINT textureSize = rowPitch * 512;

		std::vector<UINT8> data(textureSize);
		UINT8* pData = &data[0];

		for (UINT n = 0; n < textureSize; n += 4)
		{
			UINT x = n % rowPitch;
			UINT y = n / rowPitch;
			UINT i = x / cellPitch;
			UINT j = y / cellHeight;

			if (i % 2 == j % 2)
			{
				pData[n] = 0x00;		// R
				pData[n + 1] = 0x00;	// G
				pData[n + 2] = 0x00;	// B
				pData[n + 3] = 0xff;	// A
			}
			else
			{
				pData[n] = 0xff;		// R
				pData[n + 1] = 0xff;	// G
				pData[n + 2] = 0xff;	// B
				pData[n + 3] = 0xff;	// A
			}
		}

		return data;
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
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}

		*ppAdapter = adapter.Detach();
	}

	// Preparar para moverse al siguiente frame
	void DirectRenderer::MoveToNextFrame()
	{
		// Preparamos una señal al command list.
		const UINT64 currentFenceValue = fenceValues[frameIndex];
		ThrowIfFailed(commandQueue->Signal(fence.Get(), currentFenceValue));

		// Actualizamos el frame index
		frameIndex = swapChain->GetCurrentBackBufferIndex();

		//Obtenemos el valor de completado
		UINT64 compValue = fence->GetCompletedValue();

		// Si el frame no está listo todavia, esperamos
		if (compValue < fenceValues[frameIndex])
		{
			ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
			WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
		}

		// Asignamos el fence value para el siguiente frame.
		fenceValues[frameIndex] = currentFenceValue + 1;
	}

	void DirectRenderer::WaitForGpu()
	{
		// Preparamos una señal al command list.
		ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValues[frameIndex]));

		// Esperamos hasta que todo se haya procesado
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

		// Asignamos el fence value para el frame actual.
		fenceValues[frameIndex]++;
	}
}