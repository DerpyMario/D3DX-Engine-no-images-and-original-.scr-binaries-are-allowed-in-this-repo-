/*-----------------------------------------------------------------------------
 * File: marquee.c
 *
 * Desc: The "Marquee" screen saver (ssmarque.scr) of Windows XP Professional
 *       x64 Edition / Windows Server 2003 SP1.
 *
 *       A line of text scrolls across the screen (from right to left, or
 *       from left to right on Arabic and Hebrew systems), centred or at a
 *       random height.  The configuration dialog shows the text scrolling in
 *       a small window of its own ("showtext" class).
 *
 *       Settings ([Screen Saver.Marquee] in control.ini):
 *           Text            default "Windows NT <major>.<minor>"
 *           Font, Size, CharSet
 *           Attributes      five characters '0'/'1': underline, strike out,
 *                           italic, random position, bold
 *           TextColor       "R G B" (default magenta)
 *           BackgroundColor "R G B" (default black)
 *           Speed           1 ... 30 (default 10)
 *
 *       The program entry point, the command line handling and the screen
 *       saver window come from the screen saver library, see
 *       ..\common\scrnsavw\scrnsave.c.
 *
 *       Reconstructed from ssmarque.scr 5.2.3790.1830 (amd64).  The symbol
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
#include <commdlg.h>
#include <scrnsave.h>
#include <string.h>
#include "../common/comstr.h"
#include "marquee.h"

#define MAXTEXT         1024        /* characters of the text */
#define FACESIZE        32

#define MINSPEED        1
#define MAXSPEED        30
#define DEFSPEED        10

#define ID_TIMER        9
#define ID_PREVIEWTIMER 1

#define ITEM_HEIGHT     12          /* height of the items of the colour list */

/* the random generator of the C run time, without its 15 bit mask */
#define RAND(n)         ((int)((dwRandSeed = dwRandSeed * 214013 + 2531011) >> 16) % (n))

/* the Attributes setting: five characters '0' or '1' */
#define ATTR_UNDERLINE  0
#define ATTR_STRIKEOUT  1
#define ATTR_ITALIC     2
#define ATTR_RANDOM     3
#define ATTR_BOLD       4
#define NUMATTRIBUTES   5

/*-----------------------------------------------------------------------------
 * Globals
 *---------------------------------------------------------------------------*/
TCHAR       szFontKey[]         = TEXT("Font");
TCHAR       szSizeKey[]         = TEXT("Size");
TCHAR       szTextKey[]         = TEXT("Text");
TCHAR       szTextColorKey[]    = TEXT("TextColor");
TCHAR       szBkColorKey[]      = TEXT("BackgroundColor");
TCHAR       szAttributesKey[]   = TEXT("Attributes");
TCHAR       szSpeedKey[]        = TEXT("Speed");
TCHAR       szCharSetKey[]      = TEXT("CharSet");
TCHAR       szShowTextClass[]   = TEXT("showtext");

TCHAR       chUnderline         = TEXT('0');
TCHAR       chStrikeOut         = TEXT('0');
TCHAR       chItalic            = TEXT('0');
TCHAR       chBold              = TEXT('0');
DWORD       dwRandSeed          = 1;

/* context help of the configuration dialog (display.hlp) */
DWORD       aMarqueeHelpIds[] =
{
    (DWORD)-1,          (DWORD)-1,
    ID_FORMATTEXT,      IDH_DISPLAY_SCREENSAVER_MARQUEE_FORMATTEXT,
    ID_CENTERED,        IDH_DISPLAY_SCREENSAVER_MARQUEE_CENTERED,
    ID_RANDOM,          IDH_DISPLAY_SCREENSAVER_MARQUEE_RANDOM,
    ID_BGCOLOR_LABEL,   IDH_DISPLAY_SCREENSAVER_MARQUEE_BGCOLOR,
    ID_BGCOLOR,         IDH_DISPLAY_SCREENSAVER_MARQUEE_BGCOLOR,
    ID_SLOW,            IDH_DISPLAY_SCREENSAVER_MARQUEE_SPEED,
    ID_FAST,            IDH_DISPLAY_SCREENSAVER_MARQUEE_SPEED,
    ID_SPEED,           IDH_DISPLAY_SCREENSAVER_MARQUEE_SPEED,
    ID_TEXT_LABEL,      IDH_DISPLAY_SCREENSAVER_MARQUEE_TEXT,
    ID_TEXT,            IDH_DISPLAY_SCREENSAVER_MARQUEE_TEXT,
    ID_SHOWTEXT,        IDH_DISPLAY_SCREENSAVER_MARQUEE_EXAMPLE,
    0, 0
};

