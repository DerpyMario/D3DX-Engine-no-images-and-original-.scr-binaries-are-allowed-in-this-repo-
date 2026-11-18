/*-----------------------------------------------------------------------------
 * File: mystify.c
 *
 * Desc: The "Mystify" screen saver (ssmyst.scr) of Windows XP Professional
 *       x64 Edition / Windows Server 2003 SP1.
 *
 *       Two polygons of four corners bounce around the screen, each drawn
 *       with a trail of up to 15 earlier copies of itself.  Their colours
 *       fade slowly from one colour to the next, either between a start and
 *       an end colour or between random colours.  The fade is done with an
 *       animated palette: every line is drawn with a pen of a palette index
 *       of its own, and changing the palette entry recolours the line.
 *
 *       Settings ([Screen Saver.Mystify] in control.ini), n = 1 or 2:
 *           Clear Screen    clear the screen first (default 1)
 *           Active<n>       polygon n is shown (default 1)
 *           WalkRandom<n>   random colours (default 1) or two colours
 *           Lines<n>        length of the trail, 1 ... 15 (default 5)
 *           StartColor<n>   "R G B" (default black)
 *           EndColor<n>     "R G B" (default white)
 *
 *       The program entry point, the command line handling and the screen
 *       saver window come from the screen saver library, see
 *       ..\common\scrnsavw\scrnsave.c.
 *
 *       Reconstructed from ssmyst.scr 5.2.3790.1830 (amd64).  The symbol
 *       file of this build is not published: apart from the functions and
 *       globals that <scrnsave.h> defines, every name in this file is a
 *       choice of the reconstruction.
 *---------------------------------------------------------------------------*/
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <scrnsave.h>
#include "../common/comstr.h"
#include "mystify.h"

#define NUMPOLYGONS     2
#define NUMPOINTS       4       /* corners of a polygon */
#define MINLINES        1
#define MAXLINES        15
#define DEFLINES        5

#define MAXSPEED        12      /* largest step of a corner, in pixels */
#define COLORSTEPS      255     /* steps of a colour fade */

/* palette: MAXLINES entries for each polygon, and black */
#define PALETTESIZE       (NUMPOLYGONS * MAXLINES + 1)
#define BLACKINDEX      (NUMPOLYGONS * MAXLINES)

#define ID_TIMER        1
#define TIMER_INTERVAL  10      /* ms */

#define ITEM_HEIGHT     15      /* height of the items of the colour lists */

/* the random generator of the C run time, without its 15 bit mask */
#define RAND(x)         ((int)((dwRandSeed = dwRandSeed * 214013 + 2531011) >> 16) % (x))

/*-----------------------------------------------------------------------------
 * Globals
 *---------------------------------------------------------------------------*/
TCHAR       szClearScreen[] = TEXT("Clear Screen");
DWORD       dwRandSeed      = 1;

/* context help of the configuration dialog (display.hlp) */
DWORD       aMystifyHelpIds[] =
{
    (DWORD)-1,          (DWORD)-1,
    ID_SHAPE_LABEL,     IDH_DISPLAY_SCREENSAVER_MYSTIFY_SHAPE,
    ID_SHAPE,           IDH_DISPLAY_SCREENSAVER_MYSTIFY_SHAPE,
    ID_ACTIVE,          IDH_DISPLAY_SCREENSAVER_MYSTIFY_ACTIVE,
    ID_LINES_LABEL,     IDH_DISPLAY_SCREENSAVER_MYSTIFY_LINES,
    ID_LINES,           IDH_DISPLAY_SCREENSAVER_MYSTIFY_LINES,
    ID_LINESARROW,      IDH_DISPLAY_SCREENSAVER_MYSTIFY_LINES,
    ID_COLORGROUP,      (DWORD)-1,
    ID_2COLORS,         IDH_DISPLAY_SCREENSAVER_MYSTIFY_2COLORS,
    ID_COLOR1,          IDH_DISPLAY_SCREENSAVER_MYSTIFY_2COLORS,
    ID_COLOR2,          IDH_DISPLAY_SCREENSAVER_MYSTIFY_2COLORS,
    ID_RANDOMCOLORS,    IDH_DISPLAY_SCREENSAVER_MYSTIFY_RANDOMCOLORS,
    ID_CLEARSCREEN,     IDH_DISPLAY_SCREENSAVER_MYSTIFY_CLEARSCREEN,
    0, 0
};

