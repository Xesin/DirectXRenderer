#include "TriangleGame.h"
#include "../Renderer/DirectRenderer.h"

TriangleGame::TriangleGame(unsigned int width, unsigned int height, std::wstring name) :
	EngineState(width, height, name)
{
}

void TriangleGame::OnInit()
{
	renderer = new DirectRenderer(GetWidth(), GetHeight());
	renderer->Initialize();
}

void TriangleGame::OnUpdate()
{
}

void TriangleGame::OnRender()
{
	renderer->OnRender();
}

void TriangleGame::OnDestroy()
{
}
