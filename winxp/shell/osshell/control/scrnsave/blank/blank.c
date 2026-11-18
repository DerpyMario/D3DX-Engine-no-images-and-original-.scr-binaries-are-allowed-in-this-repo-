/*-----------------------------------------------------------------------------
 * File: blank.c
 *
 * Desc: The "Blank" screen saver (scrnsave.scr, "Default Screen Saver") of
 *       Windows XP Professional x64 Edition / Windows Server 2003 SP1.
 *
 *       It does not use the screen saver library: it is a small program of
 *       its own that covers the screen with a black window.  Older still, it
 *       can hand the work to a "screen saver server" of IWLIB.DLL - when that
 *       DLL is present and the rest of the command line names an existing
 *       file, the server is loaded and asked to blank the screen (/s) or to
 *       show its configuration dialog.
 *
 *       Command line:
 *           /s              blank the screen
 *           /c              show "this screen saver has no options"
 *           /p <hwnd>       preview: a black child window of hwnd
 *
 *       Reconstructed from scrnsave.scr 5.2.3790.1830 (amd64).  The symbol
 *       file of this build is not published: every name in this file is a
 *       choice of the reconstruction, except those of the IWLIB.DLL exports.
 *---------------------------------------------------------------------------*/
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define IDS_APPNAME         100     /* "Blank Screen Saver" */
#define IDS_NOCONFIG        101     /* "This screen saver has no options that you can set." */

/* the mouse has to move this far (|dx| + |dy|, pixels) to end the saver */
#define MOUSE_THRESHOLD     4

/* exports of the screen saver server DLL (they return 16-bit BOOLs) */
typedef SHORT (WINAPI *FINITSCRSAVEPROC)(HINSTANCE hInstance, HWND hwnd);
typedef void  (WINAPI *TERMSCRSAVEPROC)(void);
typedef void  (WINAPI *SCRBLANKPROC)(BOOL fBlank);
typedef void  (WINAPI *SCRSETIGNOREPROC)(BOOL fIgnore);
typedef SHORT (WINAPI *SCRLOADSERVERPROC)(LPCSTR pszServer);
typedef SHORT (WINAPI *SCRSETSERVERPROC)(LPCSTR pszServer);
typedef void  (WINAPI *SCRINVOKEDLGPROC)(HINSTANCE hInstance, HWND hwndParent);

static const char   szServerDLL[] = "IWLIB.DLL";

static HINSTANCE    hMainInstance;
static HWND         hwndParent;         /* owner of the message box (/c) */
static HWND         hwndPreview;        /* parent of the preview window (/p) */
static BOOL         fScreenSave;        /* /s or /p */
static BOOL         fServerLoaded;
static POINT        ptLastCursor;
static UINT         uScrSaveMessage;    /* "SCRSAVE": the server ends the saver */
static char         szAppName[256];
static char         szNoConfig[256];

static HMODULE              hServerDLL;
static FINITSCRSAVEPROC     lpfnFInitScrSave;
static TERMSCRSAVEPROC      lpfnTermScrSave;
static SCRBLANKPROC         lpfnScrBlank;
static SCRSETIGNOREPROC     lpfnScrSetIgnore;
static SCRLOADSERVERPROC    lpfnScrLoadServer;
static SCRSETSERVERPROC     lpfnScrSetServer;
static SCRINVOKEDLGPROC     lpfnScrInvokeDlg;




/*-----------------------------------------------------------------------------
 * Name: LoadServer()
 * Desc: Loads IWLIB.DLL and the screen saver server pszServer, if both
 *       exist.
 *---------------------------------------------------------------------------*/
static BOOL LoadServer(LPCSTR pszServer)
{
    OFSTRUCT of;

    while (*pszServer == ' ')
        pszServer++;

    if (*pszServer == '\0' ||
        OpenFile(szServerDLL, &of, OF_EXIST | OF_SHARE_DENY_NONE) == HFILE_ERROR ||
        OpenFile(pszServer, &of, OF_EXIST | OF_SHARE_DENY_NONE) == HFILE_ERROR)
    {
        return FALSE;
    }

    hServerDLL = LoadLibraryA(szServerDLL);
    if (hServerDLL == NULL)
        return FALSE;

    lpfnFInitScrSave  = (FINITSCRSAVEPROC) GetProcAddress(hServerDLL, "FInitScrSave");
    lpfnTermScrSave   = (TERMSCRSAVEPROC)  GetProcAddress(hServerDLL, "TermScrSave");
    lpfnScrBlank      = (SCRBLANKPROC)     GetProcAddress(hServerDLL, "ScrBlank");
    lpfnScrSetIgnore  = (SCRSETIGNOREPROC) GetProcAddress(hServerDLL, "ScrSetIgnore");
    lpfnScrLoadServer = (SCRLOADSERVERPROC)GetProcAddress(hServerDLL, "ScrLoadServer");
    lpfnScrSetServer  = (SCRSETSERVERPROC) GetProcAddress(hServerDLL, "ScrSetServer");
    lpfnScrInvokeDlg  = (SCRINVOKEDLGPROC) GetProcAddress(hServerDLL, "ScrInvokeDlg");

    if (lpfnFInitScrSave != NULL && lpfnTermScrSave != NULL && lpfnScrBlank != NULL &&
        lpfnScrSetIgnore != NULL && lpfnScrLoadServer != NULL && lpfnScrSetServer != NULL &&
        lpfnScrInvokeDlg != NULL && lpfnFInitScrSave(hMainInstance, NULL))
    {
        if (lpfnScrLoadServer(pszServer))
        {
            uScrSaveMessage = RegisterWindowMessageA("SCRSAVE");
            fServerLoaded = TRUE;
            return TRUE;
        }
        lpfnTermScrSave();
    }

    FreeLibrary(hServerDLL);
    return FALSE;
}