/* settings */
BOOL        fClearScreen;
BOOL        fActive[NUMPOLYGONS];
BOOL        fWalkRandom[NUMPOLYGONS];
WORD        wLines[NUMPOLYGONS];
COLORREF    crStartColor[NUMPOLYGONS];
COLORREF    crEndColor[NUMPOLYGONS];

/* the polygons: corners and their steps; [0] is the newest copy */
POINT       aptPoly[NUMPOLYGONS][MAXLINES][NUMPOINTS];
POINT       aptDelta[NUMPOLYGONS][MAXLINES][NUMPOINTS];
WORD        wCurLines[NUMPOLYGONS];     /* copies on the screen */
WORD        wColorIndex[NUMPOLYGONS];   /* palette entry of the next copy */

/* the colour fade */
COLORREF    crFrom[NUMPOLYGONS];
COLORREF    crTo[NUMPOLYGONS];
COLORREF    crCurrent[NUMPOLYGONS];
WORD        wStep[NUMPOLYGONS];
WORD        wSteps[NUMPOLYGONS];

WORD        wScreenX;
WORD        wScreenY;
UINT_PTR    wTimer;
HPEN        hBlackPen;
HPALETTE    hPalette;

/* the palette: a LOGPALETTE with room for PALETTESIZE entries */
struct
{
    WORD            palVersion;
    WORD            palNumEntries;
    PALETTEENTRY    palPalEntry[PALETTESIZE];
}           LogPalette;
LPLOGPALETTE pLogPal;
PALETTEENTRY *pPalEntries;              /* (set, never read) */

/* the configuration dialog */
WORD        wCurPoly;                   /* the polygon shown in the dialog */
HWND        hOK;                        /* (not used) */
TCHAR       szBuffer[1024];
TCHAR       szValue[1024];

/*-----------------------------------------------------------------------------
 * Name: PatB() / DrawFrame()
 * Desc: Fill a rectangle with a colour (ExtTextOut is the fastest way), and
 *       draw a frame of nWidth pixels with it.
 *---------------------------------------------------------------------------*/
static void PatB(HDC hdc, int x, int y, int dx, int dy, COLORREF cr)
{
    RECT rc;

    SetBkColor(hdc, cr);
    rc.left   = x;
    rc.top    = y;
    rc.right  = x + dx;
    rc.bottom = y + dy;
    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
}

void DrawFrame(HDC hdc, LPRECT prc, COLORREF cr, int nWidth)
{
    int dx = prc->right - prc->left;
    int dy = prc->bottom - 2 * nWidth - prc->top;

    PatB(hdc, prc->left, prc->top, dx, nWidth, cr);
    PatB(hdc, prc->left, prc->bottom - nWidth, dx, nWidth, cr);
    PatB(hdc, prc->left, prc->top + nWidth, nWidth, dy, cr);
    PatB(hdc, prc->right - nWidth, prc->top + nWidth, nWidth, dy, cr);
}

/*-----------------------------------------------------------------------------
 * Name: EnablePolygonControls()
 * Desc: Enables the controls of the dialog that apply to polygon wPoly.
 *       (The second parameter is not used; the only caller passes the
 *       polygon twice.)
 *---------------------------------------------------------------------------*/
static void EnablePolygonControls(HWND hDlg, WORD wUnused, WORD wPoly)
{
    EnableWindow(GetDlgItem(hDlg, ID_COLORGROUP),   fActive[wPoly]);
    EnableWindow(GetDlgItem(hDlg, ID_2COLORS),      fActive[wPoly]);
    EnableWindow(GetDlgItem(hDlg, ID_RANDOMCOLORS), fActive[wPoly]);
    EnableWindow(GetDlgItem(hDlg, ID_LINES),        fActive[wPoly]);
    EnableWindow(GetDlgItem(hDlg, ID_LINESARROW),   fActive[wPoly]);

    EnableWindow(GetDlgItem(hDlg, ID_COLOR1), !fWalkRandom[wPoly] && fActive[wPoly]);
    InvalidateRect(GetDlgItem(hDlg, ID_COLOR1), NULL, TRUE);
    EnableWindow(GetDlgItem(hDlg, ID_COLOR2), !fWalkRandom[wPoly] && fActive[wPoly]);
    InvalidateRect(GetDlgItem(hDlg, ID_COLOR2), NULL, TRUE);
}

