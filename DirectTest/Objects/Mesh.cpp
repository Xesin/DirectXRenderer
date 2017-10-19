#include "Mesh.h"
#include "../Core/DirectX/DirectXHelper.h"
#include "../Renderer/DirectRenderer.h"

Mesh::Mesh() :
	vertexBuffer(nullptr),
	indexBuffer(nullptr),
	numVertices(0),
	numIndices(0),
	instanciated(false)
{

}

void Mesh::SetVertices(Vertex* vertList, UINT numVertices) {
	vertexList = vertList;
	this->numVertices = numVertices;
}

void Mesh::SetIndices(DWORD* indicesList, UINT numIndices) {
	this->indicesList = indicesList;
	this->numIndices = numIndices;
}

void Mesh::Instanciate(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	instanciated = true;

	//Tamaño de los buffers
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

	// Podemos ponerle un nombre al buffer y así debugear mejor
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
	indexData.RowPitch = iBufferSize; // tamaño de nuestro array
	indexData.SlicePitch = iBufferSize; // tamaño de nuestro array

										// Ahora creamo el command command list para copiar los datos desde
										// el upload heap al default heap
	UpdateSubresources<1>(commandList, indexBuffer.Get(), iBufferUploadHeap, 0, 0, 1, &indexData);

	// Transición del index buffer desde copia a index buffer state
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

	// Transición del vertex buffer desde copia a vertex buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Crear el vertex buffer view. Obtenemos la posicion de memoria del index buffer mediante el GetGPUVirtualAddress()
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vBufferSize;
}
