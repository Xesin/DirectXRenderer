#pragma once
#include "../Core/Core.h"
#include "../Renderer/DirectRenderer.h"
#include <wrl\client.h>

struct Vertex;


using namespace Microsoft::WRL;
namespace Renderer {
	class DirectRenderer;
}

using namespace Renderer;
using namespace DirectX;

class Mesh {
public:
	Mesh(DirectRenderer* context);
	void SetVertices(Vertex* vertList, UINT numVertices);
	void SetIndices(DWORD* indicesList, UINT numIndices);
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void Update(XMMATRIX viewMat, XMMATRIX projectionMat);
	void Begin();
	void Draw();
	void End();

public:
	Vertex* vertexList;
	DWORD* indicesList;
	UINT numVertices;
	UINT numIndices;
	XMFLOAT3 pos;
	XMFLOAT3 scale;
	XMFLOAT3 rotation;

	AppBuffer constBuffer;
	int constBufferAlignedSize = (sizeof(constBuffer) + 255) & ~255; // Necesitamos que el buffer sea de bloques alineados de 256 bytes

	ComPtr<ID3D12Resource> vertexBuffer; //El buffer encargado de cargar los vertices en la GPU
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView; //Una estructura que almacena la información de los vertices
	ComPtr<ID3D12Resource> indexBuffer; // El buffer encargado de cargar los indices en la GPU
	D3D12_INDEX_BUFFER_VIEW indexBufferView; //Una estructura que almacena la información de los indices
	DirectRenderer* context;
	ComPtr<ID3D12DescriptorHeap> cbvHeap; //almacena la posición de nuestro constant buffer view
	ComPtr<ID3D12Resource> constBufferUploadHeap;  //El buffer encargado de cargar el constant buffer a la GPU
	UINT8* constBufferGPUAddress; //Posición de memoria de nuestro Constant Buffer
	bool instanciated;
};