/*-----------------------------------------------------------------------------
 * Name: OwnerDraw()
 * Desc: WM_DRAWITEM / WM_MEASUREITEM / WM_DELETEITEM of the two colour
 *       lists: item i is a bar of colour i of the default palette.
 *---------------------------------------------------------------------------*/
BOOL OwnerDraw(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    LPDRAWITEMSTRUCT    lpdis;
    COLORREF            cr;
    RECT                rc;
    HDC                 hdc;

    switch (message)
    {
    case WM_DRAWITEM:
        lpdis = (LPDRAWITEMSTRUCT)lParam;
        rc = lpdis->rcItem;
        cr = PALETTEINDEX((WORD)lpdis->itemID);

        if (lpdis->itemState & ODS_SELECTED)
        {
            DrawFrame(lpdis->hDC, &rc, RGB(0, 0, 0), 2);
            InflateRect(&rc, -1, -1);
            DrawFrame(lpdis->hDC, &rc, RGB(255, 255, 255), 2);
            InflateRect(&rc, -1, -1);
        }
        if (lpdis->itemState & ODS_DISABLED)
            cr = RGB(128, 128, 128);

        hdc = lpdis->hDC;
        SetBkColor(hdc, cr);
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
        return TRUE;

    case WM_MEASUREITEM:
        ((LPMEASUREITEMSTRUCT)lParam)->itemHeight = ITEM_HEIGHT;
        return TRUE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
 * Name: GetColorEntry()
 * Desc: Reads a colour "R G B" from control.ini.
 *---------------------------------------------------------------------------*/
COLORREF GetColorEntry(LPCTSTR pszSection, LPCTSTR pszKey, COLORREF crDefault)
{
    TCHAR   szColor[80];
    LPTSTR  psz;
    LPTSTR  pszDigit;
    WORD    wRed, wGreen, wBlue;

    GetPrivateProfileString(pszSection, pszKey, TEXT(""), szColor, ARRAYSIZE(szColor), szIniFile);
    if (szColor[0] == TEXT('\0'))
        return crDefault;

    psz = szColor;

    for (wRed = 0, pszDigit = psz; *pszDigit >= TEXT('0') && *pszDigit <= TEXT('9'); pszDigit++)
        wRed = wRed * 10 + *pszDigit - TEXT('0');
    while (*psz && *psz != TEXT(' '))
        psz++;
    while (*psz && *psz == TEXT(' '))
        psz++;

    for (wGreen = 0, pszDigit = psz; *pszDigit >= TEXT('0') && *pszDigit <= TEXT('9'); pszDigit++)
        wGreen = wGreen * 10 + *pszDigit - TEXT('0');
    while (*psz && *psz != TEXT(' '))
        psz++;
    while (*psz && *psz == TEXT(' '))
        psz++;

    for (wBlue = 0; *psz >= TEXT('0') && *psz <= TEXT('9'); psz++)
        wBlue = wBlue * 10 + *psz - TEXT('0');

    return RGB(wRed, wGreen, wBlue);
}

/*-----------------------------------------------------------------------------
 * Name: GetIniEntries()
 * Desc: Loads the strings and reads the settings.
 *---------------------------------------------------------------------------*/
void GetIniEntries(void)
{
    WORD i;

    LoadString(hMainInstance, idsName, szName, TITLEBARNAMELEN);
    LoadString(hMainInstance, idsAppName, szAppName, APPNAMEBUFFERLEN);
    LoadString(hMainInstance, idsIniFile, szIniFile, MAXFILELEN);
    LoadString(hMainInstance, idsScreenSaver, szScreenSaver, SCREENSAVERLEN);
    LoadString(hMainInstance, idsHelpFile, szHelpFile, MAXFILELEN);
    LoadString(hMainInstance, idsNoHelpMemory, szNoHelpMemory, BUFFLEN);

    fClearScreen = GetPrivateProfileInt(szAppName, szClearScreen, TRUE, szIniFile);
    if (fClearScreen)
        fClearScreen = TRUE;

    for (i = 0; i < NUMPOLYGONS; i++)
    {
        wsprintf(szBuffer, TEXT("Active%d"), i + 1);
        fActive[i] = GetPrivateProfileInt(szAppName, szBuffer, TRUE, szIniFile);
        if (fActive[i])
            fActive[i] = TRUE;

        wsprintf(szBuffer, TEXT("WalkRandom%d"), i + 1);
        fWalkRandom[i] = GetPrivateProfileInt(szAppName, szBuffer, TRUE, szIniFile);
        if (fWalkRandom[i])
            fWalkRandom[i] = TRUE;

        wsprintf(szBuffer, TEXT("Lines%d"), i + 1);
        wLines[i] = (WORD)GetPrivateProfileInt(szAppName, szBuffer, DEFLINES, szIniFile);
        if (wLines[i] < MINLINES)
            wLines[i] = MINLINES;
        else if (wLines[i] > MAXLINES)
            wLines[i] = MAXLINES;

        wsprintf(szBuffer, TEXT("StartColor%d"), i + 1);
        crStartColor[i] = GetColorEntry(szAppName, szBuffer, RGB(0, 0, 0));

        wsprintf(szBuffer, TEXT("EndColor%d"), i + 1);
        crEndColor[i] = GetColorEntry(szAppName, szBuffer, RGB(255, 255, 255));
    }
}

/*-----------------------------------------------------------------------------
 * Name: RandomColor()
 *---------------------------------------------------------------------------*/
#define RandomColor(cr)                                                     \
    {                                                                       \
        int r_ = RAND(256);                                                 \
        int g_ = RAND(256);                                                 \
        int b_ = RAND(256);                                                 \
        (cr) = (COLORREF)((b_ * 256 + g_) * 256 + r_);                      \
    }

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverProc()
 *---------------------------------------------------------------------------*/
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    HDC         hdc;
    HPALETTE    hOldPal;
    HPEN        hPen;
    HGDIOBJ     hOldPen;
    WORD        wPoly, wLine, wPoint, wByte, wIndex;
    int         x, y, nColor, nFrom;

    switch (message)
    {
    case WM_CREATE:
        GetIniEntries();
        wTimer = SetTimer(hWnd, ID_TIMER, TIMER_INTERVAL, NULL);

        wScreenX = (WORD)((LPCREATESTRUCT)lParam)->cx;
        wScreenY = (WORD)((LPCREATESTRUCT)lParam)->cy;
        dwRandSeed = GetTickCount();

        for (wPoly = 0; wPoly < NUMPOLYGONS; wPoly++)
        {
            if (!fActive[wPoly])
                continue;

            for (wPoint = 0; wPoint < NUMPOINTS; wPoint++)
            {
                aptPoly[wPoly][0][wPoint].x = RAND(wScreenX);
                aptPoly[wPoly][0][wPoint].y = RAND(wScreenY);

                /* a step of 1 ... 12 pixels in either direction */
                aptDelta[wPoly][0][wPoint].x = RAND(2 * MAXSPEED) + 1;
                if (aptDelta[wPoly][0][wPoint].x > MAXSPEED)
                    aptDelta[wPoly][0][wPoint].x = MAXSPEED - aptDelta[wPoly][0][wPoint].x;
                aptDelta[wPoly][0][wPoint].y = RAND(2 * MAXSPEED) + 1;
                if (aptDelta[wPoly][0][wPoint].y > MAXSPEED)
                    aptDelta[wPoly][0][wPoint].y = MAXSPEED - aptDelta[wPoly][0][wPoint].y;
            }

            wCurLines[wPoly]   = 1;
            wColorIndex[wPoly] = 0;
            wStep[wPoly]       = 0;
            wSteps[wPoly]      = 0;     /* start a new fade at the first tick */

            if (fWalkRandom[wPoly])
                RandomColor(crTo[wPoly])
            else
                crTo[wPoly] = crStartColor[wPoly];
        }

        /* the palette: all entries black, animated */
        LogPalette.palVersion    = 0x300;
        LogPalette.palNumEntries = PALETTESIZE;
        pPalEntries = &LogPalette.palPalEntry[1];
        pLogPal = (LPLOGPALETTE)&LogPalette;
        for (wIndex = 0; wIndex < PALETTESIZE; wIndex++)
        {
            pLogPal->palPalEntry[wIndex].peRed   = 0;
            pLogPal->palPalEntry[wIndex].peGreen = 0;
            pLogPal->palPalEntry[wIndex].peBlue  = 0;
            pLogPal->palPalEntry[wIndex].peFlags = PC_RESERVED;
        }

        hBlackPen = CreatePen(PS_SOLID, 1, PALETTEINDEX(BLACKINDEX));
        hPalette  = CreatePalette(pLogPal);
        break;

    case WM_SIZE:
        wScreenX = LOWORD(lParam);
        wScreenY = HIWORD(lParam);
        break;

    case WM_ERASEBKGND:
        if (!fClearScreen)
            return 0;
        break;

    case WM_TIMER:
        hdc = GetDC(hWnd);
        if (hdc == NULL)
            break;

        hOldPal = SelectPalette(hdc, hPalette, FALSE);
        RealizePalette(hdc);

        /*
         * The colours: each polygon fades from crFrom to crTo in 255 steps,
         * then picks the next colour.  The colour of its newest line is
         * written into that line's palette entry.
         */
        for (wPoly = 0; wPoly < NUMPOLYGONS; wPoly++)
        {
            if (!fActive[wPoly])
                continue;

            if (wStep[wPoly] == wSteps[wPoly])
            {
                wStep[wPoly]  = 0;
                crFrom[wPoly] = crTo[wPoly];
                wSteps[wPoly] = COLORSTEPS;

                if (fWalkRandom[wPoly])
                    RandomColor(crTo[wPoly])
                else if (crFrom[wPoly] == crEndColor[wPoly])
                    crTo[wPoly] = crStartColor[wPoly];
                else
                    crTo[wPoly] = crEndColor[wPoly];
            }
            else
            {
                wStep[wPoly]++;
            }

            for (nColor = 0, wByte = 0; wByte < 3; wByte++)
            {
                nFrom   = (crFrom[wPoly] >> (wByte * 8)) & 0xFF;
                nColor += ((((int)((crTo[wPoly] >> (wByte * 8)) & 0xFF) - nFrom) * 10 / wSteps[wPoly]
                            * wStep[wPoly]) / 10 + nFrom) << (wByte * 8);
            }
            crCurrent[wPoly] = nColor;

            wIndex = wPoly * MAXLINES + wColorIndex[wPoly];
            pLogPal->palPalEntry[wIndex].peRed   = GetRValue(crCurrent[wPoly]);
            pLogPal->palPalEntry[wIndex].peGreen = GetGValue(crCurrent[wPoly]);
            pLogPal->palPalEntry[wIndex].peBlue  = GetBValue(crCurrent[wPoly]);
            pLogPal->palPalEntry[wIndex].peFlags = PC_RESERVED;
            AnimatePalette(hPalette, wIndex, 1, &pLogPal->palPalEntry[wIndex]);
        }

        /*
         * The polygons: erase the oldest copy, move the others one place
         * back, move the newest one on and draw it.
         */
        for (wPoly = 0; wPoly < NUMPOLYGONS; wPoly++)
        {
            if (!fActive[wPoly])
                continue;

            if (wCurLines[wPoly] == wLines[wPoly])
            {
                hOldPen = SelectObject(hdc, hBlackPen);
                wLine = wCurLines[wPoly] - 1;
                MoveToEx(hdc, aptPoly[wPoly][wLine][0].x, aptPoly[wPoly][wLine][0].y, NULL);
                for (wPoint = 0; wPoint < NUMPOINTS; wPoint++)
                    LineTo(hdc, aptPoly[wPoly][wLine][(wPoint + 1) % NUMPOINTS].x,
                                aptPoly[wPoly][wLine][(wPoint + 1) % NUMPOINTS].y);
                if (hOldPen)
                    SelectObject(hdc, hOldPen);
            }

            for (wLine = wCurLines[wPoly] - 1; wLine > 0; wLine--)
            {
                for (wPoint = 0; wPoint < NUMPOINTS; wPoint++)
                {
                    aptPoly[wPoly][wLine][wPoint]  = aptPoly[wPoly][wLine - 1][wPoint];
                    aptDelta[wPoly][wLine][wPoint] = aptDelta[wPoly][wLine - 1][wPoint];
                }
            }

            for (wPoint = 0; wPoint < NUMPOINTS; wPoint++)
            {
                aptPoly[wPoly][0][wPoint].x += aptDelta[wPoly][0][wPoint].x;
                aptPoly[wPoly][0][wPoint].y += aptDelta[wPoly][0][wPoint].y;
                x = aptPoly[wPoly][0][wPoint].x;
                y = aptPoly[wPoly][0][wPoint].y;

                /* bounce off the edges, with a new random speed */
                if (x >= (int)wScreenX)
                {
                    aptPoly[wPoly][0][wPoint].x  = 2 * wScreenX - x - 2;
                    aptDelta[wPoly][0][wPoint].x = -1 - RAND(MAXSPEED);
                }
                if (aptPoly[wPoly][0][wPoint].x < 0)
                {
                    aptPoly[wPoly][0][wPoint].x  = -aptPoly[wPoly][0][wPoint].x;
                    aptDelta[wPoly][0][wPoint].x = RAND(MAXSPEED) + 1;
                }
                if (y >= (int)wScreenY)
                {
                    aptPoly[wPoly][0][wPoint].y  = 2 * wScreenY - y - 2;
                    aptDelta[wPoly][0][wPoint].y = -1 - RAND(MAXSPEED);
                }
                if (aptPoly[wPoly][0][wPoint].y < 0)
                {
                    aptPoly[wPoly][0][wPoint].y  = -aptPoly[wPoly][0][wPoint].y;
                    aptDelta[wPoly][0][wPoint].y = RAND(MAXSPEED) + 1;
                }
            }

            hPen = CreatePen(PS_SOLID, 1, PALETTEINDEX(wPoly * MAXLINES + wColorIndex[wPoly]));
            hOldPen = SelectObject(hdc, hPen);
            MoveToEx(hdc, aptPoly[wPoly][0][0].x, aptPoly[wPoly][0][0].y, NULL);
            for (wPoint = 0; wPoint < NUMPOINTS; wPoint++)
                LineTo(hdc, aptPoly[wPoly][0][(wPoint + 1) % NUMPOINTS].x,
                            aptPoly[wPoly][0][(wPoint + 1) % NUMPOINTS].y);
            if (hOldPen)
                SelectObject(hdc, hOldPen);
            if (hPen)
                DeleteObject(hPen);

            if (++wColorIndex[wPoly] == wLines[wPoly])
                wColorIndex[wPoly] = 0;
            if (wCurLines[wPoly] < wLines[wPoly])
                wCurLines[wPoly]++;
        }

        if (hOldPal)
            SelectPalette(hdc, hOldPal, FALSE);
        ReleaseDC(hWnd, hdc);
        break;

    case WM_DESTROY:
        if (wTimer)
            KillTimer(hWnd, ID_TIMER);
        if (hPalette)
            DeleteObject(hPalette);
        if (hBlackPen)
            DeleteObject(hBlackPen);
        break;
    }

    return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverConfigureDialog()
 *---------------------------------------------------------------------------*/
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    TCHAR           szFormat[80];
    TCHAR           szTemp[1024];
    RECT            rc;
    PALETTEENTRY    pe = { 0 };
    HPALETTE        hPal;
    WORD            nEntries;
    WORD            i;
    UINT            nVal;
    BOOL            fOK;
    LRESULT         lIndex;

    switch (message)
    {
    case WM_INITDIALOG:
        GetIniEntries();
        GetWindowRect(hDlg, &rc);
        hOK = GetDlgItem(hDlg, IDOK);
        CheckDlgButton(hDlg, ID_CLEARSCREEN, fClearScreen);

        for (i = 0; i < NUMPOLYGONS; i++)
        {
            LoadString(hMainInstance, idsPolygon, szFormat, ARRAYSIZE(szFormat));
            wsprintf(szTemp, szFormat, i + 1);
            lIndex = SendDlgItemMessage(hDlg, ID_SHAPE, CB_ADDSTRING, 0, (LPARAM)szTemp);
            SendDlgItemMessage(hDlg, ID_SHAPE, CB_SETITEMDATA, LOWORD(lIndex), i);
        }

        /* the colour lists show the colours of the default palette (owner
           drawn lists without strings: the "string" is only item data) */
        GetObject(GetStockObject(DEFAULT_PALETTE), sizeof(nEntries), &nEntries);
        for (i = 0; i < nEntries; i++)
        {
            SendDlgItemMessage(hDlg, ID_COLOR1, CB_ADDSTRING, 0, (LPARAM)TEXT("a"));
            SendDlgItemMessage(hDlg, ID_COLOR2, CB_ADDSTRING, 0, (LPARAM)TEXT("a"));
        }

        SendDlgItemMessage(hDlg, ID_LINES, EM_LIMITTEXT, 2, 0);
        SendDlgItemMessage(hDlg, ID_LINESARROW, UDM_SETRANGE, 0, MAKELONG(MAXLINES, MINLINES));

        wCurPoly = 0;
        SendDlgItemMessage(hDlg, ID_SHAPE, CB_SETCURSEL, 0, 0);
        SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(ID_SHAPE, CBN_SELCHANGE), 0);
        return TRUE;

    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_DELETEITEM:
        return OwnerDraw(hDlg, message, wParam, lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_SHAPE:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                lIndex = SendDlgItemMessage(hDlg, ID_SHAPE, CB_GETCURSEL, 0, 0);
                wCurPoly = (WORD)SendDlgItemMessage(hDlg, ID_SHAPE, CB_GETITEMDATA, LOWORD(lIndex), 0);

                CheckDlgButton(hDlg, ID_ACTIVE, fActive[wCurPoly]);
                SetDlgItemInt(hDlg, ID_LINES, wLines[wCurPoly], FALSE);

                hPal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
                GetObject(hPal, sizeof(nEntries), &nEntries);
                if (SendDlgItemMessage(hDlg, ID_COLOR1, CB_SETCURSEL,
                                       GetNearestPaletteIndex(hPal, crStartColor[wCurPoly]), 0) == CB_ERR)
                    SendDlgItemMessage(hDlg, ID_COLOR1, CB_SETCURSEL, 0, 0);
                if (SendDlgItemMessage(hDlg, ID_COLOR2, CB_SETCURSEL,
                                       GetNearestPaletteIndex(hPal, crEndColor[wCurPoly]), 0) == CB_ERR)
                    SendDlgItemMessage(hDlg, ID_COLOR2, CB_SETCURSEL, 0, 0);

                CheckRadioButton(hDlg, ID_2COLORS, ID_RANDOMCOLORS, ID_2COLORS + fWalkRandom[wCurPoly]);
                EnablePolygonControls(hDlg, wCurPoly, wCurPoly);
            }
            break;

        case ID_ACTIVE:
            fActive[wCurPoly] ^= 1;
            CheckDlgButton(hDlg, LOWORD(wParam), fActive[wCurPoly]);
            EnablePolygonControls(hDlg, wCurPoly, wCurPoly);
            break;

        case ID_CLEARSCREEN:
            fClearScreen ^= 1;
            CheckDlgButton(hDlg, LOWORD(wParam), fClearScreen);
            break;

        case ID_COLOR1:
        case ID_COLOR2:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                lIndex = SendDlgItemMessage(hDlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
                GetPaletteEntries((HPALETTE)GetStockObject(DEFAULT_PALETTE), LOWORD(lIndex), 1, &pe);
                if (LOWORD(wParam) == ID_COLOR1)
                    crStartColor[wCurPoly] = *(LPDWORD)&pe & 0x00FFFFFF;
                else
                    crEndColor[wCurPoly] = *(LPDWORD)&pe & 0x00FFFFFF;
            }
            break;

        case ID_2COLORS:
        case ID_RANDOMCOLORS:
            fWalkRandom[wCurPoly] = LOWORD(wParam) - ID_2COLORS;
            CheckRadioButton(hDlg, ID_2COLORS, ID_RANDOMCOLORS, LOWORD(wParam));
            EnableWindow(GetDlgItem(hDlg, ID_COLOR1), !fWalkRandom[wCurPoly]);
            InvalidateRect(GetDlgItem(hDlg, ID_COLOR1), NULL, TRUE);
            EnableWindow(GetDlgItem(hDlg, ID_COLOR2), !fWalkRandom[wCurPoly]);
            InvalidateRect(GetDlgItem(hDlg, ID_COLOR2), NULL, TRUE);
            break;

        case ID_LINES:
            if (HIWORD(wParam) == EN_UPDATE)
            {
                nVal = GetDlgItemInt(hDlg, LOWORD(wParam), &fOK, FALSE);
                fOK = fOK && (WORD)nVal >= MINLINES && (WORD)nVal <= MAXLINES;
                EnableWindow(GetDlgItem(hDlg, ID_LINESARROW), fOK);
                EnableWindow(GetDlgItem(hDlg, IDOK), fOK);
                if (fOK)
                    wLines[wCurPoly] = (WORD)nVal;
            }
            break;

        case IDOK:
            wLines[wCurPoly] = (WORD)GetDlgItemInt(hDlg, ID_LINES, &fOK, FALSE);

            wsprintf(szBuffer, TEXT("%d"), fClearScreen);
            WritePrivateProfileString(szAppName, szClearScreen, szBuffer, szIniFile);

            for (i = 0; i < NUMPOLYGONS; i++)
            {
                wsprintf(szBuffer, TEXT("Active%d"), i + 1);
                wsprintf(szValue, TEXT("%d"), fActive[i]);
                WritePrivateProfileString(szAppName, szBuffer, szValue, szIniFile);

                wsprintf(szBuffer, TEXT("WalkRandom%d"), i + 1);
                wsprintf(szValue, TEXT("%d"), fWalkRandom[i]);
                WritePrivateProfileString(szAppName, szBuffer, szValue, szIniFile);

                wsprintf(szBuffer, TEXT("Lines%d"), i + 1);
                wsprintf(szValue, TEXT("%d"), wLines[i]);
                WritePrivateProfileString(szAppName, szBuffer, szValue, szIniFile);

                wsprintf(szBuffer, TEXT("StartColor%d"), i + 1);
                wsprintf(szValue, TEXT("%d %d %d"), GetRValue(crStartColor[i]),
                         GetGValue(crStartColor[i]), GetBValue(crStartColor[i]));
                WritePrivateProfileString(szAppName, szBuffer, szValue, szIniFile);

                wsprintf(szBuffer, TEXT("EndColor%d"), i + 1);
                wsprintf(szValue, TEXT("%d %d %d"), GetRValue(crEndColor[i]),
                         GetGValue(crEndColor[i]), GetBValue(crEndColor[i]));
                WritePrivateProfileString(szAppName, szBuffer, szValue, szIniFile);
            }
            /* fall through */

        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam) == IDOK);
            return TRUE;
        }
        break;

    case WM_HELP:
        WinHelp((HWND)((LPHELPINFO)lParam)->hItemHandle, szHelpFile, HELP_WM_HELP,
                (ULONG_PTR)(LPVOID)aMystifyHelpIds);
        break;

    case WM_CONTEXTMENU:
        WinHelp((HWND)wParam, szHelpFile, HELP_CONTEXTMENU, (ULONG_PTR)(LPVOID)aMystifyHelpIds);
        break;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
 * Name: RegisterDialogClasses()
 * Desc: The dialog uses an up-down control.
 *---------------------------------------------------------------------------*/
BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
    InitCommonControls();
    return TRUE;
}
