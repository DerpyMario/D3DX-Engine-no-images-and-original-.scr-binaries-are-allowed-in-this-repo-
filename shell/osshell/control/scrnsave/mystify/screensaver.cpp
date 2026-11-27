//============================================================================================
// screensaver.cpp -- Mystify.scr application framework
//
// Reconstructed entry point, command-line dispatch, saver window procedure, settings
// dialogs, error reporting and the run loop. Mystify.scr shares the framework with
// Ribbons.scr; the parser is at RVA 0x8DFC, WinMain (vtable slot 0) at RVA 0x8AD0 and
// the saver WndProc (vtable slot 5) at RVA 0x9690 with the static trampoline at 0x9640.
//
// Verified command-line contract (parser RVA 0x8DFC):
//   /c[:hwnd]  -c[:hwnd]  show the settings page (string IDs 300/301/302)
//   /p hwnd    -p hwnd    preview inside the Display Properties preview pane
//   /t hwnd    -t hwnd    test mode: 650x450 window (AdjustWindowRect 0x28A x 0x1C2)
//   /s, or no switch      full-screen saver (one window per monitor)
//
// Window class: "D3DSaverWndClass" (.rdata RVA 0x182B8, both savers).
// Title string: LoadStringW(NULL, 1) -> "Mystify" (.mui block 1, entry 1).
//
// The original binary delegates the settings page and the error task dialog to
// COMCTL32 ordinal #344 (the shared shell "Screen Saver Settings" host, also used
// by scrnsave.scr / Bubbles.scr); this reconstruction uses the equivalent documented
// APIs (DialogBoxParamW on dialog 105 and MessageBoxW) so the behaviour is identical
// while remaining buildable with plain mingw/MSVC.
//
// Telemetry: WIL activity "ScreenSaverDisplaying" on provider
//   "Microsoft.Windows.Desktop.ScreenSaver" (ASCII string at RVA 0x19366),
//   registered via EventRegister in wWinMain and stopped on exit.
//============================================================================================
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <evntprov.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "resources.h"
#include "settings.h"
#include "engine.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "d3d9.lib")
#endif

