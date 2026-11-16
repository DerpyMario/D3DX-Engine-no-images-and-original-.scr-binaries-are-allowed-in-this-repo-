//-----------------------------------------------------------------------------
// File: logon.cpp
//
// Desc: The "Windows Logo" screen saver (logon.scr) of Windows Vista SP1.
//
//       A small layered window drifts over a black screen.  It alternately
//       shows a lens flare animation (45 bitmaps) and the Windows logo of the
//       installed edition (loaded through the branding DLL), fading each of
//       them in and out.  On battery power and on server editions only the
//       logo is shown, and it is not faded.
//
//       The program entry point, the command line handling and the screen
//       saver window come from the classic screen saver library, see
//       ..\common\scrnsavw\scrnsave.c.
//
//       Reconstructed from logon.scr 6.0.6001.18000 (amd64).  Function and
//       global names are the ones of the public symbols; the MODE enumerators,
//       the constants and the locals are named by this reconstruction.
//
//       Original object:
//           shell\osshell\control\scrnsave\logon\objfre\amd64\logon.obj
//-----------------------------------------------------------------------------
#include <initguid.h>           // instantiate GUID_ACDC_POWER_SOURCE / GUID_MONITOR_POWER_ON
#include <windows.h>
#include <commctrl.h>
#include <scrnsave.h>
#include <strsafe.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "winbrand.h"
#include "resource.h"

#ifndef PBT_POWERSETTINGCHANGE
#define PBT_POWERSETTINGCHANGE  0x8013
#endif

//-----------------------------------------------------------------------------
// The state machine.  One "tic" is one WM_PAINT of the logo window, which a
// 17 ms timer requests (roughly 60 per second).
//-----------------------------------------------------------------------------
enum MODE
{
    MODE_BLANK = 0,             // nothing visible
    MODE_FLAREFADEIN,           // first flare frame fades in at a new position
    MODE_FLAREANIM,             // the flare animation plays
    MODE_FLAREFADEOUT,          // last flare frame fades out
    MODE_LOGOFADEIN,            // logo fades in at a new position
    MODE_LOGOANIM,              // logo stays
    MODE_LOGOFADEOUT            // logo fades out
};

#define NUM_FLARE_FRAMES        45

#define LOGON_TIMER_ID          1
#define LOGON_TIMER_INTERVAL    17      // ms

// Size of the two pictures in the little preview monitor of the control panel
#define PREVIEW_FLARE_WIDTH     69
#define PREVIEW_FLARE_HEIGHT    36
#define PREVIEW_LOGO_WIDTH      70
#define PREVIEW_LOGO_HEIGHT     42

#define BLUR_MAX_WIDTH          1024    // size of the scan line buffer of Blur()

// Posted to the screen saver window once the logo window exists
#define WM_SHOWLOGON            WM_USER

// Bitmap resource of shellbrd.dll: the Windows logo of the installed edition
#define IDB_BRANDING_LOGO       1000

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
int         g_nBlankTics        = 60;
int         g_nFlareFadeInTics  = 20;
int         g_nFlareAnimTics    = 40;
int         g_nFlareFadeOutTics = 60;
int         g_nLogoFadeInTics   = 20;
int         g_nLogoAnimTics     = 40;
int         g_nLogoFadeOutTics  = 60;
float       g_flarePercentage   = 0.4f;     // chance that a flare follows a flare
MODE        g_mode              = MODE_FLAREFADEIN;
int         g_tics              = -1;       // tics spent in g_mode

MODE        g_prevMode;
HBITMAP     g_hbmFlare[NUM_FLARE_FRAMES];
HBITMAP     g_hbmLogo;
HDC         g_hdcLogon;                     // memory DC the pictures are selected into
HWND        g_hwndLogon;                    // the window that shows them
HINSTANCE   g_hInst;                        // never assigned: stays NULL
HBRUSH      g_hbrBlack;
int         g_cxhwndLogon;                  // current size of g_hwndLogon
int         g_cyhwndLogon;
int         g_logoWidth;
int         g_logoHeight;
int         g_flareWidth;
int         g_flareHeight;
int         g_xScreen;                      // the screen saver window (virtual desktop)
int         g_yScreen;
int         g_cxScreen;
int         g_cyScreen;
bool        g_fServer;                      // server edition: logo only
bool        g_fBatteries;                   // running on batteries: logo only
HPOWERNOTIFY g_hPowerSrc;
HPOWERNOTIFY g_hMonitorOn;


