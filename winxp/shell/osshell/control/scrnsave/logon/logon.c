/*-----------------------------------------------------------------------------
 * File: logon.c
 *
 * Desc: The "Windows Server 2003" / "Windows XP" logo screen saver
 *       (logon.scr) of Windows Server 2003 SP2 and Windows XP Professional
 *       x64 Edition SP2.
 *
 *       The screen turns black and a window with the product logo jumps to
 *       a new random place every ten seconds.  The logo is the one of the
 *       installed edition: special server editions (Storage Server, Compute
 *       Cluster, Home Server, R2) take it from their branding DLL, all
 *       others from the bitmaps of this program.
 *
 *       The program entry point, the command line handling and the screen
 *       saver window come from the screen saver library, see
 *       ..\common\scrnsavw\scrnsave.c.
 *
 *       Reconstructed from logon.scr 5.2.3790.3959 (amd64).  The function
 *       and global names are those of the public symbols; the constants, the
 *       resource identifiers and the locals are named by this reconstruction.
 *       The file is C: its uninitialised globals are common symbols ("*
 *       Linker *" contributions in the symbol file) and its functions have
 *       undecorated names.
 *
 *       Original object:
 *           d:\nt\shell\osshell\control\scrnsave\logon\obj\amd64\logon.obj
 *---------------------------------------------------------------------------*/
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <scrnsave.h>
#include <shlwapi.h>
#include "resource.h"

/*
 * IsOS() values.  The public ones are those of <shlwapi.h>; 40 to 43 are
 * private values of the Windows Server 2003 R2 era shlwapi.dll (not in the
 * SDK) - their names here are chosen after the branding DLL each one
 * selects.
 */
#ifndef OS_EMBEDDED
#define OS_EMBEDDED                 13
#define OS_HOME                     19
#define OS_DATACENTER               21
#define OS_ADVSERVER                22
#define OS_SERVER                   23
#define OS_ANYSERVER                29
#define OS_WEBSERVER                31
#define OS_SMALLBUSINESSSERVER      32
#define OS_TABLETPC                 33
#define OS_APPLIANCE                36
#endif
#define OS_STORAGESERVER            40      /* wssbrand.dll: Windows Storage Server 2003 */
#define OS_COMPUTECLUSTER           41      /* hpcbrand.dll: Windows Compute Cluster Server 2003 */
#define OS_SERVERR2                 42      /* r2brand.dll:  Windows Server 2003 R2 */
#define OS_HOMESERVER               43      /* whsbrand.dll: Windows Home Server */

/* exported by ntdll.dll (declared in the DDK's ntrtl.h) */
__declspec(dllimport) ULONG NTAPI RtlRandom(PULONG Seed);

/* the logo window */
#define LOGON_CLASS                 TEXT("LOGON")
#define LOGON_TIMER_ID              1
#define LOGON_TIMER_INTERVAL        10000   /* ms between two jumps */
#define LOGON_START_POS             5       /* first position (x and y) */

/* size of the logo in the little monitor of the control panel preview, and
 * the size used when the bitmap has none */
#define PREVIEW_CX                  50
#define PREVIEW_CY                  32

/* posted to the screen saver window once the logo window exists */
#define WM_SHOWLOGON                WM_USER

/* bitmaps of the branding DLLs of the special server editions */
#define IDB_BRAND_LOGO              1201    /* wssbrand, hpcbrand, whsbrand */
#define IDB_BRAND_LOGO_R2REFRESH    1202    /* wssbrand, if R2Refresh = 1 */
#define IDB_R2_STANDARD             1007    /* r2brand */
#define IDB_R2_ENTERPRISE           1008
#define IDB_R2_DATACENTER           1009

/*-----------------------------------------------------------------------------
 * Globals
 *---------------------------------------------------------------------------*/
HWND        g_hwndLogon = NULL;     /* the window that shows the logo */
HBITMAP     g_hbmLogon  = NULL;     /* the logo */
HPALETTE    g_hpal      = NULL;     /* its palette (8 bit logos) */

HDC         g_hdcLogon;             /* memory DC with g_hbmLogon selected */
int         g_cxhwndLogon;          /* size of the logo window */
int         g_cyhwndLogon;
int         g_xScreen;              /* the area the logo moves in */
int         g_yScreen;
int         g_cxScreen;
int         g_cyScreen;
HBRUSH      g_hbrBlack;
HINSTANCE   g_hInst;                /* never set: NULL (the class is registered for the .exe) */

/* random number generator state of the jumps.  It is never seeded, so the
 * logo visits the same places in the same order every time. */
