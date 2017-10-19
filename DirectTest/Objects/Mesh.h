#pragma once
#include "../Core/RootSignature.h"
#include <wrl\client.h>

struct Vertex;


using namespace Microsoft::WRL;

class Mesh {
public:
	Mesh();
	void SetVertices(Vertex* vertList, UINT numVertices);
	void SetIndices(DWORD* indicesList, UINT numIndices);
	void Instanciate(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

public:
	Vertex* vertexList;
	DWORD* indicesList;
	UINT numVertices;
	UINT numIndices;

	ComPtr<ID3D12Resource> vertexBuffer; //El buffer encargado de cargar los vertices en la GPU
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView; //Una estructura que almacena la información de los vertices
	ComPtr<ID3D12Resource> indexBuffer; // El buffer encargado de cargar los indices en la GPU
	D3D12_INDEX_BUFFER_VIEW indexBufferView; //Una estructura que almacena la información de los indices
	bool instanciated;
};
