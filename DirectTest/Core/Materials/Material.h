#pragma once
#include "../Core.h"

namespace Renderer {
	class GraphicContext;
}

using namespace Renderer;

class Material {
public:
	Material(GraphicContext* context) : 
		context(context) 
	{

	}
	virtual void BeginRender() = 0;
	virtual void OnRender() = 0;
	virtual void OnEndRender() = 0;

protected:
	GraphicContext* context;
};