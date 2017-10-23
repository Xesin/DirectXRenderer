#pragma once
#include "../resource.h"
#include <Windows.h>

class EngineState;
class Time;

class WinApplication {
public:
	static int Run(EngineState* engineApp, HINSTANCE hInstance, int nCmdShow);
	static HWND GetHwnd() { return m_hwnd; }

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	static void Update();
	static int InitInstance(HINSTANCE hInstance, int nCmdShow, EngineState* engineApp);
	static ATOM MyRegisterClass(HINSTANCE hInstance);
	static HWND m_hwnd;
	static Time* timer;
};