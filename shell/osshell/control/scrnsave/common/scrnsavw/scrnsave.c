/*-----------------------------------------------------------------------------
 * File: scrnsave.c
 *
 * Desc: The classic Win32 screen saver library (scrnsavw.lib, the UNICODE
 *       build of scrnsave.lib) in the form that is linked into the Windows
 *       Vista SP1 logon.scr.  It supplies the program entry point, parses
 *       the screen saver command line, creates the full screen / preview
 *       window and implements DefScreenSaverProc().
 *
 *       The screen saver itself provides ScreenSaverProc() and
 *       ScreenSaverConfigureDialog() (see <scrnsave.h>).
 *
 *       Reconstructed from logon.scr 6.0.6001.18000 (amd64).  Function and
 *       global names are the ones of the symbol file, which also records the
 *       names of the static functions.  The names of the static variables
 *       (fOnWin95, fSwitchL, uShellAutoPlayQueryMessage), of the inlined
 *       helper HogMachine() and of all locals are descriptive choices of
 *       this reconstruction.
 *
 *       Original object:
 *           shell\osshell\control\scrnsave\common\scrnsavw\objfre\amd64\scrnsave.obj
 *
 *       Command line:
 *           (none)          show the configuration dialog (no parent)
 *           /c[:hwnd]       show the configuration dialog, modal to hwnd
 *           /s              run the screen saver full screen
 *           /p <hwnd>       run as a child of hwnd (control panel preview)
 *           /l <hwnd>       same as /p (also sets a flag nothing reads)
 *           /a <hwnd>       change the password (Windows 95 only)
 *---------------------------------------------------------------------------*/
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <regstr.h>
#include <imm.h>
#include <scrnsave.h>

#ifndef WM_POWERBROADCAST
#define WM_POWERBROADCAST               0x0218
#endif
#ifndef PBT_POWERSETTINGCHANGE
#define PBT_POWERSETTINGCHANGE          0x8013
#endif
#ifndef PBTF_APMRESUMEFROMFAILURE
#define PBTF_APMRESUMEFROMFAILURE       0x00000001
#endif
#ifndef SPI_SCREENSAVERRUNNING
#define SPI_SCREENSAVERRUNNING          0x0061
#endif
#ifndef MSGFLT_ADD
#define MSGFLT_ADD                      1
#endif
#ifndef AC_LINE_UNKNOWN
#define AC_LINE_UNKNOWN                 255
#endif
#ifndef HeapEnableTerminationOnCorruption
#define HeapEnableTerminationOnCorruption 1
#endif

/* Structured exception handling is only available with Microsoft compilers. */
#if defined(_MSC_VER)
#define SCRNSAVE_TRY            __try
#define SCRNSAVE_EXCEPT(filter) __except (filter)
#else
#define SCRNSAVE_TRY            if (1)
#define SCRNSAVE_EXCEPT(filter) else
#endif

#define CLASS_SCRNSAVE          TEXT("WindowsScreenSaverClass")
#define TITLE_SCRNSAVE          TEXT("Screen Saver")
#define TITLE_PREVIEW           TEXT("Preview")

#define WAKE_THRESHOLD          4       /* mouse travel (in pixels) that ends the saver */
#define PASSWORD_RECHECK_MS     200     /* minimum time between two password prompts   */

typedef BOOL    (WINAPI *VERIFYPWDPROC)(HWND);
typedef DWORD   (WINAPI *PWDCHANGEPROC)(LPCWSTR, HWND, DWORD, LPVOID);
typedef HIMC    (WINAPI *IMMASSOCPROC)(HWND, HIMC);
typedef BOOL    (WINAPI *HEAPSETINFOPROC)(HANDLE, int, PVOID, SIZE_T);

/*-----------------------------------------------------------------------------
 * Globals (the first three are the documented interface of the library)
 *---------------------------------------------------------------------------*/
HINSTANCE       hMainInstance       = NULL;
HWND            hMainWindow         = NULL;
BOOL            fChildPreview       = FALSE;

BOOL            fClosing            = FALSE;    /* the saver has decided to go away        */
BOOL            fCheckingPassword   = FALSE;    /* inside the password dialog              */
static BOOL     fSwitchL            = FALSE;    /* "/l" was given; written but never read  */

