#include "Game1/TriangleGame.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	TriangleGame sample(1280, 720, L"Test DirectX");
	return WinApplication::Run(&sample, hInstance, nCmdShow);
}
