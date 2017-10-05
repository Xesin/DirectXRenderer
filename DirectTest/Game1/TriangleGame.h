#pragma once

#include "../Core/EngineState.h"

class TriangleGame : public EngineState
{
public:
	TriangleGame(unsigned int width, unsigned int height, std::wstring name);


	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();
};