HINSTANCE       hInstPwdDLL         = NULL;
VERIFYPWDPROC   VerifyPassword      = NULL;
static UINT     uShellAutoPlayQueryMessage = 0;
static BOOL     fOnWin95            = FALSE;    /* TRUE if the saver owns the password UI  */

HINSTANCE       hInstImm            = NULL;
IMMASSOCPROC    ImmFnc              = NULL;
HIMC            hPrevImc            = (HIMC)0;

DWORD           dwPasswordDelay     = 0;        /* grace period before a password is asked */
DWORD           dwBlankTime         = 0;        /* tick count at which the saver started   */
POINT           ptMouse;                        /* cursor position when the saver started  */

TCHAR           szPasswordActiveValue[] = REGSTR_VALUE_USESCRPASSWORD;
DWORD           dwWakeThreshold     = 4;        /* kept for compatibility; see WAKE_THRESHOLD */
TCHAR           szPwdDLL[]          = TEXT("PASSWORD.CPL");
BYTE            bACLineStatus       = AC_LINE_UNKNOWN;
CHAR            szFnName[]          = "VerifyScreenSavePwd";
TCHAR           szImmDLL[]          = TEXT("IMM32.DLL");
CHAR            szImmFnc[]          = "ImmAssociateContext";

/*-----------------------------------------------------------------------------
 * Prototypes
 *---------------------------------------------------------------------------*/
INT_PTR PASCAL          WinMainN(HINSTANCE hInst, HINSTANCE hPrev, LPTSTR pszCmdLine, int nCmdShow);
LRESULT CALLBACK        RealScreenSaverProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR          DoScreenSave(HWND hParent);
static BOOL             DoPasswordCheck(HWND hParent);
static void             InitRealScreenSave(void);
void                    UnloadPwdDLL(void);
void                    DummyEntry(void);


/*-----------------------------------------------------------------------------
 * Name: HogMachine()
 * Desc: Tells Windows 95 that a password protected screen saver is running,
 *       which disables Ctrl+Alt+Del and Alt+Tab.
 *---------------------------------------------------------------------------*/
static void HogMachine(BOOL fDisable)
{
    if (fOnWin95)
    {
        UINT uOldValue;

        SystemParametersInfo(SPI_SCREENSAVERRUNNING, fDisable, &uOldValue, 0);
    }
}


/*-----------------------------------------------------------------------------
 * Name: atoui()
 * Desc: Minimal unsigned decimal parser (window handles on the command line).
 *---------------------------------------------------------------------------*/
static UINT_PTR atoui(LPCTSTR psz)
{
    UINT_PTR n = 0;

    while (*psz >= TEXT('0') && *psz <= TEXT('9'))
    {
        n = n * 10 + (*psz - TEXT('0'));
        psz++;
    }

    return n;
}


/*-----------------------------------------------------------------------------
 * Name: WinMainN()
 * Desc: The real WinMain of a screen saver: decodes the command line.
 *---------------------------------------------------------------------------*/
