#include "DirectRenderer.h"
#include "../Core/WinApplication.h"
#include "../Core/DirectXHelper.h"
#include <d3dcompiler.h>
#include "d3dx12.h"



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

// Load the rendering pipeline dependencies.
void DirectRenderer::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

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

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FRAME_COUNT;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChainInt;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		WinApplication::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChainInt
	));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(WinApplication::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChainInt.As(&swapChain));
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FRAME_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

		rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvHeap)));

		// Describe and create a shader resource view (SRV) heap for the texture.
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)));
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV and a command allocator for each frame.
		for (UINT n = 0; n < FRAME_COUNT; n++)
		{
			ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
			device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, rtvDescriptorSize);

			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
		}
	}
}

// Load the sample assets.
void DirectRenderer::LoadAssets()
{
	HRESULT hr;
	// create a root descriptor, which explains where to find the data for this root parameter
	D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
	rootCBVDescriptor.RegisterSpace = 0;
	rootCBVDescriptor.ShaderRegister = 0;

	// create a descriptor range (descriptor table) and fill it out
	// this is a range of descriptors inside a descriptor heap
	D3D12_DESCRIPTOR_RANGE  descriptorTableRanges[1]; // only one range right now
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // this is a range of shader resource views (descriptors)
	descriptorTableRanges[0].NumDescriptors = 1; // we only have one texture right now, so the range is only 1
	descriptorTableRanges[0].BaseShaderRegister = 0; // start index of the shader registers in the range
	descriptorTableRanges[0].RegisterSpace = 0; // space 0. can usually be zero
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

																									   // create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges); // we only have one range
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0]; // the pointer to the beginning of our ranges array

																   // create a root parameter for the root descriptor and fill it out
	D3D12_ROOT_PARAMETER  rootParameters[2]; // two root parameters
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // this is a constant buffer view root descriptor
	rootParameters[0].Descriptor = rootCBVDescriptor; // this is the root descriptor for this root parameter
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // our pixel shader will be the only shader accessing this parameter for now

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
	rootParameters[1].DescriptorTable = descriptorTable; // this is our descriptor table for this root parameter
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), // we have 1 root parameter
		rootParameters, // a pointer to the beginning of our root parameters array
		1,
		&sampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // we can deny shader stages here for better performance
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

	ID3DBlob* signature;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
	if (FAILED(hr))
	{
		return;
	}

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr))
	{
		return;
	}

	UINT8* pVertexShaderData;
	UINT vertexShaderDataLength;
	UINT8* pPixelShaderData;
	UINT pixelShaderDataLength;

	ThrowIfFailed(ReadDataFromFile(L"Resources/VertexShader.cso", &pVertexShaderData, &vertexShaderDataLength));
	ThrowIfFailed(ReadDataFromFile(L"Resources/PixelShader.cso", &pPixelShaderData, &pixelShaderDataLength));

	// create input layout

	// The input layout is used by the Input Assembler so that it knows
	// how to read the vertex data bound to it.

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 12 +8+16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// fill out an input layout description structure
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

	// we can get the number of elements in an array by "sizeof(array) / sizeof(arrayElementType)"
	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	// create a pipeline state object (PSO)

	// In a real application, you will have many pso's. for each different shader
	// or different combinations of shaders, different blend states or different rasterizer states,
	// different topology types (point, line, triangle, patch), or a different number
	// of render targets you will need a pso

	// VS is the only required shader for a pso. You might be wondering when a case would be where
	// you only set the VS. It's possible that you have a pso that only outputs data with the stream
	// output, and not on a render target, which means you would not need anything after the stream
	// output.

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
	psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
	psoDesc.pRootSignature = rootSignature.Get(); // the root signature that describes the input data this pso needs
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the render target
	psoDesc.SampleDesc = sampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
	psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
	psoDesc.NumRenderTargets = 1; // we are only binding one render target
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

								  // create the pso
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));

	// Create the command list.
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), pipelineState.Get(), IID_PPV_ARGS(&commandList)));

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(device->CreateFence(fenceValues[frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		fenceValues[frameIndex]++;

		// Create an event handle to use for frame synchronization.
		fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	// Create vertex buffer
	{
		// a triangle
		Vertex vList[] = {
			// front face (closer to camera)
			{ { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, -1.0f} },
			{ { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, -1.0f } },
			{ { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { -1.0f, -1.0f, -1.0f } },
			{ { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, -1.0f } },

			// right side face
			{ { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, -1.0f} },
			{ { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } , { 1.0f, 1.0f, 1.0f}},
			{ { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f} },
			{ { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, -1.0f} },


			// left side face
			{ { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, 1.0f } },
			{ { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, -1.0f} },
			{ { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } , { -1.0f, -1.0f, 1.0f}},
			{ { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } , { -1.0f, 1.0f, -1.0f}},


			// back face
			{ { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } ,{ 1.0f, 1.0f, 1.0f } },
			{ { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 1.0f} },
			{ { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } , { 1.0f, -1.0f, 1.0f }},
			{ { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } , { -1.0f, 1.0f, 1.0f }},


			// top face
			{ { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, -1.0f} },
			{ { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } , { 1.0f, 1.0f, 1.0f}},
			{ { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, -1.0f} },
			{ { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f} },


			// bottom face
			{ { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },{ 1.0f, -1.0f, 1.0f } },
			{ { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, -1.0f} },
			{ { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } , { 1.0f, -1.0f, -1.0f}},
			{ { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } , { -1.0f, -1.0f, 1.0f}},

		};

		// a triangle
		DWORD iList[] = {
			// ffront face
			0, 1, 2, // first triangle
			0, 3, 1, // second triangle

			// left face
			4, 5, 6, // first triangle
			4, 7, 5, // second triangle

			// right face
			8, 9, 10, // first triangle
			8, 11, 9, // second triangle

			// back face
			12, 13, 14, // first triangle
			12, 15, 13, // second triangle

			// top face
			16, 17, 18, // first triangle
			16, 19, 17, // second triangle

			// bottom face
			20, 21, 22, // first triangle
			20, 23, 21, // second triangle
		};

		int vBufferSize = sizeof(vList);
		int iBufferSize = sizeof(iList);

		numCubeIndices = sizeof(iList) / sizeof(DWORD);

		// create default heap to hold index buffer
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize), // resource description for a buffer
			D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
			nullptr, // optimized clear value must be null for this type of resource
			IID_PPV_ARGS(&indexBuffer));

		// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
		indexBuffer->SetName(L"Index Buffer Resource Heap");

		// create upload heap to upload index buffer
		ID3D12Resource* iBufferUploadHeap;
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
			D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
			nullptr,
			IID_PPV_ARGS(&iBufferUploadHeap));
		iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

		// store vertex buffer in upload heap
		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData = reinterpret_cast<BYTE*>(iList); // pointer to our index array
		indexData.RowPitch = iBufferSize; // size of all our index buffer
		indexData.SlicePitch = iBufferSize; // also the size of our index buffer

		// we are now creating a command with the command list to copy the data from
		// the upload heap to the default heap
		UpdateSubresources<1>(commandList.Get(), indexBuffer.Get(), iBufferUploadHeap, 0, 0, 1, &indexData);

		// transition the vertex buffer data from copy destination state to vertex buffer state
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
		indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
		indexBufferView.SizeInBytes = iBufferSize;

		// create default heap
		// default heap is memory on the GPU. Only the GPU has access to this memory
		// To get data into this heap, we will have to upload the data using
		// an upload heap
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
			D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
											// from the upload heap to this heap
			nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
			IID_PPV_ARGS(&vertexBuffer));

		// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
		vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

		// create upload heap
		// upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
		// We will upload the vertex buffer using this heap to the default heap
		ID3D12Resource* vBufferUploadHeap;
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
			D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
			nullptr,
			IID_PPV_ARGS(&vBufferUploadHeap));
		vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

		// store vertex buffer in upload heap
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = reinterpret_cast<BYTE*>(vList); // pointer to our vertex array
		vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
		vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

											 // we are now creating a command with the command list to copy the data from
											 // the upload heap to the default heap
		UpdateSubresources<1>(commandList.Get(), vertexBuffer.Get(), vBufferUploadHeap, 0, 0, 1, &vertexData);

		// transition the vertex buffer data from copy destination state to vertex buffer state
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = vBufferSize;
	}

	//Create the depth/stencil
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

		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&depthStencilBuffer)
		);
		dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

		device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}

	ComPtr<ID3D12Resource> textureUploadHeap;
	//Create the texture
	{
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = 512;
		textureDesc.Height = 512;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&textureBuffer)));


		UINT64 textureUploadBufferSize;
		// this function gets the size an upload buffer needs to be to upload a texture to the gpu.
		// each row must be 256 byte aligned except for the last row, which can just be the size in bytes of the row
		// eg. textureUploadBufferSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
		//textureUploadBufferSize = (((imageBytesPerRow + 255) & ~255) * (textureDesc.Height - 1)) + imageBytesPerRow;
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

		// now we create an upload heap to upload our texture to the GPU
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
			D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap));
		textureUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		std::vector<UINT8> texture = GenerateTextureData();

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &texture[0];
		textureData.RowPitch = 512 * 4;
		textureData.SlicePitch = textureData.RowPitch * 512;

		UpdateSubresources(commandList.Get(), textureBuffer.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(textureBuffer.Get(), &srvDesc, srvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// Now we execute the command list to upload the initial assets (triangle data)
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//Create the constant buffer
	{
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&constBufferUploadHeap));

		constBufferUploadHeap->SetName(L"Constant Buffer Upload Resource Heap");

		ZeroMemory(&constBuffer, sizeof(AppBuffer));

		XMStoreFloat4x4(&constBuffer.wvpMat, XMMatrixTranspose(XMMatrixIdentity()));
		XMStoreFloat4x4(&constBuffer.worldMat, XMMatrixTranspose(XMMatrixIdentity()));

		CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
		hr = constBufferUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&constBufferGPUAddress));
		memcpy(constBufferGPUAddress, &constBuffer, sizeof(constBuffer));
		memcpy(constBufferGPUAddress + constBufferAlignedSize, &constBuffer, sizeof(constBuffer));
		constBufferUploadHeap->Unmap(0, nullptr);

	}

	//Build projection and view matrix
	{
		XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(60.0f*(3.14f / 180.0f), (float)width / (float)height, 0.1f, 1000.0f);
		XMStoreFloat4x4(&cameraProjMat, tmpMat);
		// set starting camera state
		cameraPosition = XMFLOAT4(0.0f, 0.0f, -4.0f, 0.0f);
		cameraTarget = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		cameraUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

		// build view matrix
		XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
		XMVECTOR cTarg = XMLoadFloat4(&cameraTarget);
		XMVECTOR cUp = XMLoadFloat4(&cameraUp);
		tmpMat = XMMatrixLookAtLH(cPos, cTarg, cUp);
		XMStoreFloat4x4(&cameraViewMat, tmpMat);

		cubePos = XMFLOAT4(0.0f, -1.f, 0.0f, 0.0f); // set cube 1's position
		XMVECTOR posVec = XMLoadFloat4(&cubePos); // create xmvector for cube1's position

		tmpMat = XMMatrixTranslationFromVector(posVec); // create translation matrix from cube1's position vector
		XMStoreFloat4x4(&cubeRotMat, XMMatrixIdentity()); // initialize cube1's rotation matrix to identity matrix
		XMStoreFloat4x4(&cubeWorldMat, tmpMat); // store cube1's world matrix

		cube2Pos = XMFLOAT4(1.5f, -1.f, 0.0f, 0.0f); // set cube 1's position
		XMVECTOR pos2Vec = XMLoadFloat4(&cube2Pos); // create xmvector for cube1's position

		tmpMat = XMMatrixTranslationFromVector(pos2Vec); // create translation matrix from cube1's position vector
		XMStoreFloat4x4(&cube2RotMat, XMMatrixIdentity()); // initialize cube1's rotation matrix to identity matrix
		XMStoreFloat4x4(&cube2WorldMat, tmpMat); // store cube1's world matrix
	}

	// Wait for the command list to execute; we are reusing the same command 
	// list in our main loop but for now, we just want to wait for setup to 
	// complete before continuing.
	WaitForGpu();

}