//-----------------------------------------------------------------------------
// Name: SetMode()
// Desc: Switches the state machine to another mode.
//-----------------------------------------------------------------------------
static inline void SetMode(MODE mode)
{
    g_prevMode = g_mode;
    g_mode     = mode;
    g_tics     = -1;
}


//-----------------------------------------------------------------------------
// Name: MoveLogonWindow()
// Desc: Puts the logo window at a random position of the screen and gives it
//       the size of the picture that is about to be shown.
//-----------------------------------------------------------------------------
static inline void MoveLogonWindow()
{
    int x = rand() % (g_cxScreen - g_cxhwndLogon) + g_xScreen;
    int y = rand() % (g_cyScreen - g_cyhwndLogon) + g_yScreen;

    SetWindowPos(g_hwndLogon, NULL, x, y, g_cxhwndLogon, g_cyhwndLogon, SWP_NOZORDER);
}


//-----------------------------------------------------------------------------
// Name: PaintBitmap()
// Desc: Draws one picture into the logo window.
//-----------------------------------------------------------------------------
static inline void PaintBitmap(HWND hwnd, HBITMAP hbm)
{
    PAINTSTRUCT ps;

    BeginPaint(hwnd, &ps);
    SelectObject(g_hdcLogon, hbm);
    BitBlt(ps.hdc, 0, 0, g_cxhwndLogon, g_cyhwndLogon, g_hdcLogon, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}


//-----------------------------------------------------------------------------
// Name: BlankOnPaint()
// Desc: Waits, then decides what to show next and sizes the window for it.
//-----------------------------------------------------------------------------
void BlankOnPaint(HWND hwnd)
{
    if (g_tics > g_nBlankTics)
    {
        // A flare always follows the logo; after a flare the logo comes next
        // in (1 - g_flarePercentage) of the cases.
        if (g_fBatteries || g_fServer ||
            (g_prevMode == MODE_FLAREFADEOUT && (float)rand() / (RAND_MAX + 1) >= g_flarePercentage))
        {
            SetMode(MODE_LOGOFADEIN);
        }
        else
        {
            SetMode(MODE_FLAREFADEIN);
        }

        if (g_mode == MODE_FLAREFADEIN)
        {
            g_cxhwndLogon = fChildPreview ? PREVIEW_FLARE_WIDTH  : g_flareWidth;
            g_cyhwndLogon = fChildPreview ? PREVIEW_FLARE_HEIGHT : g_flareHeight;
        }
        else
        {
            g_cxhwndLogon = fChildPreview ? PREVIEW_LOGO_WIDTH  : g_logoWidth;
            g_cyhwndLogon = fChildPreview ? PREVIEW_LOGO_HEIGHT : g_logoHeight;
        }
    }

    ValidateRect(hwnd, NULL);
}


//-----------------------------------------------------------------------------
// Name: FlareFadeInOnPaint()
//-----------------------------------------------------------------------------
void FlareFadeInOnPaint(HWND hwnd)
{
    if (g_tics == 0)
        MoveLogonWindow();

    SetLayeredWindowAttributes(g_hwndLogon, 0, (BYTE)(g_tics * 255 / g_nFlareFadeInTics), LWA_ALPHA);

    PaintBitmap(hwnd, g_hbmFlare[0]);

    if (g_tics >= g_nFlareFadeInTics)
        SetMode(MODE_FLAREANIM);
}


//-----------------------------------------------------------------------------
// Name: FlareAnimOnPaint()
//-----------------------------------------------------------------------------
void FlareAnimOnPaint(HWND hwnd)
{
    PaintBitmap(hwnd, g_hbmFlare[g_tics * (NUM_FLARE_FRAMES - 1) / g_nFlareAnimTics]);

    if (g_tics >= g_nFlareAnimTics)
        SetMode(MODE_FLAREFADEOUT);
}


//-----------------------------------------------------------------------------
// Name: FlareFadeOutOnPaint()
//-----------------------------------------------------------------------------
void FlareFadeOutOnPaint(HWND hwnd)
{
    SetLayeredWindowAttributes(g_hwndLogon, 0, (BYTE)(255 - g_tics * 255 / g_nFlareFadeOutTics), LWA_ALPHA);

    PaintBitmap(hwnd, g_hbmFlare[NUM_FLARE_FRAMES - 1]);

    if (g_tics >= g_nFlareFadeOutTics)
        SetMode(MODE_BLANK);
}


//-----------------------------------------------------------------------------
// Name: LogoFadeInOnPaint()
//-----------------------------------------------------------------------------
void LogoFadeInOnPaint(HWND hwnd)
{
    if (g_tics == 0)
        MoveLogonWindow();

    // no fading on batteries or on a server: jump to the end of the fade
    if (g_fBatteries || g_fServer)
        g_tics = g_nLogoFadeInTics;

    SetLayeredWindowAttributes(g_hwndLogon, 0, (BYTE)(g_tics * 255 / g_nLogoFadeInTics), LWA_ALPHA);

    PaintBitmap(hwnd, g_hbmLogo);

    if (g_tics >= g_nLogoFadeInTics)
        SetMode(MODE_LOGOANIM);
}


//-----------------------------------------------------------------------------
// Name: LogoFadeOutOnPaint()
//-----------------------------------------------------------------------------
void LogoFadeOutOnPaint(HWND hwnd)
{
    SetLayeredWindowAttributes(g_hwndLogon, 0, (BYTE)(255 - g_tics * 255 / g_nLogoFadeOutTics), LWA_ALPHA);

    PaintBitmap(hwnd, g_hbmLogo);

    if (g_tics >= g_nLogoFadeOutTics)
        SetMode(MODE_BLANK);
}


//-----------------------------------------------------------------------------
// Name: OnPaint()
// Desc: WM_PAINT of the logo window: advances the state machine by one tic.
//-----------------------------------------------------------------------------
void OnPaint(HWND hwnd)
{
    g_tics++;

    switch (g_mode)
    {
    case MODE_BLANK:
        BlankOnPaint(hwnd);
        break;

    case MODE_FLAREFADEIN:
        FlareFadeInOnPaint(hwnd);
        break;

    case MODE_FLAREANIM:
        FlareAnimOnPaint(hwnd);
        break;

    case MODE_FLAREFADEOUT:
        FlareFadeOutOnPaint(hwnd);
        break;

    case MODE_LOGOFADEIN:
        LogoFadeInOnPaint(hwnd);
        break;

    case MODE_LOGOANIM:
        // the logo is on screen already, there is nothing to draw
        if (g_tics > g_nLogoAnimTics)
            SetMode((g_fBatteries || g_fServer) ? MODE_BLANK : MODE_LOGOFADEOUT);

        ValidateRect(hwnd, NULL);
        break;

    case MODE_LOGOFADEOUT:
        LogoFadeOutOnPaint(hwnd);
        break;
    }
}


//-----------------------------------------------------------------------------
// Name: WndProc()
// Desc: Window procedure of the logo window.
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SETFOCUS:
        return 1;

    case WM_PAINT:
        OnPaint(hwnd);
        break;

    case WM_CLOSE:
        if (g_hPowerSrc)
        {
            UnregisterPowerSettingNotification(g_hPowerSrc);
            g_hPowerSrc = NULL;
        }
        if (g_hMonitorOn)
        {
            UnregisterPowerSettingNotification(g_hMonitorOn);
            g_hMonitorOn = NULL;
        }
        ExitProcess(0);
        break;

    case WM_TIMER:
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_POWERBROADCAST:
        if (wParam == PBT_POWERSETTINGCHANGE)
        {
            POWERBROADCAST_SETTING* pSetting = (POWERBROADCAST_SETTING*)lParam;

            if (memcmp(&pSetting->PowerSetting, &GUID_MONITOR_POWER_ON, sizeof(GUID)) == 0)
            {
                // the display was switched off: no point in animating any longer
                if (pSetting->Data[0] == 0)
                    PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            else if (memcmp(&pSetting->PowerSetting, &GUID_ACDC_POWER_SOURCE, sizeof(GUID)) == 0)
            {
                // 0 = AC, 1 = DC, 2 = short term source (UPS)
                g_fBatteries = (pSetting->Data[0] != 0);
            }
        }
        break;

    default:
        return DefScreenSaverProc(hwnd, uMsg, wParam, lParam) != 0;
    }

    return 0;
}


