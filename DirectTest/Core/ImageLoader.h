#pragma once
#include <wincodec.h>

struct Image {
	UINT textureWidth;
	UINT textureHeight;
	DXGI_FORMAT dxgiFormat;
	UINT sizeInBytes;
	BYTE* imageData;
};

class ImageLoader {

public:
	static int LoadImageFromFile(LPCWSTR filename, int &bytesPerRow, Image* outImage);
	static int GetDXGIFormatBitsPerPixel(DXGI_FORMAT & dxgiFormat);
private:
	static void Initialize();
	static WICPixelFormatGUID GetConvertToWICFormat(WICPixelFormatGUID & wicFormatGUID);
	static DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID & wicFormatGUID);

private:
	static IWICImagingFactory* wicFactory;
};

