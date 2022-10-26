#include "GraphicContext.h"
#include <d3dcompiler.h>

#include "../Components/Mesh.h"
#include "../Materials/StandardMaterial.h"

Mesh* newMesh;
Mesh* newMesh2;
StandardMaterial* mat;

namespace Renderer {
	ComPtr<ID3D12Device> device;
	GraphicContext* renderer;

	GraphicContext::GraphicContext(UINT width, UINT height) :
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

	void GraphicContext::Initialize()
	{
		LoadPipeline();
		LoadAssets();
	}

	void GraphicContext::LoadPipeline()
	{
		UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
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


		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

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

		ThrowIfFailed(factory->MakeWindowAssociation(WinApplication::GetHwnd(), DXGI_MWA_VALID));

		ThrowIfFailed(swapChainTmp.As(&swapChain));

		frameIndex = swapChain->GetCurrentBackBufferIndex();

		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = FRAME_COUNT;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));

			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

			for (UINT n = 0; n < FRAME_COUNT; n++)
			{
				ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));

				device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, rtvDescriptorSize);

				ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
			}
		}
	}

	void GraphicContext::LoadAssets()
	{
		ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), nullptr, IID_PPV_ARGS(&commandList)));

		{
			ThrowIfFailed(device->CreateFence(fenceValues[frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			fenceValues[frameIndex]++;

			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		{
			std::vector<Vertex> vecVList;
			Vertex* vList;
			// Front Face
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f,  0.0f, -1.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },	{  0.0f, 0.0f, -1.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },	{ 0.0f, 0.0f, -1.0f } });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  0.0f,  0.0f, -1.0f } });

			// Right Face
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },	{  1.0f, 0.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } ,		{  1.0f,  0.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  1.0f, 0.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  1.0f,  0.0f, 0.0f} });


			// Left Face
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },		{ -1.0f,  0.0f,  0.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },	{ -1.0f, 0.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ -1.0f, 0.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ -1.0f,  0.0f, 0.0f} });


			// Back Face
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } ,		{  0.0f,  0.0f,  1.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f,  1.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{  0.0f, 0.0f,  1.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ 0.0f,  0.0f,  1.0f } });


			// Upper Face
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f,  1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } ,		{  0.0f,  1.0f,  0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, 0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{  0.0f,  1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, 0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },		{ 0.0f,  1.0f,  0.0f} });


			// Bottom Face
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },		{  0.0f, -1.0f,  0.0f } });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 1.0f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },	{ 0.0f, -1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { 0.25f * aspectRatio, -0.25f * aspectRatio, -0.25f * aspectRatio },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{  0.0f, -1.0f, 0.0f} });
			vecVList.insert(vecVList.end(), { { -0.25f * aspectRatio, -0.25f * aspectRatio, 0.25f * aspectRatio },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } ,	{ 0.0f, -1.0f,  0.0f} });

			vList = vecVList.data();

			DWORD iList[] = {
				// Front Face
				0, 1, 2, 
				0, 3, 1, 

				// Left Face
				4, 5, 6, 
				4, 7, 5, 

				// Right Face
				8, 9, 10, 
				8, 11, 9, 

				// Back Face
				12, 13, 14, 
				12, 15, 13, 

				// Upper Face
				16, 17, 18, 
				16, 19, 17, 

				// Bottom Face
				20, 21, 22, 
				20, 23, 21, 
			};

			int vBufferSize = sizeof(vecVList);
			int iBufferSize = sizeof(iList);
			
			numCubeIndices = sizeof(iList) / sizeof(DWORD);

			newMesh = new Mesh(this, nullptr);
			newMesh->SetVertices(vList, vecVList.size());
			newMesh->SetIndices(iList, sizeof(iList) / sizeof(DWORD));

			newMesh->Initialize(device.Get(), commandList.Get());

			newMesh->pos = XMFLOAT3(0.0f, 0.0f, -2.f);

			newMesh2 = new Mesh(this, nullptr);
			newMesh2->SetVertices(vList, vecVList.size());
			newMesh2->SetIndices(iList, sizeof(iList) / sizeof(DWORD));

			newMesh2->Initialize(device.Get(), commandList.Get());

			newMesh2->pos = XMFLOAT3(2.0f, 0.0f, -2.f);
			newMesh2->scale = XMFLOAT3(.5f, 0.5f, 0.5f);
		}

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

		mat = new StandardMaterial(this);

		{
			XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(60.0f*(3.14f / 180.0f), (float)width / (float)height, 0.1f, 1000.0f);

			XMStoreFloat4x4(&cameraProjMat, tmpMat);

			cameraPosition = XMFLOAT4(0.0f, 0.0f, -4.0f, 0.0f);
			cameraTarget = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			cameraUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

			XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
			XMVECTOR cTarg = XMLoadFloat4(&cameraTarget);
			XMVECTOR cUp = XMLoadFloat4(&cameraUp);

			tmpMat = XMMatrixLookAtLH(cPos, cTarg, cUp);

			XMStoreFloat4x4(&cameraViewMat, tmpMat);
		}

		commandList->Close();
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		WaitForGpu();

	}

	void GraphicContext::OnUpdate()
	{
		XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat); // load view matrix
		XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat); // load projection matrix
		

		newMesh->rotation.x += 0.5f * Time::deltaTime;
		newMesh->rotation.y += 0.2f * Time::deltaTime;
		newMesh->rotation.z += 0.4f * Time::deltaTime;

		newMesh->Update(viewMat, projMat);

		newMesh2->rotation = newMesh->rotation;
		newMesh2->Update(viewMat, projMat);
	}

	bool GraphicContext::OnRender()
	{
		PopulateCommandList();

		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		ThrowIfFailed(swapChain->Present(1, 0));

		MoveToNextFrame();

		return true;
	}

	void GraphicContext::Release()
	{
		MoveToNextFrame();

		CloseHandle(fenceEvent);
	}

	void GraphicContext::OnResize(UINT width, UINT height)
	{
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	}

	void GraphicContext::SetRootSignature(const RootSignature & RootSig)
	{
		if (RootSig.GetSignature() == currRootSignature)
			return;

		commandList->SetGraphicsRootSignature(currRootSignature = RootSig.GetSignature());
	}

	void GraphicContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
	{
		commandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, nullptr);
	}


	void GraphicContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
	{
		commandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
	}

	void GraphicContext::SetViewport(const D3D12_VIEWPORT & vp)
	{
		commandList->RSSetViewports(1, &vp);
	}

	void GraphicContext::SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth, FLOAT maxDepth)
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

	void GraphicContext::SetScissor(const D3D12_RECT & rect)
	{
		ASSERT(rect.left < rect.right && rect.top < rect.bottom);
		commandList->RSSetScissorRects(1, &rect);
	}

	void GraphicContext::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
	{
		SetScissor(CD3DX12_RECT(left, top, right, bottom));
	}

	void GraphicContext::SetViewportAndScissor(const D3D12_VIEWPORT & vp, const D3D12_RECT & rect)
	{
		ASSERT(rect.left < rect.right && rect.top < rect.bottom);
		commandList->RSSetViewports(1, &vp);
		commandList->RSSetScissorRects(1, &rect);
	}

	void GraphicContext::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
	{
		SetViewport((float)x, (float)y, (float)w, (float)h);
		SetScissor(x, y, x + w, y + h);
	}

	void GraphicContext::SetStencilRef(UINT StencilRef)
	{
		commandList->OMSetStencilRef(StencilRef);
	}

	void GraphicContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
	{
		commandList->IASetPrimitiveTopology(Topology);
	}

	void GraphicContext::SetPipelineState(const GraphicsPSO & PSO)
	{
		ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
		if (PipelineState == m_CurGraphicsPipelineState)
			return;

		commandList->SetPipelineState(PipelineState);
		m_CurGraphicsPipelineState = PipelineState;
	}

	void GraphicContext::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV, UINT Offset)
	{
		commandList->SetGraphicsRootConstantBufferView(RootIndex, CBV + Offset);
	}

	void GraphicContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void * BufferData)
	{
		//TODO
	}

	void GraphicContext::SetBufferSRV(UINT RootIndex, const D3D12_GPU_VIRTUAL_ADDRESS vAddress, UINT Offset)
	{
		commandList->SetGraphicsRootShaderResourceView(RootIndex, vAddress + Offset);
	}

	void GraphicContext::SetBufferUAV(UINT RootIndex, const D3D12_GPU_VIRTUAL_ADDRESS vAddress, UINT Offset)
	{
		commandList->SetGraphicsRootUnorderedAccessView(RootIndex, vAddress + Offset);
	}

	void GraphicContext::SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle)
	{
		commandList->SetGraphicsRootDescriptorTable(RootIndex, FirstHandle);
	}

	void GraphicContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW & IBView)
	{
		commandList->IASetIndexBuffer(&IBView);
	}

	void GraphicContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW & VBView)
	{
		SetVertexBuffers(Slot, 1, &VBView);
	}

	void GraphicContext::SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[])
	{
		commandList->IASetVertexBuffers(StartSlot, Count, VBViews);
	}

	void GraphicContext::Draw(UINT VertexCount, UINT VertexStartOffset)
	{
		DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
	}

	void GraphicContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
	{
		DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
	}

	void GraphicContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
	{
		commandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	void GraphicContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
	{
		commandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	void GraphicContext::PopulateCommandList()
	{
		ThrowIfFailed(commandAllocators[frameIndex]->Reset());

		ThrowIfFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr));

		mat->BeginRender();

		SetViewportAndScissor(viewport, scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		SetRenderTargets(1, &rtvHandle, dsvHandle);

		commandList->ClearRenderTargetView(rtvHandle, reinterpret_cast<float*>(&clearColor), 0, nullptr); //Limpiamos el canvas

		commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		
		newMesh->Begin();

		newMesh->Draw();

		newMesh2->Begin();
		newMesh2->Draw();

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		currRootSignature = nullptr;
		m_CurGraphicsPipelineState = nullptr;

		ThrowIfFailed(commandList->Close());
	}

	std::vector<UINT8> GraphicContext::GenerateTextureData()
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

	void GraphicContext::GetHardwareAdapter(IDXGIFactory4 * pFactory, IDXGIAdapter1 ** ppAdapter)
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

	void GraphicContext::MoveToNextFrame()
	{
		const UINT64 currentFenceValue = fenceValues[frameIndex];
		ThrowIfFailed(commandQueue->Signal(fence.Get(), currentFenceValue));

		frameIndex = swapChain->GetCurrentBackBufferIndex();

		UINT64 compValue = fence->GetCompletedValue();

		if (compValue < fenceValues[frameIndex])
		{
			ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
			WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
		}

		fenceValues[frameIndex] = currentFenceValue + 1;
	}

	void GraphicContext::WaitForGpu()
	{
		ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValues[frameIndex]));

		ThrowIfFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent));
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

		fenceValues[frameIndex]++;
	}
}