//-----------------------------------------------------------------------------
// Name: Blur()
// Desc: Prepares the logo for the tiny preview window: blurs a 16 bit (5-5-5)
//       DIB section with a 7 pixel box filter, horizontally and vertically,
//       and then shrinks it to a quarter of its size by dropping pixels.  The
//       result ends up in the upper left corner of the (bottom-up) bitmap.
//
//       The scan line buffer is also used for the columns, so the height of
//       the bitmap must not exceed BLUR_MAX_WIDTH either - the original does
//       not check this.
//-----------------------------------------------------------------------------
#define RED555(w)       (((w) >> 10) & 0x1f)
#define GREEN555(w)     (((w) >> 5) & 0x1f)
#define BLUE555(w)      ((w) & 0x1f)
#define PACK555(r, g, b) ((WORD)((((r) / 7) * 32 + ((g) / 7)) * 32 + ((b) / 7)))

void Blur(HWND hwnd, HBITMAP hbm)
{
    BITMAP  bm;
    WORD    line[BLUR_MAX_WIDTH];
    WORD*   pBits;
    WORD    w;
    int     x, y;
    int     r, g, b;

    (void)hwnd;

    if (!GetObject(hbm, sizeof(bm), &bm))
        return;

    if (bm.bmBitsPixel / 8 != 2 || bm.bmWidth > BLUR_MAX_WIDTH || bm.bmWidth <= 6)
        return;

    pBits = (WORD*)bm.bmBits;

    // Blur the rows.  A running sum of seven pixels moves along the row; at
    // the borders it holds fewer pixels but is still divided by seven, which
    // darkens the edges.
    for (y = 0; y < bm.bmHeight; y++)
    {
        WORD* pRow = pBits + (bm.bmWidthBytes * y) / 2;

        r = g = b = 0;

        for (x = 0; x < 3; x++)
        {
            w = pRow[x];
            r += RED555(w);
            g += GREEN555(w);
            b += BLUE555(w);
        }

        for (x = 0; x < 3; x++)
        {
            w = pRow[x + 3];
            r += RED555(w);
            g += GREEN555(w);
            b += BLUE555(w);
            line[x] = PACK555(r, g, b);
        }

        for (x = 3; x < bm.bmWidth - 3; x++)
        {
            w = pRow[x + 3];
            r += RED555(w);
            g += GREEN555(w);
            b += BLUE555(w);
            line[x] = PACK555(r, g, b);

            w = pRow[x - 3];
            r -= RED555(w);
            g -= GREEN555(w);
            b -= BLUE555(w);
        }

        for (x = bm.bmWidth - 3; x < bm.bmWidth; x++)
        {
            line[x] = PACK555(r, g, b);

            w = pRow[x - 3];
            r -= RED555(w);
            g -= GREEN555(w);
            b -= BLUE555(w);
        }

        for (x = 0; x < bm.bmWidth; x++)
            pBits[(bm.bmWidthBytes * y) / 2 + x] = line[x];
    }

    // Blur the columns in the same way.
    for (x = 0; x < bm.bmWidth; x++)
    {
        r = g = b = 0;

        for (y = 0; y < 3; y++)
        {
            w = pBits[(bm.bmWidthBytes * y) / 2 + x];
            r += RED555(w);
            g += GREEN555(w);
            b += BLUE555(w);
        }

        for (y = 0; y < 3; y++)
        {
            w = pBits[(bm.bmWidthBytes * (y + 3)) / 2 + x];
            r += RED555(w);
            g += GREEN555(w);
            b += BLUE555(w);
            line[y] = PACK555(r, g, b);
        }

        for (y = 3; y < bm.bmHeight - 3; y++)
        {
            w = pBits[(bm.bmWidthBytes * (y + 3)) / 2 + x];
            r += RED555(w);
            g += GREEN555(w);
            b += BLUE555(w);
            line[y] = PACK555(r, g, b);

            w = pBits[(bm.bmWidthBytes * (y - 3)) / 2 + x];
            r -= RED555(w);
            g -= GREEN555(w);
            b -= BLUE555(w);
        }

        for (y = bm.bmHeight - 3; y < bm.bmHeight; y++)
        {
            line[y] = PACK555(r, g, b);

            w = pBits[(bm.bmWidthBytes * (y - 3)) / 2 + x];
            r -= RED555(w);
            g -= GREEN555(w);
            b -= BLUE555(w);
        }

        for (y = 0; y < bm.bmHeight; y++)
            pBits[(y * bm.bmWidthBytes) / 2 + x] = line[y];
    }

    // Shrink to a quarter (every fourth pixel of every fourth row) ...
    int cxSmall = bm.bmWidth / 4;
    int cySmall = bm.bmHeight / 4;

    for (y = 0; y < cySmall; y++)
    {
        for (x = 0; x < cxSmall; x++)
            pBits[(y * bm.bmWidthBytes) / 2 + x] = pBits[(y * bm.bmWidthBytes * 4) / 2 + x * 4];
    }

    // ... and move the small copy from the first rows of the bitmap data (the
    // bottom of the picture) to the last ones (its top), where the preview
    // window shows it.
    int yDest = bm.bmHeight - cySmall - 1;

    for (y = 0; y < cySmall; y++, yDest++)
    {
        for (x = 0; x < cxSmall; x++)
            pBits[(bm.bmWidthBytes * yDest) / 2 + x] = pBits[(y * bm.bmWidthBytes) / 2 + x];
    }
}


