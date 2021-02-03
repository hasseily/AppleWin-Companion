#pragma once
#include <string>

static INT_PTR CALLBACK LogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

class LogWindow
{
public:
	LogWindow::LogWindow(HINSTANCE app, HWND hMainWindow);

	void ShowLogWindow();
	void HideLogWindow();
	bool IsLogWindowDisplayed();
	void ClearLog();
	void AppendLog(std::wstring str);
	void SaveLog();
};

