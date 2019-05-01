#pragma once

#include <d2d1.h>

class OutputManager
{
	ID2D1Factory *factory;
	ID2D1HwndRenderTarget *renderTarget;

public:
	OutputManager() 
	{ 
		factory = nullptr;
		renderTarget = nullptr; 
	}
	~OutputManager() 
	{
		if (factory)
			factory->Release();
		if (renderTarget)
			renderTarget->Release();
	}

	bool Initialize(HWND hWnd)
	{
		if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory) != S_OK)
			return false;
		RECT rect;
		GetClientRect(hWnd, &rect);
		if (factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rect.right, rect.bottom)),
			&renderTarget) != S_OK)
			return false;
		return true;
	}

	void BeginDraw() { renderTarget->BeginDraw(); }
	void EndDraw() { renderTarget->EndDraw(); }
	void DrawImage(int width, int height, IWICBitmapSource* bmpSource)
	{
		printf("%d\n", bmpSource);
		if (bmpSource == nullptr)
			return;

		ID2D1Bitmap* bmp;
		renderTarget->CreateBitmapFromWicBitmap(bmpSource, &bmp);
		D2D_RECT_F sourceRect = { 0, 0, width, height };
		D2D_RECT_F destRect = { 0, 0, renderTarget->GetSize().width, renderTarget->GetSize().height };
		renderTarget->DrawBitmap(bmp, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, sourceRect);
		bmp->Release();
		bmp = nullptr;
	}
	void ClearScene()
	{
		D2D1_COLOR_F color;
		color.r = 1.0f;
		color.b = 1.0f;
		color.g = 0.0f;
		color.a = 1.0f;
		renderTarget->Clear(color);
	}

};