INT_PTR PASCAL WinMainN(HINSTANCE hInst, HINSTANCE hPrev, LPTSTR pszCmdLine, int nCmdShow)
{
    LPTSTR                  pch = pszCmdLine;
    HWND                    hParent;
    OSVERSIONINFO           osvi;
    INITCOMMONCONTROLSEX    icce;
    HEAPSETINFOPROC         pfnHeapSetInformation;

    (void)hPrev;
    (void)nCmdShow;

    /* Terminate on heap corruption, where the system supports it. */
    pfnHeapSetInformation = (HEAPSETINFOPROC)(void *)
        GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "HeapSetInformation");
    if (pfnHeapSetInformation)
        pfnHeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    icce.dwSize = sizeof(icce);
    icce.dwICC  = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icce);

    hMainInstance = hInst;

    /* Only on Windows 95 does the screen saver handle the password itself;
       on Windows NT winlogon takes care of it. */
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    fOnWin95 = (GetVersionEx(&osvi) && osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);

    SCRNSAVE_TRY
    {
        for (;;)
        {
            switch (*pch)
            {
            case TEXT('S'):
            case TEXT('s'):
                return DoScreenSave(NULL);

            case TEXT('L'):
            case TEXT('l'):
                fSwitchL = TRUE;
                /* fall through */

            case TEXT('P'):
            case TEXT('p'):
                do
                    pch++;
                while (*pch == TEXT(' '));          /* skip to the good stuff */

                hParent = NULL;
                while (*pch >= TEXT('0') && *pch <= TEXT('9'))
                {
                    hParent = (HWND)((UINT_PTR)hParent * 10 + (*pch - TEXT('0')));
                    pch++;
                }

                if (hParent && IsWindow(hParent))
                    return DoScreenSave(hParent);
                return -1;

            case TEXT('A'):
            case TEXT('a'):
                if (!fOnWin95)
                    return -1;

                do
                    pch++;
                while (*pch == TEXT(' '));          /* skip to the good stuff */

                hParent = NULL;
                while (*pch >= TEXT('0') && *pch <= TEXT('9'))
                {
                    hParent = (HWND)((UINT_PTR)hParent * 10 + (*pch - TEXT('0')));
                    pch++;
                }

                if (!hParent || !IsWindow(hParent))
                    hParent = GetForegroundWindow();

                /* ScreenSaverChangePassword() */
                {
                    HINSTANCE hMpr = LoadLibrary(TEXT("MPR.DLL"));

                    if (hMpr)
                    {
                        PWDCHANGEPROC pfnChange = (PWDCHANGEPROC)(void *)
                            GetProcAddress(hMpr, "PwdChangePasswordW");

                        if (pfnChange)
                            pfnChange(L"SCRSAVE", hParent, 0, NULL);

                        FreeLibrary(hMpr);
                    }
                }
                return 0;

            case TEXT('C'):
            case TEXT('c'):
                hParent = NULL;
                pch++;
                if (*pch == TEXT(':'))
                    hParent = (HWND)atoui(++pch);

                if (!hParent || !IsWindow(hParent))
                    hParent = GetForegroundWindow();

                return DialogBoxParam(hMainInstance, MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE),
                                      hParent, (DLGPROC)(void *)ScreenSaverConfigureDialog, 0);

            case TEXT('\0'):
                return DialogBoxParam(hMainInstance, MAKEINTRESOURCE(DLG_SCRNSAVECONFIGURE),
                                      NULL, (DLGPROC)(void *)ScreenSaverConfigureDialog, 0);

            case TEXT(' '):
            case TEXT('-'):
            case TEXT('/'):
                pch++;                              /* skip spaces and common switch prefixes */
                break;

            default:
                return -1;
            }
        }
    }
    SCRNSAVE_EXCEPT (UnhandledExceptionFilter(GetExceptionInformation()))
    {
        /* don't leave the machine locked up if the saver crashed */
        HogMachine(FALSE);
    }

    return -1;
}


/*-----------------------------------------------------------------------------
 * Name: DefScreenSaverProc()
 * Desc: Default message handling for the screen saver window: everything the
 *       user does ends the saver.
 *---------------------------------------------------------------------------*/