/*-----------------------------------------------------------------------------
 * Name: BlankWndProc()
 * Desc: The black window: any key, click or mouse move ends the saver,
 *       except in the preview.
 *---------------------------------------------------------------------------*/
static LRESULT CALLBACK BlankWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    POINT pt;

    switch (msg)
    {
        case WM_CREATE:
            GetCursorPos(&ptLastCursor);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_ACTIVATE:
        case WM_ACTIVATEAPP:
            if (wParam != 0)
                break;
            /* deactivated */
            /* fall through */
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if (hwndPreview == NULL)
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
            break;

        case WM_SETCURSOR:
            if (hwndPreview == NULL)
            {
                SetCursor(NULL);
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (hwndPreview == NULL)
            {
                GetCursorPos(&pt);
                if (abs(pt.x - ptLastCursor.x) + abs(pt.y - ptLastCursor.y) >= MOUSE_THRESHOLD)
                    PostMessageA(hwnd, WM_CLOSE, 0, 0);
            }
            break;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}




/*-----------------------------------------------------------------------------
 * Name: CreateBlankWindow()
 * Desc: Covers the screen (or the preview window) with a black window.
 *---------------------------------------------------------------------------*/
static BOOL CreateBlankWindow(void)
{
    WNDCLASSA       wc;
    RECT            rc;
    HDC             hdc;
    OSVERSIONINFOA  osvi;

    wc.lpfnWndProc   = BlankWndProc;
    wc.style         = 0;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hMainInstance;
    wc.hIcon         = NULL;
    wc.hCursor       = (hwndPreview == NULL) ? NULL : LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = szAppName;
    wc.lpszMenuName  = NULL;

    if (!RegisterClassA(&wc))
        return FALSE;

    hdc = GetDC(NULL);
    GetClipBox(hdc, &rc);
    ReleaseDC(NULL, hdc);

    /* In a remote session the screen DC may have no visible area: use the
       virtual screen then. */
    if (IsRectEmpty(&rc))
    {
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        if (GetVersionExA(&osvi) && osvi.dwPlatformId == VER_PLATFORM_WIN32_NT &&
            osvi.dwMajorVersion > 4 && GetSystemMetrics(SM_REMOTESESSION))
        {
            rc.left   = 0;
            rc.top    = 0;
            rc.right  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            rc.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        }
    }

    return CreateWindowExA(WS_EX_TOPMOST, szAppName, szAppName,
                           ((hwndPreview == NULL) ? WS_POPUP : WS_CHILD) | WS_VISIBLE,
                           rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                           hwndPreview, NULL, hMainInstance, NULL) != NULL;
}




/*-----------------------------------------------------------------------------
 * Name: InitApplication()
 * Desc: Parses the command line and starts the saver, the server or the
 *       "no options" message.
 *---------------------------------------------------------------------------*/
static BOOL InitApplication(HINSTANCE hInstance, LPSTR pszCmdLine)
{
    LPSTR p;

    LoadStringA(hMainInstance, IDS_APPNAME, szAppName, sizeof(szAppName) - 1);
    LoadStringA(hMainInstance, IDS_NOCONFIG, szNoConfig, sizeof(szNoConfig) - 1);

    /* skip the program name */
    if (*pszCmdLine != '\0')
    {
        p = strchr(pszCmdLine, ' ');
        if (p == NULL)
        {
            pszCmdLine += strlen(pszCmdLine);
        }
        else
        {
            pszCmdLine = p;
            while (*pszCmdLine == ' ')
                pszCmdLine++;
        }
    }

    /* the switches; what is left is the name of a screen saver server */
    for (p = pszCmdLine; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '-')
        {
            if (p[1] == 's' || p[1] == 'S')
                fScreenSave = TRUE;

            if (p[1] == 'c' || p[1] == 'C')
                hwndParent = GetActiveWindow();

            if (p[1] == 'p' || p[1] == 'P')
            {
                fScreenSave = TRUE;
                hwndPreview = (HWND)(LONG_PTR)atoi(p + 2);
                break;
            }

            p[0] = ' ';
            p[1] = ' ';
        }
    }

    if (!LoadServer(pszCmdLine))
    {
        if (!fScreenSave || !CreateBlankWindow())
        {
            MessageBoxA(hwndParent, szNoConfig, szAppName, MB_ICONEXCLAMATION);
            PostQuitMessage(0);
        }
        return TRUE;
    }

    if (fScreenSave)
    {
        lpfnScrSetIgnore(TRUE);
        lpfnScrBlank(TRUE);
        return TRUE;
    }

    lpfnScrInvokeDlg(hMainInstance, hwndParent);
    PostQuitMessage(0);
    return TRUE;
}




/*-----------------------------------------------------------------------------
 * Name: WinMain()
 *---------------------------------------------------------------------------*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG     msg;
    LPSTR   pszCmdLine;
    BOOL    fResult;

    pszCmdLine = GetCommandLineA();
    hMainInstance = GetModuleHandleA(NULL);

    fResult = InitApplication(hMainInstance, pszCmdLine);
    if (!fResult)
        return fResult;

    while (GetMessageA(&msg, NULL, 0, 0) &&
           !(msg.message == uScrSaveMessage && msg.wParam == 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (fServerLoaded)
    {
        lpfnTermScrSave();
        FreeLibrary(hServerDLL);
    }

    return TRUE;
}