int         nDBCSEnabled        = -1;   /* GetSystemMetrics(SM_DBCSENABLED), when known */

/* settings */
BOOL        fRandom;                    /* random position */
BYTE        bCharSet;
COLORREF    crText;
COLORREF    crBackground;
TCHAR       szFont[FACESIZE];
TCHAR       szText[MAXTEXT + 1];
TCHAR       szDefText[MAXTEXT + 1];     /* "Windows NT x.y" */
TCHAR       szDefFont[FACESIZE];        /* "Tahoma" */
TCHAR       szFormatText[40];           /* title of the font dialog */
CHARSETINFO csiAnsi;                    /* character set of the ANSI code page */

/* the screen saver */
HFONT       hFont;
HBRUSH      hbrBackground;
SIZE        sizeText;
TEXTMETRIC  tm;
int         cchText;
int         nSize;                      /* font height */
SHORT       cxScreen;
WORD        cyScreen;
SHORT       xText;                      /* position of the text */
WORD        wY;
WORD        wSpeed;
WORD        wSteps;                     /* thirds of a pixel to go */
BOOL        fRTL;                       /* right to left reading */
UINT_PTR    wTimer;

/* the configuration dialog */
HWND        hOK;                        /* (not used) */
int         nFontHeight;
HGDIOBJ     hOldFont;
LOGFONT     lf;                         /* the font shown */
LOGFONT     lfSaved;                    /* the font chosen, when lf had to */
HFONT       hfontSaved;                 /* take another character set */
UINT        PWM_NEWSPEED;               /* messages for the "showtext" window */
UINT        PWM_NEWPOSITION;

/* the "showtext" window */
UINT_PTR    wPreviewTimer;
SHORT       xPreview;
SHORT       yPreview;
SHORT       nPreviewSpeed;
SHORT       nPreviewSteps;
SIZE        sizePreview;
BOOL        fPreviewNewLine;            /* start the text again */
BOOL        fRTLPreview;

/*-----------------------------------------------------------------------------
 * Name: IsTextDBCS()
 * Desc: Whether the text needs a double byte character set: on DBCS systems,
 *       whether its ANSI form has a lead byte (or, meant for Japanese, a
 *       half-width katakana - a test that cannot succeed, the character is
 *       signed).
 *---------------------------------------------------------------------------*/
