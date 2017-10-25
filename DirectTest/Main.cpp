#include "EngineCore\Core\Common.h"
#include "EngineCore\Core\EngineApp.h"

class MyApp : public EngineApp {
public:
	MyApp();

	virtual void OnInit() {} ;
	virtual void OnUpdate() {};
	virtual void OnRender() {};
	virtual void OnDestroy() {};
};
MyApp::MyApp() : EngineApp() {

}

CREATE_APPLICATION(MyApp)