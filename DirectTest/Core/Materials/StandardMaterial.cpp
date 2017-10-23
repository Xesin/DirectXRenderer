#include "StandardMaterial.h"
#include "../../Renderer/DirectRenderer.h"
#include "../DirectX/PipelineState.h"
#include "../DirectX/RootSignature.h"

UINT8* StandardMaterial::pVertexShaderData = nullptr;
UINT StandardMaterial::vertexShaderDataLength = 0;
UINT8* StandardMaterial::pPixelShaderData = nullptr;
UINT StandardMaterial::pixelShaderDataLength = 0;

using namespace Renderer;

StandardMaterial::StandardMaterial(DirectRenderer * context) : Material(context)
{
	// Describir y crear el shader resource view (SRV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)));

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

	rootSignature.Finalize(L"Standard diffuse", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12 + 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 12 + 8 + 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//Cargamos el vertex shader
	if (vertexShaderDataLength <= 0) {
		ThrowIfFailed(ReadDataFromFile(L"Resources\\ShaderLib\\VertexShader.cso", &pVertexShaderData, &vertexShaderDataLength));
	}
	//Cargamos el pixel shader
	if (pixelShaderDataLength <= 0) {
		ThrowIfFailed(ReadDataFromFile(L"Resources\\ShaderLib\\PixelShader.cso", &pPixelShaderData, &pixelShaderDataLength));
	}
	// Creamos un pipeline state object (PSO)
	CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
	CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
	blendDesc.AlphaToCoverageEnable = FALSE;

	graphicPSO.SetInputLayout(4, inputLayout);
	graphicPSO.SetRootSignature(rootSignature);
	graphicPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength));
	graphicPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength));
	graphicPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	graphicPSO.SetRenderTargetFormat(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
	graphicPSO.SetRasterizerState(rasterDesc);
	graphicPSO.SetBlendState(blendDesc);
	graphicPSO.SetDepthStencilState(depthStencilDesc);
	graphicPSO.Finalize();


	
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
		UpdateSubresources(context->commandList.Get(), textureBuffer, textureUploadHeap.Get(), 0, 0, 1, &textureData);

		context->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		// Describimos y creamos el SRV para la textura
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(textureBuffer, &srvDesc, srvHeap->GetCPUDescriptorHandleForHeapStart());
	}
}

StandardMaterial::~StandardMaterial()
{
	textureBuffer->Release();
	delete textureBufferGPUAddress;
}

void StandardMaterial::BeginRender() {

	//Asignamos el PSO
	context->SetPipelineState(graphicPSO);
	// Asignamos el root signature
	context->SetRootSignature(rootSignature);

	// Asignamos el SRV Heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap.Get() };
	context->commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// Asignamos el descriptor table
	context->SetDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());
}