//-----------------------------------------------------------------------------
// Name: OnCreateSS()
// Desc: WM_CREATE of the screen saver window: loads the pictures and creates
//       the logo window.
//-----------------------------------------------------------------------------
LRESULT OnCreateSS(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CREATESTRUCT*   pcs = (CREATESTRUCT*)lParam;
    HINSTANCE       hInstance;
    BITMAP          bm;
    WNDCLASS        wc;
    WCHAR           szName[32];
    OSVERSIONINFOEX osvi;
    DWORD           dwExStyle, dwStyle;
    HWND            hwndParent;
    int             i;

    (void)uMsg;
    (void)wParam;

    srand((unsigned int)time(NULL));

    g_cyScreen = pcs->cy;
    g_cxScreen = pcs->cx;
    g_xScreen  = pcs->x;
    g_yScreen  = pcs->y;

    g_hbrBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);

    g_cxhwndLogon = 0;
    g_cyhwndLogon = 0;

    hInstance = GetModuleHandle(NULL);

    // The logo of this edition of Windows comes from the branding DLL.
    g_hbmLogo = (HBITMAP)BrandingLoadImage(L"Shellbrd", IDB_BRANDING_LOGO, IMAGE_BITMAP,
                                           g_cxhwndLogon, g_cyhwndLogon, LR_CREATEDIBSECTION);
    if (GetObject(g_hbmLogo, sizeof(bm), &bm))
    {
        g_logoWidth  = bm.bmWidth;
        g_logoHeight = bm.bmHeight;
    }

    if (fChildPreview)
    {
        Blur(hwnd, g_hbmLogo);

        // the small set of flares is loaded at the size of the preview
        g_cxhwndLogon = PREVIEW_FLARE_WIDTH;
        g_cyhwndLogon = PREVIEW_FLARE_HEIGHT;
    }

    for (i = 0; i < NUM_FLARE_FRAMES; i++)
    {
        g_hbmFlare[i] = NULL;

        StringCchPrintf(szName, ARRAYSIZE(szName),
                        fChildPreview ? L"IDB_SMALLFLARE%02d" : L"IDB_FLARE%02d", i + 1);

        g_hbmFlare[i] = (HBITMAP)LoadImage(hInstance, szName, IMAGE_BITMAP,
                                           g_cxhwndLogon, g_cyhwndLogon, LR_CREATEDIBSECTION);
    }

    if (GetObject(g_hbmFlare[0], sizeof(bm), &bm))
    {
        g_flareWidth  = bm.bmWidth;
        g_flareHeight = bm.bmHeight;
    }

    // The window gets its real size when the first picture fades in.
    g_cxhwndLogon = 1;
    g_cyhwndLogon = 1;

    if (g_hdcLogon)
        DeleteDC(g_hdcLogon);

    g_hdcLogon = CreateCompatibleDC(NULL);
    if (g_hdcLogon && g_hbmFlare[0])
        SelectObject(g_hdcLogon, g_hbmFlare[0]);

    if (g_hwndLogon)
    {
        DestroyWindow(g_hwndLogon);
    }
    else
    {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = sizeof(LONG);
        wc.hInstance     = g_hInst;
        wc.hIcon         = NULL;
        wc.hCursor       = NULL;
        wc.hbrBackground = NULL;
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = L"LOGON";

        RegisterClass(&wc);
    }

    if (fChildPreview)
    {
        dwExStyle  = 0;
        dwStyle    = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
        hwndParent = hwnd;
    }
    else
    {
        dwExStyle  = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
        dwStyle    = WS_POPUP | WS_VISIBLE;
        hwndParent = hMainWindow;
    }

    g_hwndLogon = CreateWindowEx(dwExStyle, L"LOGON", NULL, dwStyle, 5, 5,
                                 g_cxhwndLogon, g_cyhwndLogon, hwndParent, NULL, g_hInst, NULL);
    if (g_hwndLogon)
    {
        SetTimer(g_hwndLogon, LOGON_TIMER_ID, LOGON_TIMER_INTERVAL, NULL);
        PostMessage(hwnd, WM_SHOWLOGON, 0, 0);
    }

    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (GetVersionEx((OSVERSIONINFO*)&osvi) &&
        (osvi.wProductType == VER_NT_SERVER || osvi.wProductType == VER_NT_DOMAIN_CONTROLLER))
    {
        g_fServer = true;
    }

    g_hPowerSrc  = RegisterPowerSettingNotification(g_hwndLogon, &GUID_ACDC_POWER_SOURCE,
                                                    DEVICE_NOTIFY_WINDOW_HANDLE);
    g_hMonitorOn = RegisterPowerSettingNotification(g_hwndLogon, &GUID_MONITOR_POWER_ON,
                                                    DEVICE_NOTIFY_WINDOW_HANDLE);

    return 0;
}


