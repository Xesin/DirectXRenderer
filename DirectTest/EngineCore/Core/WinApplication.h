#pragma once
#include "../../resource.h"
#include <Windows.h>

class EngineApp;
class Time;

class WinApplication {
public:
	static int Run(EngineApp* engineApp, HINSTANCE hInstance, int nCmdShow, const wchar_t* className);
	static HWND GetHwnd() { return m_hwnd; }

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	static void Update();
	static int InitInstance(HINSTANCE hInstance, int nCmdShow, EngineApp* engineApp);
	static ATOM MyRegisterClass(HINSTANCE hInstance);
	static HWND m_hwnd;
	static Time* timer;
};

#define CREATE_APPLICATION(AppClass) int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) \
{\
	return WinApplication::Run(&AppClass(), hInstance, nCmdShow , L#AppClass);\
}