LRESULT WINAPI DefScreenSaverProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!fChildPreview && !fClosing)
    {
        switch (msg)
        {
        case WM_CLOSE:
            /* Only do the password check on Windows 95; winlogon handles it on NT. */
            if (fOnWin95)
            {
                if (!DoPasswordCheck(hWnd))
                {
                    GetCursorPos(&ptMouse);         /* re-establish the reference position */
                    return 0;
                }
            }
            break;

        case SCRM_VERIFYPW:
            if (fOnWin95)
                return VerifyPassword ? (LRESULT)VerifyPassword(hWnd) : 1;
            break;

        default:
            if (fCheckingPassword)
                break;

            switch (msg)
            {
            case WM_MOUSEMOVE:
            {
                POINT   ptNow;
                int     dx, dy;

                GetCursorPos(&ptNow);

                dx = ptNow.x - ptMouse.x;
                if (dx < 0)
                    dx = -dx;
                dy = ptNow.y - ptMouse.y;
                if (dy < 0)
                    dy = -dy;

                if ((DWORD)(dx + dy) > WAKE_THRESHOLD)
                {
                    PostMessage(hWnd, WM_CLOSE, 0, 0);
                    ptMouse = ptNow;
                }
                break;
            }

            case WM_POWERBROADCAST:
                switch (wParam)
                {
                case PBT_APMQUERYSUSPEND:
                    return TRUE;

                case PBT_APMRESUMECRITICAL:
                case PBT_APMRESUMESUSPEND:
                case PBT_APMRESUMESTANDBY:
                case PBT_APMRESUMEAUTOMATIC:
                    /* a resume after a failed suspend is not user activity */
                    if (!(lParam & PBTF_APMRESUMEFROMFAILURE))
                        PostMessage(hWnd, WM_CLOSE, 0, 0);
                    break;

                case PBT_APMPOWERSTATUSCHANGE:
                case PBT_POWERSETTINGCHANGE:
                    break;

                default:
                    PostMessage(hWnd, WM_CLOSE, 0, 0);
                    break;
                }
                break;

            case WM_POWER:
                /* a critical resume does not generate a WM_POWERBROADCAST */
                if (wParam != PWR_CRITICALRESUME)
                    break;
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                break;

            case WM_ACTIVATEAPP:
                if (wParam)
                    break;
                /* another application came to the foreground */
                /* fall through */

            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                break;

            case WM_SHOWWINDOW:
                if (wParam)
                    SetCursor(NULL);
                break;

            case WM_SETCURSOR:
                SetCursor(NULL);
                return TRUE;
            }
            break;
        }
    }

    /* The shell asks whether AutoPlay may start: wake up, and refuse while a
       password protects the session. */
    if (msg == uShellAutoPlayQueryMessage && msg != 0)
    {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
        return (VerifyPassword != NULL);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


/*-----------------------------------------------------------------------------
 * Name: RealScreenSaverProc()
 * Desc: The window procedure of the screen saver window.  Filters a few
 *       messages and passes the rest on to the saver's ScreenSaverProc().
 *---------------------------------------------------------------------------*/
LRESULT CALLBACK RealScreenSaverProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        /* screen savers do not need an input method editor */
        hInstImm = GetModuleHandle(szImmDLL);
        if (hInstImm)
        {
            ImmFnc = (IMMASSOCPROC)(void *)GetProcAddress(hInstImm, szImmFnc);
            if (ImmFnc)
                hPrevImc = ImmFnc(hWnd, (HIMC)0);
        }

        GetCursorPos(&ptMouse);

        if (!fChildPreview)
            SetCursor(NULL);
        break;

    case WM_DESTROY:
        if (hInstImm && ImmFnc && hPrevImc)
            ImmFnc(hWnd, hPrevImc);

        PostQuitMessage(0);
        break;

    case WM_SETTEXT:
        /* nobody gets to change our title: FindWindow() must keep working */
        return 0;

    case WM_SYSCOMMAND:
        if (!fChildPreview)
        {
            switch (wParam)
            {
            case SC_NEXTWINDOW:         /* no Alt+Tab          */
            case SC_PREVWINDOW:
            case SC_SCREENSAVE:         /* no nested savers    */
                return 0;

            case SC_MONITORPOWER:       /* display powers down */
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                break;
            }
        }
        break;

    case WM_HELP:
    case WM_CONTEXTMENU:
        if (fChildPreview)
        {
            /* let the control panel applet show its own help */
            HWND hParent = GetParent(hWnd);

            if (hParent && IsWindow(hParent))
                PostMessage(hParent, msg, (WPARAM)hParent, lParam);
            return TRUE;
        }
        break;

    case WM_TIMER:
        if (fClosing)
            return 0;
        Sleep(0);
        break;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (fClosing)
            return DefWindowProc(hWnd, msg, wParam, lParam);
        break;

    case WM_PAINT:
        if (fClosing)
            return DefWindowProc(hWnd, msg, wParam, lParam);
        if (!fChildPreview)
            SetCursor(NULL);
        break;
    }

    return ScreenSaverProc(hWnd, msg, wParam, lParam);
}


/*-----------------------------------------------------------------------------
 * Name: DoScreenSave()
 * Desc: Creates the screen saver window (full virtual screen, or a child of
 *       the preview window) and runs the message loop.
 *---------------------------------------------------------------------------*/
