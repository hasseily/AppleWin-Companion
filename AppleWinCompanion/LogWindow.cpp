#include "pch.h"
#include "LogWindow.h"
#include "resource.h"
#define LF_FACESIZE 32
#include <Richedit.h>
#include <strsafe.h>

static HINSTANCE appInstance = NULL;
static HWND hwndLog = NULL;				// handle to log window
static HWND hwndEdit = NULL;			// handle to rich edit window

// Register the window class.
const wchar_t CLASS_NAME[] = L"WIndow Log Class";

HWND CreateRichEdit(HWND hwndOwner,        // Dialog box handle.
    int x, int y,          // Location.
    int width, int height, // Dimensions.
    HINSTANCE hinst)       // Application or DLL instance.
{
    LoadLibrary(TEXT("Msftedit.dll"));

    HWND _hwndEdit = CreateWindowEx(0, MSFTEDIT_CLASS, TEXT(""),
        ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VSCROLL,
        x, y, width, height,
        hwndOwner, NULL, hinst, NULL);

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
        DestroyWindow(hwndDlg);
        break;
    case WM_SIZING:
        // when resizing the log window, resize the edit control as well
        RECT rc;
        GetClientRect(hwndDlg, &rc);
        SetWindowPos(hwndEdit, HWND_TOP, rc.left, rc.top, rc.right, rc.bottom, 0);
        break;
    case EN_REQUESTRESIZE:
        REQRESIZE* pReqResize = (REQRESIZE*)lParam;
        SetWindowPos(hwndEdit, HWND_TOP, pReqResize->rc.left, pReqResize->rc.top, pReqResize->rc.right, pReqResize->rc.bottom, 0);
    }
    return DefWindowProc(hwndDlg, message, wParam, lParam);
}

LogWindow::LogWindow(HINSTANCE app, HWND hMainWindow)
{
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
        NULL,       // Menu
        app,        // Instance handle
        NULL        // Additional application data
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
    
    if (hwndLog != 0)
    {
        hwndEdit = CreateRichEdit(hwndLog, 0, 0, 380, 595, app);
        ShowLogWindow(true);
    }
}

void LogWindow::ShowLogWindow(bool bringToFront)
{
    ShowWindow(hwndLog, SW_SHOW);
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
    SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)str.c_str());
    SendMessage(hwndEdit, EM_REQUESTRESIZE, 0, 0);
    SendMessage(hwndEdit, EM_LINESCROLL, 0, UINT16_MAX);
}

void LogWindow::SaveLog()
{
}
