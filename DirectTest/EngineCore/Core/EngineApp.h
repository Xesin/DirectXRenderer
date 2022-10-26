#pragma once
#include "Common.h"
#include "WinApplication.h"

class EngineApp {
public:
	EngineApp();
	virtual ~EngineApp();

	virtual void OnInit() {}
	virtual void OnUpdate() {}
	virtual void OnRender() {}
	virtual void OnDestroy() {}

	// Samples override the event handlers to handle specific messages.
	virtual void OnKeyDown(UINT8 /*key*/) {}
	virtual void OnKeyUp(UINT8 /*key*/) {}

	// Accessors.
	INLINE const WCHAR* GetTitle() const { return m_title.c_str(); }
	INLINE const void SetTitle(const wchar_t* className) { m_title.assign(className); }

	void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
	std::wstring GetAssetFullPath(LPCWSTR assetName);
	void SetCustomWindowText(LPCWSTR text);

	// Adapter info.
	bool m_useWarpDevice;
	
private:
	
	// Root assets path.
	std::wstring m_assetsPath;

	// Window title.
	std::wstring m_title;
};