static INT_PTR DoScreenSave(HWND hParent)
{
    LPCTSTR     pszWindowTitle;
    DWORD       dwStyle;
    DWORD       dwExStyle = 0;
    int         x = 0, y = 0, cx, cy;
    RECT        rc;
    WNDCLASS    cls;
    MSG         msg;

    cls.hCursor       = NULL;
    cls.hIcon         = LoadIcon(hMainInstance, MAKEINTATOM(ID_APP));
    cls.lpszMenuName  = NULL;
    cls.lpszClassName = CLASS_SCRNSAVE;
    cls.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    cls.hInstance     = hMainInstance;
    cls.style         = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS | CS_OWNDC;
    cls.lpfnWndProc   = RealScreenSaverProc;
    cls.cbWndExtra    = 0;
    cls.cbClsExtra    = 0;

    if (hParent)
    {
        GetClientRect(hParent, &rc);
        cx = rc.right;
        cy = rc.bottom;

        fChildPreview  = TRUE;
        pszWindowTitle = TITLE_PREVIEW;
        dwStyle        = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
    }
    else
    {
        HWND                hOther;
        SYSTEM_POWER_STATUS sps;

        /* cover the whole (multi monitor) desktop */
        x  = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y  = GetSystemMetrics(SM_YVIRTUALSCREEN);
        cx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        if (cx == 0 || cy == 0)
        {
            /* no virtual screen metrics: use the extent of the screen DC */
            HDC hdc = GetDC(NULL);

            GetClipBox(hdc, &rc);
            ReleaseDC(NULL, hdc);

            x  = rc.left;
            y  = rc.top;
            cx = rc.right  - rc.left;
            cy = rc.bottom - rc.top;
        }

        pszWindowTitle = TITLE_SCRNSAVE;
        dwStyle        = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        dwExStyle      = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

        /* only one screen saver at a time */
        hOther = FindWindow(CLASS_SCRNSAVE, TITLE_SCRNSAVE);
        if (hOther && IsWindow(hOther))
        {
            SetForegroundWindow(hOther);
            return 0;
        }

        bACLineStatus = GetSystemPowerStatus(&sps) ? sps.ACLineStatus : AC_LINE_UNKNOWN;

        InitRealScreenSave();
    }

    if (fOnWin95)
    {
        /* the shell asks this before it AutoPlays a newly inserted disc */
        uShellAutoPlayQueryMessage = RegisterWindowMessage(TEXT("QueryCancelAutoPlay"));
        ChangeWindowMessageFilter(uShellAutoPlayQueryMessage, MSGFLT_ADD);
    }
    else
    {
        uShellAutoPlayQueryMessage = 0;
    }

    if (RegisterClass(&cls))
    {
        hMainWindow = CreateWindowEx(dwExStyle, CLASS_SCRNSAVE, pszWindowTitle, dwStyle,
                                     x, y, cx, cy, hParent, NULL, hMainInstance, NULL);
    }

    msg.wParam = 0;

    if (hMainWindow)
    {
        if (!fChildPreview)
            SetForegroundWindow(hMainWindow);

        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnloadPwdDLL();

    return (INT_PTR)msg.wParam;
}


/*-----------------------------------------------------------------------------
 * Name: DoPasswordCheck()
 * Desc: Windows 95 only: asks for the password.  Returns TRUE if the saver
 *       may close.
 *---------------------------------------------------------------------------*/
static BOOL DoPasswordCheck(HWND hParent)
{
    static DWORD    lastcheck = (DWORD)-1;
    DWORD           curtime;
    MSG             msg;

    /* don't reenter, and don't check again once the answer was yes */
    if (fCheckingPassword || fClosing)
        return FALSE;

    if (!VerifyPassword)
    {
        fClosing = TRUE;                            /* no password, just go away */
        return fClosing;
    }

    curtime = GetTickCount();

    /* inside the grace period the saver closes without a password */
    if (dwPasswordDelay && (curtime - dwBlankTime) < dwPasswordDelay)
    {
        fClosing = TRUE;
        return fClosing;
    }

    /* no rapid checking: the dialog was dismissed only a moment ago */
    if (lastcheck != (DWORD)-1 && (curtime - lastcheck) < PASSWORD_RECHECK_MS)
        return fClosing;

    fCheckingPassword = TRUE;

    /* flush pending timer messages so the saver does not draw over the dialog */
    PeekMessage(&msg, hParent, WM_TIMER, WM_TIMER, PM_REMOVE | PM_NOYIELD);
    PeekMessage(&msg, hParent, WM_TIMER, WM_TIMER, PM_REMOVE | PM_NOYIELD);

    /* the saver may override the verification by handling SCRM_VERIFYPW */
    fClosing = (BOOL)SendMessage(hParent, SCRM_VERIFYPW, 0, 0);

    fCheckingPassword = FALSE;

    if (!fClosing)
        SetCursor(NULL);

    lastcheck = GetTickCount();                     /* curtime is stale by now */

    return fClosing;
}


/*-----------------------------------------------------------------------------
 * Name: InitRealScreenSave()
 * Desc: Preparations for a real (full screen) run.  Windows 95 only: loads
 *       the password provider if the user asked for a password protected
 *       screen saver.
 *---------------------------------------------------------------------------*/
static void InitRealScreenSave(void)
{
    HKEY hKey;

    if (!fOnWin95)
        return;

    if (hInstPwdDLL)
        UnloadPwdDLL();

    if (RegOpenKeyEx(HKEY_CURRENT_USER, REGSTR_PATH_SCREENSAVE, 0, KEY_QUERY_VALUE,
                     &hKey) == ERROR_SUCCESS)
    {
        DWORD dwValue;
        DWORD cbValue = sizeof(dwValue);

        if (RegQueryValueEx(hKey, szPasswordActiveValue, NULL, NULL, (LPBYTE)&dwValue,
                            &cbValue) == ERROR_SUCCESS && dwValue)
        {
            hInstPwdDLL = LoadLibrary(szPwdDLL);

            if (hInstPwdDLL)
            {
                VerifyPassword = (VERIFYPWDPROC)(void *)GetProcAddress(hInstPwdDLL, szFnName);

                if (VerifyPassword)
                    HogMachine(TRUE);
                else
                    UnloadPwdDLL();
            }
        }

        RegCloseKey(hKey);
    }
}


/*-----------------------------------------------------------------------------
 * Name: UnloadPwdDLL()
 *---------------------------------------------------------------------------*/
void UnloadPwdDLL(void)
{
    if (!fOnWin95)
        return;

    if (hInstPwdDLL)
    {
        FreeLibrary(hInstPwdDLL);
        hInstPwdDLL = NULL;

        if (VerifyPassword)
        {
            VerifyPassword = NULL;
            HogMachine(FALSE);
        }
    }
}


/*-----------------------------------------------------------------------------
 * Name: DummyEntry()
 * Desc: Finds the arguments in the raw command line and calls WinMainN(), so
 *       that the library does not depend on which C runtime entry point the
 *       screen saver was linked with.
 *---------------------------------------------------------------------------*/
void DummyEntry(void)
{
    INT_PTR     nResult;
    STARTUPINFO si;
    LPTSTR      pszCmdLine = GetCommandLine();

    if (*pszCmdLine == TEXT('\"'))
    {
        /* scan, and skip over, subsequent characters until another
           double-quote or a null is encountered */
        while (*(pszCmdLine = CharNext(pszCmdLine)) && (*pszCmdLine != TEXT('\"')))
            ;

        /* if we stopped on a double-quote (usual case), skip over it */
        if (*pszCmdLine == TEXT('\"'))
            pszCmdLine++;
    }
    else
    {
        while ((USHORT)*pszCmdLine > TEXT(' '))
            pszCmdLine = CharNext(pszCmdLine);
    }

    /* skip past any white space preceding the second token */
    while (*pszCmdLine && ((USHORT)*pszCmdLine <= TEXT(' ')))
        pszCmdLine = CharNext(pszCmdLine);

    si.dwFlags = 0;
    GetStartupInfo(&si);

    nResult = WinMainN(GetModuleHandle(NULL), NULL, pszCmdLine,
                       (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWDEFAULT);

    ExitProcess((UINT)nResult);
}


/*-----------------------------------------------------------------------------
 * Name: main()
 * Desc: The C runtime entry point the screen saver is linked with
 *       (mainCRTStartup calls this).
 *---------------------------------------------------------------------------*/
int __cdecl main(void)
{
    DummyEntry();
    return 0;
}