static ULONG s_ulSeed = 0xEA38A250;

/*-----------------------------------------------------------------------------
 * Name: GetPalette()
 * Desc: Creates a palette from the colour table of a DIB section of up to
 *       8 bits per pixel.  Returns NULL for other bitmaps.
 *---------------------------------------------------------------------------*/
HPALETTE GetPalette(HANDLE hbm)
{
    DIBSECTION      ds;
    RGBQUAD         rgb[256];
    WORD            nColors;
    HANDLE          hMem;
    LOGPALETTE     *pLogPal;
    HDC             hdcScreen;
    HDC             hdcMem;
    HPALETTE        hpal;
    int             i;

    GetObject(hbm, sizeof(ds), &ds);

    if (ds.dsBmih.biBitCount > 8)
        return NULL;

    /* a LOGPALETTE holds at most 0xFFFF entries */
    nColors = (ds.dsBmih.biBitCount < 16) ? (WORD)(1 << ds.dsBmih.biBitCount) : 0xFFFF;

    hMem = GlobalAlloc(GHND, sizeof(LOGPALETTE) + nColors * sizeof(PALETTEENTRY));
    if (hMem == NULL)
        return NULL;

    pLogPal = (LOGPALETTE *)GlobalLock(hMem);

    hdcScreen = GetDC(NULL);
    hdcMem    = CreateCompatibleDC(hdcScreen);
    SelectObject(hdcMem, hbm);

    pLogPal->palVersion    = 0x300;
    pLogPal->palNumEntries = nColors;
    GetDIBColorTable(hdcMem, 0, nColors, rgb);

    for (i = 0; i < nColors; i++)
    {
        pLogPal->palPalEntry[i].peRed   = rgb[i].rgbRed;
        pLogPal->palPalEntry[i].peGreen = rgb[i].rgbGreen;
        pLogPal->palPalEntry[i].peBlue  = rgb[i].rgbBlue;
    }

    hpal = CreatePalette(pLogPal);

    GlobalUnlock(hMem);
    GlobalFree(hMem);
    DeleteObject(hdcMem);           /* (sic: not DeleteDC) */
    ReleaseDC(NULL, hdcScreen);

    return hpal;
}

/*-----------------------------------------------------------------------------
 * Name: WndProc()
 * Desc: Window procedure of the logo window.
 *---------------------------------------------------------------------------*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC         hdc;
    UINT        nChanged;
    int         x, y;

    switch (uMsg)
    {
    case WM_TIMER:
        /* jump to a new random place */
        x = g_xScreen + RtlRandom(&s_ulSeed) % (g_cxScreen - g_cxhwndLogon);
        y = g_yScreen + RtlRandom(&s_ulSeed) % (g_cyScreen - g_cyhwndLogon);
        SetWindowPos(g_hwndLogon, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        break;

    case WM_SETFOCUS:
        return TRUE;

    case WM_PAINT:
        BeginPaint(hWnd, &ps);
        SelectPalette(ps.hdc, g_hpal, FALSE);
        BitBlt(ps.hdc, 0, 0, g_cxhwndLogon, g_cyhwndLogon, g_hdcLogon, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
        break;

    case WM_CLOSE:
        ExitProcess(0);
        break;

    case WM_PALETTECHANGED:
        if ((HWND)wParam == hWnd)
            break;
        /* fall through */

    case WM_QUERYNEWPALETTE:
        hdc = GetDC(hWnd);
        SelectPalette(hdc, g_hpal, FALSE);
        nChanged = RealizePalette(hdc);
        ReleaseDC(hWnd, hdc);
        if (nChanged)
            InvalidateRect(hWnd, NULL, TRUE);
        break;

    default:
        /* the original hands back only TRUE or FALSE */
        return DefScreenSaverProc(hWnd, uMsg, wParam, lParam) ? TRUE : FALSE;
    }

    return 0;
}

/*-----------------------------------------------------------------------------
 * Name: OnCreateSS()
 * Desc: WM_CREATE of the screen saver window: loads the logo of the
 *       installed edition and creates the logo window.
 *---------------------------------------------------------------------------*/