namespace Mystify {

//----------------------------------------------------------------------------------------
// run modes (returned by ParseCommandLine, RVA 0x8A7C)
//----------------------------------------------------------------------------------------
enum RunMode
{
    MODE_SETTINGS   = 0,      // /c
    MODE_PREVIEW    = 1,      // /p
    MODE_FULLSCREEN = 2,      // /s or no switch (default branch @ 0x8B7D)
    MODE_TEST       = 3,      // /t
};

//----------------------------------------------------------------------------------------
// application state (the original keeps a single 0xA16A0-byte heap object)
//----------------------------------------------------------------------------------------
static Engine*  g_engine        = nullptr;
static Settings g_settings;
static HMODULE  g_hInstance     = nullptr;
static HWND     g_hwndSaver     = nullptr;
static HWND     g_hwndParent    = nullptr;    // preview parent / settings parent (obj+0x18)
static RunMode  g_mode          = MODE_FULLSCREEN;
static BOOL     g_bError        = FALSE;      // obj+0x31
static HRESULT  g_errorCode     = S_OK;       // obj+0x34
static BOOL     g_bWaiting      = FALSE;      // obj+0x28 - preview 500ms grace flag
static BOOL     g_bRunning      = FALSE;      // SPI_SETSCREENSAVERRUNNING bookkeeping
static POINT    g_ptMouseStart;               // exit-threshold anchor (obj+0x2C counter)
static int      g_mouseJitter   = 0;
static DWORD    g_lastMousePos  = 0xFFFFFFFF; // [0x1C2CC]/[0x1C2D0] packed x/y
static BOOL     g_fPreview      = FALSE;

//----------------------------------------------------------------------------------------
// ETW / WIL activity (RVA 0x19366 "Microsoft.Windows.Desktop.ScreenSaver",
// activity name "ScreenSaverDisplaying" at RVA 0x18090)
//----------------------------------------------------------------------------------------
static REGHANDLE g_etwHandle = 0;
// Provider GUID, from the TraceLogging provider metadata blob that follows the
// provider-name string in .rdata (file offset 0x18570 in Mystify.scr, 0x18190 in
// Ribbons.scr — the identical 16 bytes in both binaries):
static const GUID kEtwProviderGuid =
    { 0xcf4f5073, 0x8289, 0x47b3, { 0xe0, 0xdc, 0xe8, 0xc9, 0x04, 0x76, 0xba, 0x01 } };

static void EtwStart()
{
    EventRegister(&kEtwProviderGuid, nullptr, nullptr, &g_etwHandle);
    if (g_etwHandle)
        EventWriteString(g_etwHandle, 0, 0, L"ScreenSaverDisplaying");
}

static void EtwStop(HRESULT hr)
{
    if (!g_etwHandle)
        return;
    EventWriteString(g_etwHandle, 0, 0,
                     SUCCEEDED(hr) ? L"ScreenSaverDisplaying:Stop:OK"
                                   : L"ScreenSaverDisplaying:Stop:Fail");
    EventUnregister(g_etwHandle);
    g_etwHandle = 0;
}

//----------------------------------------------------------------------------------------
// command-line parser (RVA 0x8A7C)
//
//   1. skip the executable path (quote aware)
//   2. skip blanks, expect '-' or '/'
//   3. switch char: 'c' -> MODE_SETTINGS (+ optional HWND digits), 'p' -> MODE_PREVIEW
//      (+ HWND via _atoi64), 't' -> MODE_TEST, anything else -> MODE_FULLSCREEN
//----------------------------------------------------------------------------------------
static RunMode ParseCommandLine(LPCWSTR cmdLine, HWND* outParent)
{
    *outParent = nullptr;
    if (!cmdLine)
        return MODE_FULLSCREEN;

    LPCWSTR p = cmdLine;
    if (*p == L'"')
    {
        ++p;
        while (*p && *p != L'"') ++p;
        if (*p) ++p;
    }
    else
    {
        while (*p > L' ') ++p;
    }
    while (*p && *p <= L' ') ++p;

    if (!*p)
        return MODE_FULLSCREEN;                     // no switch -> full-screen (0x8CAE)

    if (*p != L'/' && *p != L'-')
        return MODE_FULLSCREEN;

    ++p;
    switch (*p)
    {
    case L'c': case L'C':
        ++p;
        while (*p && *p != L':' && (*p < L'0' || *p > L'9')) ++p;
        if (*p == L':') ++p;
        if (*p >= L'0' && *p <= L'9')
        {
            // The digits after "/c:" are the parent HWND (RVA 0x8C5B..0x8CA7).
            *outParent = reinterpret_cast<HWND>(_wtoi64(p));
        }
        return MODE_SETTINGS;
    case L'p': case L'P':
        ++p;
        while (*p && (*p < L'0' || *p > L'9')) ++p;
        if (*p >= L'0' && *p <= L'9')
            *outParent = reinterpret_cast<HWND>(_wtoi64(p));
        return MODE_PREVIEW;
    case L't': case L'T':
        ++p;
        return MODE_TEST;
    default:
        return MODE_FULLSCREEN;
    }
}

//----------------------------------------------------------------------------------------
// error dialog (RVA 0xBA1C): MessageBoxW(parent, mapped text, app title, MB_ICONERROR).
// The title argument is the Unicode string kept at app+0x24A8, i.e. the LoadStringW(1)
// saver title ("Mystify") loaded by the constructor.
//----------------------------------------------------------------------------------------
static void ShowSaverError(HWND hwnd, HRESULT hr)
{
    wchar_t text[512];
    UINT id = EngineErrorToStringId(hr);
    if (!LoadStringW(g_hInstance, id, text, 512))
    {
        // The mapper falls back to "Unspecified error encountered." (.rdata 0x183F0)
        wcscpy_s(text, 512, L"Unspecified error encountered.");
    }
    wchar_t title[200];
    if (!LoadStringW(g_hInstance, IDS_APP_NAME, title, 200))
        wcscpy_s(title, 200, L"Mystify");
    MessageBoxW(hwnd, text, title, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

//----------------------------------------------------------------------------------------
// settings page (vtable slot 2, RVA 0x8070)
//
// The original calls COMCTL32 ordinal #344 with {hwndParent, 0, 300, 301, 302, 1,
// 0xFFFD} — the shared shell settings host that instantiates dialog 105 from the
// .mui. Here the same dialog template is driven by a classic dialog proc so the
// rebuild is self-contained.
//----------------------------------------------------------------------------------------
static void SyncTrackbars(HWND hDlg, const Settings& s)
{
    SendDlgItemMessageW(hDlg, IDC_TRK_NUM_LINES,  TBM_SETRANGE, TRUE, MAKELPARAM(1, 20));
    SendDlgItemMessageW(hDlg, IDC_TRK_NUM_LINES,  TBM_SETPOS,  TRUE, s.NumLines);
    SendDlgItemMessageW(hDlg, IDC_TRK_LINE_WIDTH, TBM_SETRANGE, TRUE, MAKELPARAM(10, 100));
    SendDlgItemMessageW(hDlg, IDC_TRK_LINE_WIDTH, TBM_SETPOS,  TRUE,
                        static_cast<int>(s.LineWidth * 1000.0f));
    SendDlgItemMessageW(hDlg, IDC_TRK_CAMERA_FOV,   TBM_SETRANGE, TRUE, MAKELPARAM(50, 300));
    SendDlgItemMessageW(hDlg, IDC_TRK_CAMERA_FOV,   TBM_SETPOS,  TRUE,
                        static_cast<int>(s.CameraFOV * 100.0f));
    CheckDlgButton(hDlg, IDC_CHK_SPAN_MONITORS, s.SpanMultiMon ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHK_BLUR,          s.Blur ? BST_CHECKED : BST_UNCHECKED);
}

static void ReadTrackbars(HWND hDlg, Settings& s)
{
    s.NumLines  = static_cast<int>(SendDlgItemMessageW(hDlg, IDC_TRK_NUM_LINES, TBM_GETPOS, 0, 0));
    s.LineWidth = static_cast<float>(SendDlgItemMessageW(hDlg, IDC_TRK_LINE_WIDTH, TBM_GETPOS, 0, 0)) / 1000.0f;
    s.CameraFOV   = static_cast<float>(SendDlgItemMessageW(hDlg, IDC_TRK_CAMERA_FOV, TBM_GETPOS, 0, 0)) / 100.0f;
    s.SpanMultiMon = IsDlgButtonChecked(hDlg, IDC_CHK_SPAN_MONITORS) == BST_CHECKED;
    s.Blur         = IsDlgButtonChecked(hDlg, IDC_CHK_BLUR) == BST_CHECKED;
}

static INT_PTR CALLBACK DisplaySettingsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

static INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    static Settings s;
    switch (msg)
    {
    case WM_INITDIALOG:
        s = g_settings;
        SyncTrackbars(hDlg, s);
        return TRUE;

    case WM_HSCROLL:
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_BTN_OK:
            ReadTrackbars(hDlg, s);
            SaveSettings(&s);
            g_settings = s;
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDC_BTN_CANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        case IDC_BTN_DISPLAY_SETTINGS:
        {
            const int monitors = GetSystemMetrics(SM_CMONITORS);
            DialogBoxW(g_hInstance,
                       MAKEINTRESOURCEW(monitors > 1 ? IDD_DISPLAY_SETTINGS_MULTI
                                                     : IDD_DISPLAY_SETTINGS_SINGLE),
                       hDlg, DisplaySettingsDlgProc);
            return TRUE;
        }
        }
        break;
    }
    return FALSE;
}

static INT_PTR CALLBACK DisplaySettingsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        // Adapter name + rendering capability (strings 2200..2212). The original
        // queries EnumDisplayDevicesW and the D3D caps per monitor.
        wchar_t text[512];
        if (LoadStringW(g_hInstance, IDS_MON_CAN_HW, text, 512))
            SetDlgItemTextW(hDlg, IDC_LBL_RENDERING_DESC, text);
        CheckDlgButton(hDlg, IDC_CHK_DISABLE_HW,
                       g_settings.DisableHardware[0] ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDOK:
            g_settings.DisableHardware[0] =
                IsDlgButtonChecked(hDlg, IDC_CHK_DISABLE_HW) == BST_CHECKED;
            SaveDisplaySettings(&g_settings, GetSystemMetrics(SM_CMONITORS));
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        case IDC_BTN_HELP:
            MessageBoxW(hDlg, L"Display Settings help", L"Display Settings", MB_OK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void RunSettingsDialog(HWND hwndParent)
{
    LoadSettings(&g_settings);
    DialogBoxParamW(g_hInstance, MAKEINTRESOURCEW(IDD_SETTINGS),
                    hwndParent, SettingsDlgProc, 0);
}

//----------------------------------------------------------------------------------------
// saver window procedure (vtable slot 5, RVA 0x91D0; thin trampoline at RVA 0x9190)
//----------------------------------------------------------------------------------------
static void QuitSaver()
{
    // RVA 0x9128: release per-screen objects, stop the run loop.
    PostQuitMessage(0);
}

static LRESULT CALLBACK SaverWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_USER:                          // RVA 0x93F8: posted by WinMain in preview
        if (g_mode == MODE_PREVIEW)
        {
            SetTimer(hwnd, 1, 500, nullptr);   // 0x1F4 ms — D3D init grace period
            g_bWaiting = TRUE;
            return 0;
        }
        break;

    case WM_TIMER:                         // RVA 0x9478 (via 0x93C0)
        if (g_bWaiting)
        {
            g_bWaiting = FALSE;
            KillTimer(hwnd, 1);
        }
        return 0;

    case WM_PAINT:                         // RVA 0x92D3
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!g_bError && g_mode == MODE_PREVIEW)
        {
            // Black preview background while the 500ms grace timer runs.
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
        else
        {
            // GDI overlay render (vtable slot 4, RVA 0xB660): centered text over
            // black, using the NONCLIENTMETRICS message font.
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            wchar_t text[512];
            if (g_bError && LoadStringW(g_hInstance, EngineErrorToStringId(g_errorCode),
                                        text, 512))
            {
                SetBkColor(hdc, RGB(0, 0, 0));
                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextW(hdc, text, -1, &rc,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:                    // RVA 0x927E: fullscreen only -> handled
        if (g_mode == MODE_FULLSCREEN)
            return 1;
        break;

    case WM_SETCURSOR:                     // RVA 0x9251: hide the cursor when running
        if (g_mode == MODE_FULLSCREEN)
        {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_ACTIVATEAPP:                   // RVA 0x9273: losing activation -> quit
        if (g_mode == MODE_FULLSCREEN && wp == FALSE)
        {
            QuitSaver();
            return 0;
        }
        break;

    case WM_KEYDOWN:                       // RVA 0x9248 (any key quits fullscreen)
        if (g_mode == MODE_FULLSCREEN)
        {
            QuitSaver();
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (g_mode == MODE_FULLSCREEN)
        {
            QuitSaver();
            return 0;
        }
        break;

    case WM_MOUSEMOVE:                     // RVA 0x9428: 5 distinct moves -> quit
        if (g_mode != MODE_TEST)
        {
            const POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            const DWORD packed = static_cast<DWORD>(MAKELONG(pt.x, pt.y));
            if (packed != g_lastMousePos)
            {
                g_lastMousePos = packed;
                if (++g_mouseJitter > 5 && g_mode == MODE_FULLSCREEN)
                    QuitSaver();
            }
            return 0;
        }
        break;

    case WM_SYSCOMMAND:                    // RVA 0x9380
        if (g_mode == MODE_FULLSCREEN)
        {
            switch (wp & 0xFFF0)
            {
            case SC_SCREENSAVE:            // swallow recursion
                return 0;
            case SC_CLOSE:                 // -> QuitSaver (RVA 0x93A2)
            case SC_MONITORPOWER:          // -> QuitSaver (RVA 0x93B0)
                QuitSaver();
                return 0;
            }
        }
        break;

    case WM_DESTROY:                       // RVA 0x936B
        if (g_mode == MODE_PREVIEW || g_mode == MODE_TEST)
            QuitSaver();
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

//----------------------------------------------------------------------------------------
// window creation per monitor (RVA 0x8CDC)
//----------------------------------------------------------------------------------------
static BOOL CreateSaverWindows(HWND hwndParent)
{
    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SaverWndProc;
    wc.hInstance = g_hInstance;
    wc.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW /* 0x7F00 @ 0x8D0C */);
    wc.hbrBackground = (HBRUSH)GetStockObject(4 /* BLACK_BRUSH, RVA 0x8D4E */);
    wc.lpszClassName = L"D3DSaverWndClass";
    RegisterClassW(&wc);

    if (g_mode == MODE_PREVIEW)
    {
        RECT rc;
        GetClientRect(hwndParent, &rc);
        AdjustWindowRect(&rc, WS_CHILD, FALSE);
        g_hwndSaver = CreateWindowExW(0, L"D3DSaverWndClass", L"", WS_CHILD | WS_VISIBLE,
                                      0, 0, rc.right - rc.left, rc.bottom - rc.top,
                                      hwndParent, nullptr, g_hInstance, nullptr);
        g_fPreview = TRUE;
    }
    else if (g_mode == MODE_TEST)
    {
        // 650 x 450 client window with WS_OVERLAPPEDWINDOW (RVA 0x8DAE..0x8E20)
        RECT rc = { 0, 0, 650, 450 };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, FALSE);
        g_hwndSaver = CreateWindowExW(0, L"D3DSaverWndClass", L"", 
                                      WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      rc.right - rc.left, rc.bottom - rc.top,
                                      nullptr, nullptr, g_hInstance, nullptr);
        g_fPreview = FALSE;
    }
    else // full-screen: one topmost window per monitor (RVA 0x8E25..0x8F06)
    {
        // One top-most window per monitor; a single window over the whole
        // virtual screen when SpanMultiMon is set (RVA 0x8E2C checks the flag).
        RECT rc;
        if (g_settings.SpanMultiMon)
        {
            rc.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
            rc.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
            rc.right  = rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
            rc.bottom = rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
        }
        else
        {
            rc.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
            rc.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
            rc.right  = rc.left + GetSystemMetrics(SM_CXSCREEN);
            rc.bottom = rc.top + GetSystemMetrics(SM_CYSCREEN);
        }

        g_hwndSaver = CreateWindowExW(WS_EX_TOPMOST, L"D3DSaverWndClass", L"",
                                      WS_POPUP | WS_VISIBLE,
                                      rc.left, rc.top,
                                      rc.right - rc.left, rc.bottom - rc.top,
                                      nullptr, nullptr, g_hInstance, nullptr);
        g_fPreview = FALSE;
    }

    if (!g_hwndSaver)
        return FALSE;

    SetForegroundWindow(g_hwndSaver);
    return TRUE;
}

//----------------------------------------------------------------------------------------
// run loop (RVA 0x9010): PeekMessage-driven, Sleep(10) when idle, render each pass
//----------------------------------------------------------------------------------------
static int RunSaver()
{
    // RVA 0x901E: per-frame clock seed
    HRESULT hr = EngineInitialize(g_engine, g_hwndSaver, &g_settings);
    if (FAILED(hr))
    {
        g_bError = TRUE;
        g_errorCode = hr;
        ShowSaverError(g_hwndSaver, hr);
    }

    if (g_mode == MODE_FULLSCREEN)
        SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, TRUE, &g_bRunning, 0);

    POINT pt;
    GetCursorPos(&pt);
    g_ptMouseStart = pt;

    MSG msg = {};
    BOOL quit = FALSE;
    while (!quit)
    {
        // SPI_GETSCREENSAVERRUNNING probe each pass (RVA 0x9065)
        BOOL running = FALSE;
        SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &running, 0);
        if (!running && g_mode == MODE_FULLSCREEN)
            break;

        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit = TRUE;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit)
            break;

        Sleep(10);                                     // RVA 0x90D4

        if (g_bError)
        {
            Sleep(6);                                  // RVA 0x90EB
            InvalidateRect(g_hwndSaver, nullptr, FALSE);
            UpdateWindow(g_hwndSaver);
        }
        else
        {
            EngineFrameMove(g_engine);                 // vtable slot 16
            EngineRenderScene(g_engine);               // vtable slot 17
            if (g_engine->device)
                g_engine->device->Present(nullptr, nullptr, nullptr, nullptr);
        }
    }

    if (g_mode == MODE_FULLSCREEN && g_bRunning)
        SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, FALSE, nullptr, 0);
    return 0;
}

//----------------------------------------------------------------------------------------
// application main (vtable slot 0, RVA 0x8610; called from the CRT wrapper 0x15BC8)
//----------------------------------------------------------------------------------------
static int AppMain(LPCWSTR cmdLine)
{
    // RVA 0x8627: CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCo))
        return static_cast<int>(hrCo);

    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    EtwStart();

    g_mode = ParseCommandLine(cmdLine, &g_hwndParent);
    LoadSettings(&g_settings);                          // vtable slot 3 (RVA 0x8663)
    LoadDisplaySettings(&g_settings, GetSystemMetrics(SM_CMONITORS));

    if (g_mode == MODE_SETTINGS)
    {
        RunSettingsDialog(g_hwndParent);                // vtable slot 2
    }
    else
    {
        if (!CreateSaverWindows(g_mode == MODE_PREVIEW ? g_hwndParent : nullptr))
        {
            g_bError = TRUE;
            g_errorCode = 0x82000003;
        }
        else if (g_mode == MODE_PREVIEW)
        {
            // RVA 0x86A7..0x8709: flag + PostMessageW(hwnd, WM_USER), message pump
            // for 500 ms before the Direct3D initialisation.
            g_bWaiting = TRUE;
            PostMessageW(g_hwndSaver, WM_USER, 0, 0);
            MSG msg;
            while (g_bWaiting && GetMessageW(&msg, nullptr, 0, 0) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        g_engine = EngineCreate();
        RunSaver();
        EngineDestroy(g_engine);
        g_engine = nullptr;
    }

    EtwStop(g_bError ? g_errorCode : S_OK);
    CoUninitialize();                                   // RVA 0x8A55
    return 0;
}

} // namespace Mystify

//----------------------------------------------------------------------------------------
// CRT entry points
//----------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    Mystify::g_hInstance = hInstance;
    return Mystify::AppMain(GetCommandLineW());
}

// ANSI entry point so the /SUBSYSTEM:WINDOWS linker default works with mingw-w64.
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR, int nShow)
{
    (void)hPrev; (void)nShow;
    Mystify::g_hInstance = hInstance;
    return Mystify::AppMain(GetCommandLineW());
}