BOOL IsTextDBCS(void)
{
    CHAR    szAnsi[(MAXTEXT + 1) * 2];
    CHAR    ch;
    int     cch = lstrlen(szText);
    INT_PTR i;

    if (nDBCSEnabled == -1)
        nDBCSEnabled = GetSystemMetrics(SM_DBCSENABLED);

    if (nDBCSEnabled)
    {
        WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, szText, cch + 1,
                            szAnsi, sizeof(szAnsi), NULL, NULL);
        for (i = 0; i < cch; i++)
        {
            ch = szAnsi[i];
            if (IsDBCSLeadByte((BYTE)ch))
                return TRUE;
            if (GetACP() == 932 && ch >= 0xA0 && ch < 0xE0)
                return TRUE;
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
 * Name: LoadStrings()
 * Desc: Loads the strings, and makes the default text from the version of
 *       Windows.
 *---------------------------------------------------------------------------*/
void LoadStrings(void)
{
    OSVERSIONINFO   osvi;
    TCHAR           szFormat[MAXTEXT + 1];

    if (!TranslateCharsetInfo((DWORD *)(INT_PTR)(int)GetACP(), &csiAnsi, TCI_SRCCODEPAGE))
        csiAnsi.ciCharset = 0;

    LoadString(hMainInstance, idsName, szName, TITLEBARNAMELEN);
    LoadString(hMainInstance, idsAppName, szAppName, APPNAMEBUFFERLEN);
    LoadString(hMainInstance, idsDefaultText, szFormat, ARRAYSIZE(szFormat));

    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (!GetVersionEx(&osvi))
    {
        osvi.dwMinorVersion = 0;
        osvi.dwMajorVersion = 4;
    }
    wsprintf(szDefText, szFormat, osvi.dwMajorVersion, osvi.dwMinorVersion);

    LoadString(hMainInstance, idsIniFile, szIniFile, MAXFILELEN);
    LoadString(hMainInstance, idsScreenSaver, szScreenSaver, SCREENSAVERLEN);
    LoadString(hMainInstance, idsHelpFile, szHelpFile, MAXFILELEN);
    LoadString(hMainInstance, idsNoHelpMemory, szNoHelpMemory, BUFFLEN);
    LoadString(hMainInstance, idsFormatText, szFormatText, ARRAYSIZE(szFormatText));
    LoadString(hMainInstance, idsDefaultFont, szDefFont, ARRAYSIZE(szDefFont));
}

/*-----------------------------------------------------------------------------
 * Name: ChooseFontHookProc()
 * Desc: Gives the font dialog our title.
 *---------------------------------------------------------------------------*/
UINT_PTR CALLBACK ChooseFontHookProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG)
    {
        ShowWindow(hDlg, SW_SHOWNORMAL);
        SetWindowText(hDlg, szFormatText);
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * Name: PatB() / DrawFrame()
 * Desc: Fill a rectangle with a colour, and draw a frame of nWidth pixels.
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
 * Name: NewPreviewLine()
 * Desc: The height of the text in the "showtext" window: centred, or random.
 *---------------------------------------------------------------------------*/
static void NewPreviewLine(LPRECT prc)
{
    int cy;

    if (!fRandom)
    {
        yPreview = (SHORT)(((prc->bottom - sizePreview.cy) - prc->top) / 2);
    }
    else
    {
        cy = (prc->bottom - sizePreview.cy / 4) - prc->top;
        if (cy == 0)
            cy = 1;
        yPreview = (SHORT)RAND(cy);
    }
}

/*-----------------------------------------------------------------------------
 * Name: ShowTextWndProc()
 * Desc: The "Text Example" of the configuration dialog: the window text
 *       scrolls through the window, in the dialog's font and colours.
 *---------------------------------------------------------------------------*/
LRESULT CALLBACK ShowTextWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    TEXTMETRIC  tmPreview;
    TCHAR       szBuffer[180];
    RECT        rc;
    HDC         hdc;
    HGDIOBJ     hOld;
    HBRUSH      hbr;
    LCID        lcid;
    SHORT       nStep;
    int         cch;

    switch (message)
    {
    case WM_CREATE:
        lcid = GetUserDefaultLCID();
        fRTLPreview = (PRIMARYLANGID(lcid) == LANG_ARABIC || PRIMARYLANGID(lcid) == LANG_HEBREW);

        GetClientRect(hWnd, &rc);
        wPreviewTimer = SetTimer(hWnd, ID_PREVIEWTIMER, 1, NULL);
        nPreviewSpeed = DEFSPEED;
        xPreview = 0;
        if (wPreviewTimer)
            xPreview = (SHORT)rc.right;

        hdc = GetDC(NULL);
        GetTextExtentPoint32(hdc, TEXT("T"), 1, &sizePreview);
        fPreviewNewLine = TRUE;
        yPreview = (SHORT)(((rc.bottom - sizePreview.cy) - rc.top) / 2);
        ReleaseDC(NULL, hdc);
        break;

    case WM_DESTROY:
        KillTimer(hWnd, wPreviewTimer);
        break;

    case WM_SETTEXT:
        DefWindowProc(hWnd, WM_SETTEXT, wParam, lParam);
        InvalidateRect(hWnd, NULL, FALSE);
        /* fall through */

    case WM_PAINT:
        BeginPaint(hWnd, &ps);

        nPreviewSteps += nPreviewSpeed;
        if (nPreviewSteps > 2)
        {
            nStep = nPreviewSteps / 3 + 1;
            nPreviewSteps %= 3;
            if (!fRTLPreview)
                nStep = -nStep;
            xPreview += nStep;

            cch = GetWindowText(hWnd, szBuffer, ARRAYSIZE(szBuffer));

            hOld = NULL;
            if (hFont)
                hOld = SelectObject(ps.hdc, hFont);
            GetTextExtentPoint32(ps.hdc, szBuffer, cch, &sizePreview);
            GetTextMetrics(ps.hdc, &tmPreview);
            GetClientRect(hWnd, &rc);

            if (!fRTLPreview)
            {
                if (sizePreview.cx + xPreview < 0 || fPreviewNewLine)
                {
                    xPreview = (SHORT)rc.right;
                    NewPreviewLine(&rc);
                    fPreviewNewLine = FALSE;
                }
            }
            else if (xPreview >= rc.right || fPreviewNewLine)
            {
                xPreview = (SHORT)(1 - sizePreview.cx);
                NewPreviewLine(&rc);
                fPreviewNewLine = FALSE;
            }

            SetBkColor(ps.hdc, crBackground);
            SetTextColor(ps.hdc, crText);
            ExtTextOut(ps.hdc, xPreview, yPreview,
                       fRTLPreview ? (ETO_OPAQUE | ETO_RTLREADING) : ETO_OPAQUE,
                       &rc, szBuffer, cch, NULL);

            if (hOld)
                SelectObject(ps.hdc, hOld);
        }

        EndPaint(hWnd, &ps);
        return 0;

    case WM_ERASEBKGND:
        hbr = CreateSolidBrush(crBackground);
        if (hbr)
        {
            GetClientRect(hWnd, &rc);
            FillRect((HDC)wParam, &rc, hbr);
            DeleteObject(hbr);
        }
        return 0;

    case WM_TIMER:
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    default:
        if (message == PWM_NEWSPEED)
        {
            nPreviewSpeed = (SHORT)wParam;
        }
        else if (message == PWM_NEWPOSITION)
        {
            GetClientRect(hWnd, &rc);
            NewPreviewLine(&rc);
            fPreviewNewLine = TRUE;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

/*-----------------------------------------------------------------------------
 * Name: GetAttributes()
 * Desc: Reads the five attribute characters.
 *---------------------------------------------------------------------------*/
void GetAttributes(void)
{
    TCHAR szAttributes[NUMATTRIBUTES + 1];

    GetPrivateProfileString(szAppName, szAttributesKey, TEXT("00000"),
                            szAttributes, ARRAYSIZE(szAttributes), szIniFile);
    chUnderline = szAttributes[ATTR_UNDERLINE];
    chStrikeOut = szAttributes[ATTR_STRIKEOUT];
    chItalic    = szAttributes[ATTR_ITALIC];
    fRandom     = (szAttributes[ATTR_RANDOM] == TEXT('1'));
    chBold      = szAttributes[ATTR_BOLD];
}

/*-----------------------------------------------------------------------------
 * Name: RegisterDialogClasses()
 * Desc: Registers the class of the "Text Example" window.
 *---------------------------------------------------------------------------*/
BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
    WNDCLASS wc;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = ShowTextWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hIcon         = NULL;
    wc.hInstance     = (HINSTANCE)hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = szShowTextClass;
    return RegisterClass(&wc);
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
 * Name: PointsToHeight() / HeightToPoints()
 *---------------------------------------------------------------------------*/
static int PointsToHeight(int nPoints)
{
    HDC hdc = GetDC(NULL);
    int nHeight = -(nPoints * GetDeviceCaps(hdc, LOGPIXELSY)) / 72;

    ReleaseDC(NULL, hdc);
    return nHeight;
}

/*-----------------------------------------------------------------------------
 * Name: NewLine()
 * Desc: The height of the text on the screen: centred, or random.
 *---------------------------------------------------------------------------*/
static void NewLine(void)
{
    int cy;

    if (!fRandom)
    {
        wY = (WORD)(cyScreen - sizeText.cy) >> 1;
    }
    else
    {
        cy = (int)cyScreen - sizeText.cy;
        if (cy == 0)
            cy = 1;
        wY = (WORD)RAND(cy);
    }
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverProc()
 *---------------------------------------------------------------------------*/
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    TCHAR   szAttributes[NUMATTRIBUTES + 1];
    RECT    rc;
    HDC     hdc;
    HGDIOBJ hOld;
    LCID    lcid;

    switch (message)
    {
    case WM_CREATE:
        LoadStrings();

        GetPrivateProfileString(szAppName, szAttributesKey, TEXT("00000"),
                                szAttributes, ARRAYSIZE(szAttributes), szIniFile);
        chStrikeOut = szAttributes[ATTR_STRIKEOUT];
        chItalic    = szAttributes[ATTR_ITALIC];
        fRandom     = (szAttributes[ATTR_RANDOM] == TEXT('1'));
        chUnderline = szAttributes[ATTR_UNDERLINE];
        chBold      = szAttributes[ATTR_BOLD];

        GetPrivateProfileString(szAppName, szFontKey, szDefFont, szFont, FACESIZE, szIniFile);
        bCharSet = (BYTE)GetPrivateProfileInt(szAppName, szCharSetKey, 0, szIniFile);
        if (IsTextDBCS())
            bCharSet = (BYTE)csiAnsi.ciCharset;

        hdc = GetDC(NULL);

        lcid = GetUserDefaultLCID();
        fRTL = (PRIMARYLANGID(lcid) == LANG_ARABIC || PRIMARYLANGID(lcid) == LANG_HEBREW);

        cxScreen = (SHORT)((LPCREATESTRUCT)lParam)->cx;
        cyScreen = (WORD)((LPCREATESTRUCT)lParam)->cy;

        /* in the preview the font shrinks with the screen */
        nSize = GetPrivateProfileInt(szAppName, szSizeKey, 0, szIniFile);
        nSize = PointsToHeight(nSize);
        if (fChildPreview)
            nSize = (int)(cyScreen * nSize) / GetDeviceCaps(hdc, VERTRES);

        hFont = CreateFont(nSize, 0, 0, 0,
                           (chBold == TEXT('0')) ? FW_NORMAL : FW_BOLD,
                           chItalic != TEXT('0'), chUnderline != TEXT('0'), chStrikeOut != TEXT('0'),
                           bCharSet, 0, 0, 0, 0, szFont);

        GetPrivateProfileString(szAppName, szTextKey, szDefText, szText, MAXTEXT + 1, szIniFile);
        if ((SIZE_T)lstrlen(szDefText) + 6 < MAXTEXT + 1)
            lstrcat(szDefText, TEXT("     "));
        cchText = lstrlen(szText);

        hOld = SelectObject(hdc, hFont);
        GetTextExtentPoint32(hdc, szText, cchText, &sizeText);
        if (fChildPreview)
            sizeText.cx *= 2;
        GetTextMetrics(hdc, &tm);
        if (hOld)
            SelectObject(hdc, hOld);
        ReleaseDC(NULL, hdc);

        /* (sic: the height of the text, where the width is meant) */
        if (fRTL)
            xText = (SHORT)(1 - sizeText.cy);
        else
            xText = cxScreen;

        dwRandSeed = GetTickCount();
        NewLine();

        wSpeed = (WORD)GetPrivateProfileInt(szAppName, szSpeedKey, DEFSPEED, szIniFile);
        if (wSpeed < MINSPEED)
            wSpeed = MINSPEED;
        else if (wSpeed > MAXSPEED)
            wSpeed = MAXSPEED;

        crText        = GetColorEntry(szAppName, szTextColorKey, RGB(255, 0, 255));
        crBackground  = GetColorEntry(szAppName, szBkColorKey, RGB(0, 0, 0));
        hbrBackground = CreateSolidBrush(crBackground);

        wTimer = SetTimer(hWnd, ID_TIMER, 1, NULL);
        break;

    case WM_SIZE:
        cxScreen = (SHORT)LOWORD(lParam);
        cyScreen = HIWORD(lParam);
        break;

    case WM_ERASEBKGND:
        GetClientRect(hWnd, &rc);
        FillRect((HDC)wParam, &rc, hbrBackground);
        return 0;

    case WM_TIMER:
        /*
         * The text moves wSpeed thirds of a pixel per tick.  The rectangle
         * covers the text and the stretch it leaves behind.
         */
        rc.top    = (SHORT)wY;
        rc.left   = xText - tm.tmMaxCharWidth;
        rc.bottom = (sizeText.cy >> 3) + rc.top + sizeText.cy;
        rc.right  = xText + wSteps / 3 + tm.tmMaxCharWidth * 2 + 1 + sizeText.cx;

        wSteps += wSpeed;
        if (wSteps < 3)
            break;

        hdc = GetDC(hWnd);
        hOld = SelectObject(hdc, hFont);
        SetTextColor(hdc, crText);
        SetBkColor(hdc, crBackground);
        ExtTextOut(hdc, xText, wY, fRTL ? (ETO_OPAQUE | ETO_RTLREADING) : ETO_OPAQUE,
                   &rc, szText, cchText, NULL);
        if (hOld)
            SelectObject(hdc, hOld);

        if (!fRTL)
        {
            if (xText < (SHORT)-sizeText.cx)
            {
                /* gone: wipe what is left and start again on the right */
                hOld = SelectObject(hdc, hbrBackground);
                PatBlt(hdc, 0, (SHORT)wY, wSteps / 3 + tm.tmMaxCharWidth * 2 + 1, sizeText.cy, PATCOPY);
                if (hOld)
                    SelectObject(hdc, hOld);
                xText = cxScreen;
                if (fRandom)
                    NewLine();
            }
            else
            {
                xText -= wSteps / 3 + 1;
            }
        }
        else
        {
            if (xText < cxScreen)
            {
                xText += wSteps / 3 + 1;
            }
            else
            {
                xText = (SHORT)(1 - sizeText.cx);
                if (fRandom)
                    NewLine();
            }
        }

        ReleaseDC(hWnd, hdc);
        wSteps %= 3;
        break;

    case WM_DESTROY:
        if (wTimer)
            KillTimer(hWnd, wTimer);
        if (hFont)
            DeleteObject(hFont);
        DeleteObject(hbrBackground);
        break;
    }

    return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

/*-----------------------------------------------------------------------------
 * Name: UpdateExample()
 * Desc: Tells the "Text Example" window to start again with the new font.
 *---------------------------------------------------------------------------*/
#define UpdateExample(hDlg)                                                             \
    {                                                                                   \
        SendDlgItemMessage(hDlg, ID_SHOWTEXT, PWM_NEWPOSITION, (WPARAM)(INT_PTR)fRandom, 0); \
        InvalidateRect(GetDlgItem(hDlg, ID_SHOWTEXT), NULL, TRUE);                     \
    }

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverConfigureDialog()
 *---------------------------------------------------------------------------*/
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UINT                nEntries = 0;
    RECT                rc;
    CHOOSEFONT          cf;
    LPDRAWITEMSTRUCT    lpdis;
    HPALETTE            hPal;
    HDC                 hdc;
    UINT                i, nIndex, nSpeed;
    int                 nPos;

    switch (message)
    {
    case WM_INITDIALOG:
        PWM_NEWSPEED    = RegisterWindowMessage(TEXT("PWM_NEWSPEED"));
        PWM_NEWPOSITION = RegisterWindowMessage(TEXT("PWM_NEWPOSITION"));
        LoadStrings();
        GetAttributes();
        hOK = GetDlgItem(hDlg, IDOK);

        /* the background colours: those of the default palette */
        hPal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
        GetObject(hPal, sizeof(nEntries), &nEntries);
        for (i = 0; i < nEntries; i++)
            SendDlgItemMessage(hDlg, ID_BGCOLOR, CB_ADDSTRING, 0, (LPARAM)TEXT("a"));

        crBackground = GetColorEntry(szAppName, szBkColorKey, RGB(0, 0, 0));
        nIndex = GetNearestPaletteIndex(hPal, crBackground);
        SendDlgItemMessage(hDlg, ID_BGCOLOR, CB_SETCURSEL, nIndex, 0);
        GetPaletteEntries(hPal, nIndex, 1, (LPPALETTEENTRY)&crBackground);

        CheckRadioButton(hDlg, ID_CENTERED, ID_RANDOM, ID_CENTERED + (fRandom != 0));
        SendDlgItemMessage(hDlg, ID_SHOWTEXT, PWM_NEWPOSITION, (WPARAM)(INT_PTR)fRandom, 0);

        SetScrollRange(GetDlgItem(hDlg, ID_SPEED), SB_CTL, MINSPEED, MAXSPEED, FALSE);
        nSpeed = GetPrivateProfileInt(szAppName, szSpeedKey, DEFSPEED, szIniFile);
        if (nSpeed < MINSPEED)
            nSpeed = MINSPEED;
        else if (nSpeed > MAXSPEED)
            nSpeed = MAXSPEED;
        SetScrollPos(GetDlgItem(hDlg, ID_SPEED), SB_CTL, nSpeed, TRUE);
        SendDlgItemMessage(hDlg, ID_SHOWTEXT, PWM_NEWSPEED, nSpeed, 0);

        SendDlgItemMessage(hDlg, ID_TEXT, EM_LIMITTEXT, MAXTEXT, 0);
        GetPrivateProfileString(szAppName, szTextKey, szDefText, szText, MAXTEXT + 1, szIniFile);
        SetWindowText(GetDlgItem(hDlg, ID_TEXT), szText);

        GetPrivateProfileString(szAppName, szFontKey, szDefFont, szFont, FACESIZE, szIniFile);
        bCharSet = (BYTE)GetPrivateProfileInt(szAppName, szCharSetKey, 0, szIniFile);
        if (IsTextDBCS())
            bCharSet = (BYTE)csiAnsi.ciCharset;

        nFontHeight = GetPrivateProfileInt(szAppName, szSizeKey, 10, szIniFile);
        nFontHeight = PointsToHeight(nFontHeight);
        hFont = CreateFont(nFontHeight, 0, 0, 0,
                           (chBold == TEXT('0')) ? FW_NORMAL : FW_BOLD,
                           chItalic != TEXT('0'), chUnderline != TEXT('0'), chStrikeOut != TEXT('0'),
                           bCharSet, 0, 0, 0, 0, szFont);
        crText = GetColorEntry(szAppName, szTextColorKey, RGB(255, 0, 255));

        /* the font for the font dialog */
        lf.lfWeight    = (chBold == TEXT('0')) ? FW_NORMAL : FW_BOLD;
        lf.lfItalic    = chItalic != TEXT('0');
        lf.lfHeight    = nFontHeight;
        lf.lfCharSet   = bCharSet;
        lf.lfUnderline = chUnderline != TEXT('0');
        lf.lfStrikeOut = chStrikeOut != TEXT('0');
        lstrcpyn(lf.lfFaceName, szFont, LF_FACESIZE);
        return TRUE;

    case WM_HSCROLL:
        nPos = GetScrollPos(GetDlgItem(hDlg, ID_SPEED), SB_CTL);
        switch (LOWORD(wParam))
        {
        case SB_PAGEUP:
            nPos -= 2;
            /* fall through */
        case SB_LINEUP:
            if (--nPos < MINSPEED)
                nPos = MINSPEED;
            break;

        case SB_PAGEDOWN:
            nPos += 2;
            /* fall through */
        case SB_LINEDOWN:
            if ((UINT)++nPos > MAXSPEED)
                nPos = MAXSPEED;
            break;

        case SB_THUMBPOSITION:
            nPos = HIWORD(wParam);
            break;

        case SB_TOP:
            nPos = MINSPEED;
            break;

        case SB_BOTTOM:
            nPos = MAXSPEED;
            break;
        }
        SetScrollPos(GetDlgItem(hDlg, ID_SPEED), SB_CTL, nPos, TRUE);
        SendDlgItemMessage(hDlg, ID_SHOWTEXT, PWM_NEWSPEED, nPos, 0);
        break;

    case WM_MEASUREITEM:
        ((LPMEASUREITEMSTRUCT)lParam)->itemHeight = ITEM_HEIGHT;
        return TRUE;

    case WM_DRAWITEM:
        lpdis = (LPDRAWITEMSTRUCT)lParam;
        rc = lpdis->rcItem;
        if (lpdis->itemState & ODS_SELECTED)
        {
            DrawFrame(lpdis->hDC, &rc, RGB(0, 0, 0), 2);
            InflateRect(&rc, -1, -1);
            DrawFrame(lpdis->hDC, &rc, RGB(255, 255, 255), 2);
            InflateRect(&rc, -1, -1);
        }
        hdc = lpdis->hDC;
        SetBkColor(hdc, PALETTEINDEX((WORD)lpdis->itemID));
        ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TEXT:
            if (HIWORD(wParam) == EN_UPDATE)
            {
                /*
                 * Text that needs a double byte character set is shown in
                 * the font chosen with the character set of the system;
                 * the font chosen is kept in lfSaved / hfontSaved.
                 */
                GetDlgItemText(hDlg, ID_TEXT, szText, MAXTEXT + 1);
                if (!IsTextDBCS())
                {
                    if (lf.lfCharSet == csiAnsi.ciCharset && hfontSaved)
                    {
                        if (hFont)
                            DeleteObject(hFont);
                        hFont = hfontSaved;
                        memcpy(&lf, &lfSaved, sizeof(LOGFONT));
                        hfontSaved = NULL;
                        UpdateExample(hDlg);
                    }
                }
                else if (lf.lfCharSet != csiAnsi.ciCharset)
                {
                    if (!hfontSaved)
                    {
                        hfontSaved = hFont;
                        memcpy(&lfSaved, &lf, sizeof(LOGFONT));
                        lf.lfCharSet = (BYTE)csiAnsi.ciCharset;
                        hFont = CreateFontIndirect(&lf);
                    }
                    else
                    {
                        if (hFont)
                            DeleteObject(hFont);
                        hFont = hfontSaved;
                        memcpy(&lf, &lfSaved, sizeof(LOGFONT));
                        hfontSaved = NULL;
                    }
                    UpdateExample(hDlg);
                }
                bCharSet = lf.lfCharSet;
                SetDlgItemText(hDlg, ID_SHOWTEXT, szText);
            }
            break;

        case ID_CENTERED:
        case ID_RANDOM:
            fRandom = (wParam != ID_CENTERED);
            SendDlgItemMessage(hDlg, ID_SHOWTEXT, PWM_NEWPOSITION, (WPARAM)(INT_PTR)fRandom, 0);
            CheckRadioButton(hDlg, ID_CENTERED, ID_RANDOM, LOWORD(wParam));
            break;

        case ID_FORMATTEXT:
            hdc = GetDC(hDlg);
            hOldFont = SelectObject(hdc, hFont);
            GetTextFace(hdc, LF_FACESIZE, lf.lfFaceName);
            SelectObject(hdc, hOldFont);

            cf.lStructSize    = sizeof(cf);
            cf.hwndOwner      = hDlg;
            cf.hDC            = hdc;
            cf.lpLogFont      = &lf;
            cf.Flags          = CF_SCREENFONTS | CF_ENABLEHOOK | CF_INITTOLOGFONTSTRUCT |
                                CF_EFFECTS | CF_LIMITSIZE;
            cf.rgbColors      = crText;
            cf.lCustData      = 0;
            cf.lpfnHook       = ChooseFontHookProc;
            cf.lpTemplateName = NULL;
            cf.hInstance      = NULL;
            cf.lpszStyle      = NULL;
            cf.nFontType      = SCREEN_FONTTYPE;
            cf.nSizeMin       = 8;
            cf.nSizeMax       = 200;

            if (ChooseFont(&cf))
            {
                lstrcpyn(szFont, lf.lfFaceName, FACESIZE);
                crText      = cf.rgbColors;
                nFontHeight = lf.lfHeight;
                chStrikeOut = TEXT('0') + (lf.lfStrikeOut != 0);
                chUnderline = TEXT('0') + (lf.lfUnderline != 0);
                chItalic    = TEXT('0') + (lf.lfItalic != 0);
                chBold      = TEXT('0') + (lf.lfWeight != FW_NORMAL);
                bCharSet    = lf.lfCharSet;

                if (hfontSaved)
                {
                    DeleteObject(hfontSaved);
                    hfontSaved = NULL;
                }
                if (hFont)
                    DeleteObject(hFont);
                hFont = CreateFontIndirect(&lf);

                if (IsTextDBCS() && lf.lfCharSet != csiAnsi.ciCharset)
                {
                    hfontSaved = hFont;
                    memcpy(&lfSaved, &lf, sizeof(LOGFONT));
                    lf.lfCharSet = (BYTE)csiAnsi.ciCharset;
                    hFont = CreateFontIndirect(&lf);
                }
                UpdateExample(hDlg);
            }
            ReleaseDC(hDlg, hdc);
            break;

        case ID_BGCOLOR:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                nIndex = (UINT)SendDlgItemMessage(hDlg, LOWORD(wParam), CB_GETCURSEL, 0, 0);
                GetPaletteEntries((HPALETTE)GetStockObject(DEFAULT_PALETTE), LOWORD(nIndex), 1,
                                  (LPPALETTEENTRY)&crBackground);
                InvalidateRect(GetDlgItem(hDlg, ID_SHOWTEXT), NULL, TRUE);
            }
            break;

        case IDOK:
            GetWindowText(GetDlgItem(hDlg, ID_TEXT), szText, MAXTEXT + 1);
            WritePrivateProfileString(szAppName, szTextKey, szText, szIniFile);
            WritePrivateProfileString(szAppName, szFontKey, szFont, szIniFile);

            /* the height back to points (szText is the scratch buffer now) */
            hdc = GetDC(hDlg);
            nFontHeight = (nFontHeight * -72) / GetDeviceCaps(hdc, LOGPIXELSY);
            wsprintf(szText, TEXT("%d"), nFontHeight);
            WritePrivateProfileString(szAppName, szSizeKey, szText, szIniFile);
            ReleaseDC(hDlg, hdc);

            hPal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
            nIndex = (UINT)SendDlgItemMessage(hDlg, ID_BGCOLOR, CB_GETCURSEL, 0, 0);
            GetPaletteEntries(hPal, LOWORD(nIndex), 1, (LPPALETTEENTRY)&crBackground);
            wsprintf(szText, TEXT("%d %d %d"), GetRValue(crBackground), GetGValue(crBackground),
                     GetBValue(crBackground));
            WritePrivateProfileString(szAppName, szBkColorKey, szText, szIniFile);

            wsprintf(szText, TEXT("%d %d %d"), GetRValue(crText), GetGValue(crText), GetBValue(crText));
            WritePrivateProfileString(szAppName, szTextColorKey, szText, szIniFile);

            wsprintf(szText, TEXT("%d"), GetScrollPos(GetDlgItem(hDlg, ID_SPEED), SB_CTL));
            WritePrivateProfileString(szAppName, szSpeedKey, szText, szIniFile);

            szText[ATTR_STRIKEOUT] = chStrikeOut;
            szText[ATTR_UNDERLINE] = chUnderline;
            szText[ATTR_ITALIC]    = chItalic;
            szText[ATTR_RANDOM]    = TEXT('0') + (fRandom != 0);
            szText[ATTR_BOLD]      = chBold;
            szText[NUMATTRIBUTES]  = TEXT('\0');
            WritePrivateProfileString(szAppName, szAttributesKey, szText, szIniFile);

            wsprintf(szText, TEXT("%i"), bCharSet);
            WritePrivateProfileString(szAppName, szCharSetKey, szText, szIniFile);
            /* fall through */

        case IDCANCEL:
            if (hFont)
                DeleteObject(hFont);
            EndDialog(hDlg, LOWORD(wParam) == IDOK);
            return TRUE;
        }
        break;

    case WM_HELP:
        WinHelp((HWND)((LPHELPINFO)lParam)->hItemHandle, szHelpFile, HELP_WM_HELP,
                (ULONG_PTR)(LPVOID)aMarqueeHelpIds);
        break;

    case WM_CONTEXTMENU:
        WinHelp((HWND)wParam, szHelpFile, HELP_CONTEXTMENU, (ULONG_PTR)(LPVOID)aMarqueeHelpIds);
        break;
    }

    return FALSE;
}