//-----------------------------------------------------------------------------
// Name: ScreenSaverProc()
// Desc: Window procedure of the (black) screen saver window, called by the
//       screen saver library.
//-----------------------------------------------------------------------------
LRESULT WINAPI ScreenSaverProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        OnCreateSS(hwnd, uMsg, wParam, lParam);
        break;

    case WM_SIZE:
        g_cxScreen = LOWORD(lParam);
        g_cyScreen = HIWORD(lParam);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        RECT        rc;
        HDC         hdc = BeginPaint(hwnd, &ps);

        SetRect(&rc, g_xScreen, g_yScreen, g_cxScreen, g_cyScreen);
        FillRect(hdc, &rc, g_hbrBlack);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_WINDOWPOSCHANGING:
        // hide the logo together with the screen saver window
        if (g_hwndLogon && (((WINDOWPOS*)lParam)->flags & SWP_HIDEWINDOW))
            ShowWindow(g_hwndLogon, SW_HIDE);
        break;

    case WM_NCACTIVATE:
        // the default handling would end the saver when the logo window is activated
        if (!fChildPreview)
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        break;

    case WM_SHOWLOGON:
        if (g_hwndLogon)
        {
            SetWindowPos(g_hwndLogon, NULL, 0, 0, 0, 0,
                         SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
        break;
    }

    return DefScreenSaverProc(hwnd, uMsg, wParam, lParam);
}


//-----------------------------------------------------------------------------
// Name: ScreenSaverConfigureDialog()
// Desc: The dialog template is empty; all the "dialog" ever does is to tell
//       the user that there is nothing to configure.
//-----------------------------------------------------------------------------
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    (void)lParam;

    if (uMsg == WM_INITDIALOG)
    {
        TaskDialog(hDlg, hMainInstance, MAKEINTRESOURCE(IDS_CONFIG_TITLE),
                   MAKEINTRESOURCE(IDS_CONFIG_NOOPTIONS), MAKEINTRESOURCE(IDS_CONFIG_NOOPTIONS_TEXT),
                   TDCBF_OK_BUTTON, TD_INFORMATION_ICON, NULL);

        EndDialog(hDlg, TRUE);
    }

    return FALSE;
}