LRESULT OnCreateSS(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPCREATESTRUCT  lpcs = (LPCREATESTRUCT)lParam;
    WNDCLASS        wc;
    BITMAP          bm;
    HMODULE         hBrand;
    HWND            hwndParent;
    DWORD           dwStyle;
    DWORD           dwExStyle = 0;
    DWORD           dwType;
    DWORD           dwR2Refresh;
    DWORD           cbData;
    BOOL            fR2Refresh = FALSE;
    UINT            idLogo;

    g_cxScreen = lpcs->cx;
    g_cyScreen = lpcs->cy;
    g_xScreen  = lpcs->x;
    g_yScreen  = lpcs->y;

    g_hbrBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);

    /* full screen: the bitmap's own size; preview: stretched to fit */
    if (fChildPreview)
    {
        g_cxhwndLogon = PREVIEW_CX;
        g_cyhwndLogon = PREVIEW_CY;
    }
    else
    {
        g_cxhwndLogon = 0;
        g_cyhwndLogon = 0;
    }

    if (g_hbmLogon)
    {
        DeleteObject(g_hbmLogon);
        g_hbmLogon = NULL;
    }

    /*
     * Special server editions: the logo of their branding DLL.
     */
    if (IsOS(OS_STORAGESERVER))
    {
        hBrand = LoadLibraryExA("wssbrand.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (hBrand)
        {
            cbData = sizeof(dwR2Refresh);
            if (SHGetValue(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\ServerAppliance"),
                           TEXT("R2Refresh"), &dwType, &dwR2Refresh, &cbData) == ERROR_SUCCESS &&
                dwType == REG_DWORD && dwR2Refresh == 1)
            {
                fR2Refresh = TRUE;
            }
            g_hbmLogon = LoadBitmap(hBrand, MAKEINTRESOURCE(IDB_BRAND_LOGO + fR2Refresh));
            FreeLibrary(hBrand);
        }
    }
    else if (IsOS(OS_COMPUTECLUSTER))
    {
        hBrand = LoadLibraryExA("hpcbrand.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (hBrand)
        {
            g_hbmLogon = LoadBitmap(hBrand, MAKEINTRESOURCE(IDB_BRAND_LOGO));
            FreeLibrary(hBrand);
        }
    }
    else if (IsOS(OS_HOMESERVER))
    {
        hBrand = LoadLibraryExA("whsbrand.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (hBrand)
        {
            g_hbmLogon = LoadBitmap(hBrand, MAKEINTRESOURCE(IDB_BRAND_LOGO));
            FreeLibrary(hBrand);
        }
    }
    else if (IsOS(OS_SERVERR2) &&
             (hBrand = LoadLibraryExA("r2brand.dll", NULL, LOAD_LIBRARY_AS_DATAFILE)) != NULL)
    {
        if (IsOS(OS_ADVSERVER))
            g_hbmLogon = LoadBitmap(hBrand, MAKEINTRESOURCE(IDB_R2_ENTERPRISE));
        else if (IsOS(OS_DATACENTER))
            g_hbmLogon = LoadBitmap(hBrand, MAKEINTRESOURCE(IDB_R2_DATACENTER));
        else
            g_hbmLogon = LoadBitmap(hBrand, MAKEINTRESOURCE(IDB_R2_STANDARD));
        FreeLibrary(hBrand);
    }

    /*
     * Everybody else: one of our own bitmaps.
     */
    if (g_hbmLogon == NULL)
    {
        if (IsOS(OS_EMBEDDED))
            idLogo = IDB_LOGO_EMBEDDED;
        else if (IsOS(OS_TABLETPC))
            idLogo = IDB_LOGO_TABLETPC;
        else if (IsOS(OS_DATACENTER))
            idLogo = IDB_LOGO_DATACENTER;
        else if (IsOS(OS_ADVSERVER))
            idLogo = IDB_LOGO_ENTERPRISE;
        else if (IsOS(OS_WEBSERVER))
            idLogo = IDB_LOGO_WEB;
        else if (IsOS(OS_SMALLBUSINESSSERVER))
            idLogo = IDB_LOGO_SMALLBUSINESS;
        else if (IsOS(OS_APPLIANCE))
            idLogo = IDB_LOGO_APPLIANCE;
        else if (IsOS(OS_SERVER))
            idLogo = IDB_LOGO_STANDARD;
        else if (IsOS(OS_HOME))
            idLogo = IDB_LOGO_HOME;
        else
            idLogo = IDB_LOGO_PROFESSIONAL;

        g_hbmLogon = (HBITMAP)LoadImage(hMainInstance, MAKEINTRESOURCE(idLogo), IMAGE_BITMAP,
                                        g_cxhwndLogon, g_cyhwndLogon, LR_CREATEDIBSECTION);
    }

    if (g_hbmLogon)
    {
        ZeroMemory(&bm, sizeof(bm));
        g_hpal = GetPalette(g_hbmLogon);
        GetObject(g_hbmLogon, sizeof(bm), &bm);
        g_cxhwndLogon = bm.bmWidth;
        g_cyhwndLogon = bm.bmHeight;
    }

    if (g_cxhwndLogon == 0 || g_cyhwndLogon == 0)
    {
        g_cxhwndLogon = PREVIEW_CX;
        g_cyhwndLogon = PREVIEW_CY;
    }

    if (g_hdcLogon)
        DeleteDC(g_hdcLogon);
    g_hdcLogon = CreateCompatibleDC(NULL);
    if (g_hdcLogon && g_hbmLogon)
        SelectObject(g_hdcLogon, g_hbmLogon);

    /*
     * The logo window: a top-most popup, or a child of the preview window.
     */
    if (g_hwndLogon == NULL)
    {
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 4;
        wc.hInstance     = g_hInst;
        wc.hIcon         = NULL;
        wc.hCursor       = NULL;
        wc.hbrBackground = NULL;
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = LOGON_CLASS;
        RegisterClass(&wc);
    }
    else
    {
        DestroyWindow(g_hwndLogon);
    }

    if (fChildPreview)
    {
        dwStyle    = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
        hwndParent = hWnd;
    }
    else
    {
        dwExStyle  = WS_EX_TOPMOST;
        dwStyle    = WS_POPUP | WS_VISIBLE;
        hwndParent = hMainWindow;   /* (still NULL during WM_CREATE) */
    }

    g_hwndLogon = CreateWindowEx(dwExStyle, LOGON_CLASS, NULL, dwStyle,
                                 LOGON_START_POS, LOGON_START_POS,
                                 g_cxhwndLogon, g_cyhwndLogon,
                                 hwndParent, NULL, g_hInst, NULL);
    if (g_hwndLogon)
    {
        SetTimer(g_hwndLogon, LOGON_TIMER_ID, LOGON_TIMER_INTERVAL, NULL);
        PostMessage(hWnd, WM_SHOWLOGON, 0, 0);
    }

    return 0;
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverProc()
 * Desc: Window procedure of the screen saver window: keeps it black.
 *---------------------------------------------------------------------------*/
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    RECT        rc;
    HDC         hdc;

    switch (uMsg)
    {
    case WM_CREATE:
        OnCreateSS(hWnd, uMsg, wParam, lParam);
        break;

    case WM_SIZE:
        g_cxScreen = LOWORD(lParam);
        g_cyScreen = HIWORD(lParam);
        break;

    case WM_WINDOWPOSCHANGING:
        /* hide the logo together with the screen saver */
        if (g_hwndLogon && (((LPWINDOWPOS)lParam)->flags & SWP_HIDEWINDOW))
            ShowWindow(g_hwndLogon, SW_HIDE);
        break;

    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        SetRect(&rc, g_xScreen, g_yScreen, g_cxScreen, g_cyScreen);
        FillRect(hdc, &rc, g_hbrBlack);
        EndPaint(hWnd, &ps);
        break;

    case WM_NCACTIVATE:
        if (!fChildPreview)
            return DefWindowProc(hWnd, WM_NCACTIVATE, wParam, lParam);
        break;

    case WM_SHOWLOGON:
        if (g_hwndLogon)
            SetWindowPos(g_hwndLogon, NULL, 0, 0, 0, 0,
                         SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
        break;
    }

    return DefScreenSaverProc(hWnd, uMsg, wParam, lParam);
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverConfigureDialog()
 * Desc: There is nothing to configure: says so in a message box and closes
 *       the (empty) dialog.  The title is the product name - IDS_DESCRIPTION
 *       holds two of them, "Windows Server 2003\0Windows XP", and
 *       workstations use the second.
 *---------------------------------------------------------------------------*/
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TCHAR   szTitle[256];
    TCHAR   szMessage[256];
    int     cchTitle;
    int     cchFirst;

    if (uMsg == WM_INITDIALOG)
    {
        cchTitle = LoadString(hMainInstance, IDS_DESCRIPTION, szTitle, ARRAYSIZE(szTitle));
        if (!IsOS(OS_ANYSERVER))
        {
            cchFirst = lstrlen(szTitle);
            if (cchTitle != cchFirst)
                lstrcpyn(szTitle, szTitle + cchFirst + 1, cchTitle - cchFirst);
        }

        LoadString(hMainInstance, IDS_NOOPTIONS, szMessage, ARRAYSIZE(szMessage));
        MessageBox(hDlg, szMessage, szTitle, MB_ICONEXCLAMATION);
        EndDialog(hDlg, TRUE);
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
 * Name: RegisterDialogClasses()
 *---------------------------------------------------------------------------*/
BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
    return TRUE;
}