// Update frame-based values.
void DirectRenderer::OnUpdate()
{
	// update app logic, such as moving the camera or figuring out what objects are in view

	// create rotation matrices
	XMMATRIX rotXMat = XMMatrixRotationX(0.004f);
	XMMATRIX rotYMat = XMMatrixRotationY(0.003f);
	XMMATRIX rotZMat = XMMatrixRotationZ(0.002f);

	// add rotation to cube1's rotation matrix and store it
	XMMATRIX rotMat = XMLoadFloat4x4(&cubeRotMat) * rotXMat * rotYMat * rotZMat;
	XMStoreFloat4x4(&cubeRotMat, rotMat);

	// create translation matrix for cube 1 from cube 1's position vector
	XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cubePos));

	XMMATRIX scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	// create cube1's world matrix by first rotating the cube, then positioning the rotated cube
	XMMATRIX worldMat = rotMat * translationMat * scaleMat;

	// store cube1's world matrix
	XMStoreFloat4x4(&cubeWorldMat, worldMat);

	// update constant buffer for cube1
	// create the wvp matrix and store in constant buffer
	XMMATRIX translationOffsetMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cubePos));
	XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat); // load view matrix
	XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat); // load projection matrix
	XMMATRIX wvpMat = XMLoadFloat4x4(&cubeWorldMat) * viewMat * projMat; // create wvp matrix
	XMMATRIX transposed = XMMatrixTranspose(wvpMat); // must transpose wvp matrix for the gpu
	XMStoreFloat4x4(&constBuffer.wvpMat, transposed); // store transposed wvp matrix in constant buffer

	XMStoreFloat4x4(&constBuffer.worldMat, XMMatrixTranspose(worldMat));

													  // copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(constBufferGPUAddress, &constBuffer, sizeof(constBuffer));

	// now do cube2's world matrix
	// create rotation matrices for cube2
	rotXMat = XMMatrixRotationX(0.002f);
	rotYMat = XMMatrixRotationY(0.003f);
	rotZMat = XMMatrixRotationZ(0.001f);

	// add rotation to cube2's rotation matrix and store it
	rotMat = (XMLoadFloat4x4(&cube2RotMat) * (rotXMat * rotYMat * rotZMat));
	XMStoreFloat4x4(&cube2RotMat, rotMat);

	// create translation matrix for cube 2 to offset it from cube 1 (its position relative to cube1
	translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube2Pos));

	// we want cube 2 to be half the size of cube 1, so we scale it by .5 in all dimensions
	scaleMat = XMMatrixScaling(0.5f, 0.5f, 0.5f);

	// reuse worldMat. 
	// first we scale cube2. scaling happens relative to point 0,0,0, so you will almost always want to scale first
	// then we translate it. 
	// then we rotate it. rotation always rotates around point 0,0,0
	// finally we move it to cube 1's position, which will cause it to rotate around cube 1
	worldMat = rotMat * scaleMat * translationMat;

	wvpMat = XMLoadFloat4x4(&cube2WorldMat) * viewMat * projMat; // create wvp matrix
	transposed = XMMatrixTranspose(wvpMat); // must transpose wvp matrix for the gpu
	XMStoreFloat4x4(&constBuffer.wvpMat, transposed); // store transposed wvp matrix in constant buffer
	
	XMStoreFloat4x4(&constBuffer.worldMat, XMMatrixTranspose(worldMat));

													  // copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(constBufferGPUAddress + constBufferAlignedSize, &constBuffer, sizeof(constBuffer));

	// store cube2's world matrix
	XMStoreFloat4x4(&cube2WorldMat, worldMat);
}

