#include "Mesh.h"

using namespace Renderer;
using namespace Microsoft::WRL;

Mesh::Mesh(GraphicContext* context, Material* material) :
	vertexBuffer(nullptr),
	indexBuffer(nullptr),
	numVertices(0),
	numIndices(0),
	instanciated(false),
	context(context),
	material(material)
{
	ZeroMemory(&constBuffer, sizeof(AppBuffer));
	scale = XMFLOAT3(1.f, 1.f, 1.f);
}

void Mesh::SetVertices(Vertex* vertList, UINT numVertices) {
	vertexList = vertList;
	this->numVertices = numVertices;
}

void Mesh::SetIndices(DWORD* indicesList, UINT numIndices) {
	this->indicesList = indicesList;
	this->numIndices = numIndices;
}

void Mesh::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	instanciated = true;

	//Tama�o de los buffers
	int vBufferSize = sizeof(Vertex) * numVertices;
	int iBufferSize = sizeof(DWORD) * numIndices;

	// Crear el index buffer
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indexBuffer));

	// Podemos ponerle un nombre al buffer y as� debugear mejor
	indexBuffer->SetName(L"Index Buffer Resource Heap");

	// Crear un upload heap para cargar el index buffer
	ID3D12Resource* iBufferUploadHeap;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, // La GPU leera de este buffer y copiara los datos al otro buffer
		nullptr,
		IID_PPV_ARGS(&iBufferUploadHeap));

	iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	// Almacenar los datos del index buffer
	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = reinterpret_cast<BYTE*>(indicesList); // puntero a nuestro array
	indexData.RowPitch = iBufferSize; // tama�o de nuestro array
	indexData.SlicePitch = iBufferSize; // tama�o de nuestro array

										// Ahora creamo el command command list para copiar los datos desde
										// el upload heap al default heap
	UpdateSubresources<1>(commandList, indexBuffer.Get(), iBufferUploadHeap, 0, 0, 1, &indexData);

	// Transici�n del index buffer desde copia a index buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	// Crear el index buffer view. Obtenemos la posicion de memoria del index buffer mediante el GetGPUVirtualAddress()
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes = iBufferSize;

	// Crear el default heap para el vertex buffer
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer));

	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	// Crear el upload heap para el vertex buffer
	ID3D12Resource* vBufferUploadHeap;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vBufferUploadHeap));

	vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// Guardamos el vertex buffer en el upload heap
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<BYTE*>(vertexList);
	vertexData.RowPitch = vBufferSize;
	vertexData.SlicePitch = vBufferSize;

	//Copiamos los datos desde el upload heap al default heap
	UpdateSubresources<1>(commandList, vertexBuffer.Get(), vBufferUploadHeap, 0, 0, 1, &vertexData);

	// Transici�n del vertex buffer desde copia a vertex buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Crear el vertex buffer view. Obtenemos la posicion de memoria del index buffer mediante el GetGPUVirtualAddress()
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vBufferSize;

	// Describir y crear el constant buffer view (CBV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&cbvHeap)));

	//Creamos el constant buffer
	{

		//Como es un dato que se actualiza constantemente, solo creamos un upload heap
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // Tama�o del buffer.Tiene que ser un multiplo de 64KB para una textura o constant buffer
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constBufferUploadHeap));

		constBufferUploadHeap->SetName(L"Constant Buffer Upload Resource Heap");

		ZeroMemory(&constBuffer, sizeof(AppBuffer));

		//Asignamos los datos
		XMStoreFloat4x4(&constBuffer.wvpMat, XMMatrixTranspose(XMMatrixIdentity()));
		XMStoreFloat4x4(&constBuffer.worldMat, XMMatrixTranspose(XMMatrixIdentity()));

		CD3DX12_RANGE readRange(0, 0);    // No tenemos intencion de leer estos datos desde la CPU.

										  //Mapeamos para obtener la direcci�n de memoria del buffer en la GPU
		ThrowIfFailed(constBufferUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&constBufferGPUAddress)));

		//Copiamos los datos a esa direcci�n de memoria
		memcpy(constBufferGPUAddress, &constBuffer, sizeof(constBuffer));
		constBufferUploadHeap->Unmap(0, nullptr);
	}
}

void Mesh::Update(XMMATRIX viewMat, XMMATRIX projectionMat)
{
	XMMATRIX tmp = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&rotation));
	// A�adimos la rotacion a la que ya tenia el cubo 1
	XMMATRIX rotMat = tmp;

	//Crear el matrix de traslacion a partir de la posicion del cubo 1
	XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat3(&pos));

	//Crear un matrix de escala para el cubo 1

	XMMATRIX scaleMat = XMMatrixScalingFromVector(XMLoadFloat3(&scale));

	// Creamos el world matrix para el cubo 1, primero realizamos la rotaci�n para que sea en base al centro del objeto
	XMMATRIX worldMat = rotMat * translationMat * scaleMat;

	XMMATRIX wvpMat = worldMat * viewMat * projectionMat; // crear wvp matrix
	XMMATRIX transposed = XMMatrixTranspose(wvpMat); // se debe pasar la transpuesta del wvp matrix para la gpu
	XMStoreFloat4x4(&constBuffer.wvpMat, transposed); // guardamos el mvp matrix

															 //Guardamos el world matrix en el constant buffer
	XMStoreFloat4x4(&constBuffer.worldMat, XMMatrixTranspose(worldMat));

	//copiamos los datos a la GPU
	memcpy(constBufferGPUAddress, &constBuffer, sizeof(constBuffer));
}

void Mesh::Begin()
{
	context->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->SetVertexBuffer(0, vertexBufferView);
	context->SetIndexBuffer(indexBufferView);
	context->SetConstantBuffer(0, constBufferUploadHeap->GetGPUVirtualAddress());
}

void Mesh::Draw()
{
	context->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

void Mesh::End()
{
}
