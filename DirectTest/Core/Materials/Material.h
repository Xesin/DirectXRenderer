#pragma once
#include "../Core.h"

namespace Renderer {
	class DirectRenderer;
}

using namespace Renderer;

class Material {
public:
	Material(DirectRenderer* context) : 
		context(context) 
	{

	}
	virtual void BeginRender() = 0;
	virtual void OnRender() = 0;
	virtual void OnEndRender() = 0;

protected:
	DirectRenderer* context;
};