// Render the scene.
bool DirectRenderer::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(swapChain->Present(1, 0));

	MoveToNextFrame();

	return true;
}

void DirectRenderer::Release()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	MoveToNextFrame();

	CloseHandle(fenceEvent);
}

void DirectRenderer::OnResize(UINT width, UINT height)
{
	aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void DirectRenderer::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(commandAllocators[frameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get()));

	// Set necessary state.
	commandList->SetGraphicsRootSignature(rootSignature.Get());

	// set constant buffer descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// set the descriptor table to the descriptor heap (parameter 1, as constant buffer root descriptor is parameter index 0)
	commandList->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());

	// set the root descriptor table 0 to the constant buffer descriptor heap
	//commandList->SetGraphicsRootDescriptorTable(0, cbvHeap->GetGPUDescriptorHandleForHeapStart());

	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	// Indicate that the back buffer will be used as a render target.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
	// get a handle to the depth/stencil buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// set the render target for the output merger stage (the output of the pipeline)
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->SetGraphicsRootConstantBufferView(0, constBufferUploadHeap->GetGPUVirtualAddress());

	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);
	commandList->SetGraphicsRootConstantBufferView(0, constBufferUploadHeap->GetGPUVirtualAddress() + constBufferAlignedSize);
	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

	// Indicate that the back buffer will now be used to present.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(commandList->Close());
}

std::vector<UINT8> DirectRenderer::GenerateTextureData()
{
	const UINT rowPitch = 512 * 4;
	const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
	const UINT cellHeight = 512 >> 3;	// The height of a cell in the checkerboard texture.
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
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}

// Prepare to render the next frame.
void DirectRenderer::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = fenceValues[frameIndex];
	ThrowIfFailed(commandQueue->Signal(fence.Get(), currentFenceValue));

	// Update the frame index.
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	UINT64 compValue = fence->GetCompletedValue();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (compValue < fenceValues[frameIndex])
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	fenceValues[frameIndex] = currentFenceValue + 1;
}

void DirectRenderer::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValues[frameIndex]));

	// Wait until the fence has been processed.
	ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
	WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	fenceValues[frameIndex]++;
}
