
#include "WinApplication.h"
#include "Core.h"
#include "../Renderer/DirectRenderer.h"

#define MAX_LOADSTRING 100

using namespace Renderer;

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

HWND WinApplication::m_hwnd = nullptr;

int WinApplication::Run(EngineState* engineApp, HINSTANCE hInstance , int nCmdShow)
{
	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_DIRECTTEST, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow, engineApp))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DIRECTTEST));

	EngineState* pSample = reinterpret_cast<EngineState*>(GetWindowLongPtr(GetHwnd(), GWLP_USERDATA));
	MSG msg;
	msg.message = WM_NULL;
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			pSample->OnUpdate();
			renderer->OnUpdate();
			pSample->OnRender();
			renderer->OnRender();
		}
	}

	return (int)msg.wParam;
}

int WinApplication::InitInstance(HINSTANCE hInstance, int nCmdShow, EngineState* engineApp)
{
	hInst = hInstance; // Store instance handle in our global variable	

	RECT windowRect = { 0, 0, static_cast<LONG>(engineApp->GetWidth()), static_cast<LONG>(engineApp->GetHeight()) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
	
	m_hwnd = CreateWindow(
		szWindowClass, 
		engineApp->GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr, 
		nullptr, 
		hInstance, 
		engineApp);

	
	if (!m_hwnd)
	{
		return FALSE;
	}
	
	renderer = new DirectRenderer(engineApp->GetWidth(), engineApp->GetHeight());
	renderer->Initialize();

	engineApp->OnInit();

	ShowWindow(m_hwnd, nCmdShow);
	UpdateWindow(m_hwnd);

	return TRUE;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM WinApplication::MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WinApplication::WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIRECTTEST));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DIRECTTEST);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//
LRESULT CALLBACK WinApplication::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	EngineState* pSample = reinterpret_cast<EngineState*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	LRESULT result = 0;

	bool wasHandled = false;

	switch (message)
		{
		case WM_CREATE:
		{
			// Save the EngineState* pointer
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			result = 0;
			wasHandled = true;
		}
		break;

		case WM_KEYDOWN:
			if (pSample)
			{
				pSample->OnKeyDown(static_cast<UINT8>(wParam));
				result = 0;
				wasHandled = true;
			}
			break;

		case WM_KEYUP:
			if (pSample)
			{
				pSample->OnKeyUp(static_cast<UINT8>(wParam));
				result = 0;
				wasHandled = true;
			}
			break;
		case WM_SIZE:
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			renderer->OnResize(width, height);
			result = 0;
			wasHandled = true;
		}
			break;

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			result = 1;
			wasHandled = true;
		}
			break;
	}
	


	if (!wasHandled)
	{
		result = DefWindowProc(hWnd, message, wParam, lParam);
	}


	return result;
}