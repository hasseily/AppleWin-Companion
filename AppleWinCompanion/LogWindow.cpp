#include "pch.h"
#include "LogWindow.h"
#include "resource.h"
#define LF_FACESIZE 32      // Missing define for Richedit.h
#include <Richedit.h>
#include <strsafe.h>

static HINSTANCE appInstance = nullptr;
static HWND hwndLog = nullptr;				// handle to log window
static HWND hwndEdit = nullptr;			// handle to rich edit window

// Register the window class.
const wchar_t CLASS_NAME[] = L"WIndow Log Class";

HWND CreateRichEdit(HWND hwndOwner,        // Dialog box handle.
    int x, int y,          // Location.
    int width, int height, // Dimensions.
    HINSTANCE hinst)       // Application or DLL instance.
{
    LoadLibrary(TEXT("Msftedit.dll"));

    HWND _hwndEdit = CreateWindowEx(0, MSFTEDIT_CLASS, TEXT(""),
        ES_MULTILINE | ES_NOHIDESEL | ES_AUTOVSCROLL | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VSCROLL,
        x, y, width, height,
        hwndOwner, nullptr, hinst, nullptr);

    return _hwndEdit;
}

INT_PTR CALLBACK LogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    switch (message)
    {
    case WM_INITDIALOG:
        break;
    case WM_NCDESTROY:
        break;
    case WM_CLOSE:
        ShowWindow(hwndDlg, SW_HIDE);
        return 0;   // don't destroy the window
        break;
    case WM_SIZING:
        // when resizing the log window, resize the edit control as well
        RECT rc;
        GetClientRect(hwndDlg, &rc);
        SetWindowPos(hwndEdit, HWND_TOP, rc.left, rc.top, rc.right, rc.bottom, 0);
        break;
    case EN_REQUESTRESIZE:
        auto* pReqResize = (REQRESIZE*)lParam;
        SetWindowPos(hwndEdit, HWND_TOP, pReqResize->rc.left, pReqResize->rc.top, pReqResize->rc.right, pReqResize->rc.bottom, 0);
    }
    return DefWindowProc(hwndDlg, message, wParam, lParam);
}

LogWindow::LogWindow(HINSTANCE app, HWND hMainWindow)
{
    if (hwndLog)
    {
        ShowLogWindow();
        return;
    }
    WNDCLASS wc = { };

    wc.lpfnWndProc = LogProc;
    wc.hInstance = app;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    hwndLog = CreateWindowExW(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Game Log",                    // Window text
        WS_OVERLAPPED | WS_CAPTION |  WS_SYSMENU | WS_THICKFRAME,           // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 600,

        hMainWindow,// Parent window    
        nullptr,       // Menu
        app,        // Instance handle
        nullptr        // Additional application data
    );

    //if (hwndLog == NULL)
    //{
    //    LPVOID lpMsgBuf;
    //    LPVOID lpDisplayBuf;
    //    DWORD dw = GetLastError();

    //    FormatMessage(
    //        FORMAT_MESSAGE_ALLOCATE_BUFFER |
    //        FORMAT_MESSAGE_FROM_SYSTEM |
    //        FORMAT_MESSAGE_IGNORE_INSERTS,
    //        NULL,
    //        dw,
    //        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    //        (LPTSTR)&lpMsgBuf,
    //        0, NULL);

    //    // Display the error message and exit the process

    //    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
    //        (lstrlen((LPCTSTR)lpMsgBuf) + 70) * sizeof(TCHAR));
    //    StringCchPrintf((LPTSTR)lpDisplayBuf,
    //        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
    //        TEXT("CreateDialogW failed with error %d: %s"),
    //        dw, lpMsgBuf);
    //    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
    //}
    
    if (hwndLog != nullptr)
    {
        RECT cR;
        GetClientRect(hwndLog, &cR);
        hwndEdit = CreateRichEdit(hwndLog, cR.left, cR.top, cR.right, cR.bottom, app);
        ShowLogWindow();
    }
}

void LogWindow::ShowLogWindow()
{
    SetForegroundWindow(hwndLog);
    ShowWindow(hwndLog, SW_SHOWNORMAL);
}

void LogWindow::HideLogWindow()
{
    ShowWindow(hwndLog, SW_HIDE);
}

bool LogWindow::IsLogWindowDisplayed()
{
    return false;
}

void LogWindow::ClearLog()
{
}

void LogWindow::AppendLog(std::wstring str)
{
    // the edit window uses ES_AUTOVSCROLL so it will automatically scroll the bottom text
    CHARRANGE selectionRange = { -1, -1 };
    SendMessage(hwndEdit, EM_EXSETSEL, 0, (LPARAM)&selectionRange);     // remove selection
    SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)str.c_str());   // insert new string at end
    SendMessage(hwndEdit, EM_REQUESTRESIZE, 0, 0);                      // resize for text wrapping
}

void LogWindow::SaveLog()
{
}
