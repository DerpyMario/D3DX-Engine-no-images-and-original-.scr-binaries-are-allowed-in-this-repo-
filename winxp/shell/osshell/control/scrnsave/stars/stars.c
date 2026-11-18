/*-----------------------------------------------------------------------------
 * File: stars.c
 *
 * Desc: The "Starfield" screen saver (ssstars.scr, "Starfield Simulation")
 *       of Windows XP Professional x64 Edition / Windows Server 2003 SP1.
 *
 *       Up to 200 stars fly towards the viewer.  Each star has a position
 *       (X, Y) in a plane and a distance Z; it is drawn at
 *       (X * DEPTH / Z, Y * DEPTH / Z) from the centre of the screen, as a
 *       square that grows from 1 to 5 pixels while it comes closer.  A star
 *       that reaches the viewer or leaves the screen starts again far away.
 *
 *       Settings ([Screen Saver.Stars] in control.ini):
 *           WarpSpeed   0 ... 10, default 5
 *           Density     number of stars, 10 ... 200, default 25
 *
 *       The program entry point, the command line handling and the screen
 *       saver window come from the screen saver library, see
 *       ..\common\scrnsavw\scrnsave.c.
 *
 *       Reconstructed from ssstars.scr 5.2.3790.1830 (amd64).  The symbol
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
#include "stars.h"

#define MINSTARS        10
#define MAXSTARS        200
#define DEFSTARS        25

#define MINWARP         0
#define MAXWARP         10
#define DEFWARP         5

/* the distance a star starts at; at DEPTH it is drawn at (X, Y) */
#define DEPTH           2560

/* size of a star, 1 (far away) to 5 pixels (close) */
#define STARSIZE(z)     ((DEPTH - (z)) / (DEPTH / 4) + 1)

/* the projection divides by the distance; a star at 0 counts as at 1 */
#define NONZERO(z)      ((z) ? (z) : 1)

/* projection of star i on the screen */
#define SCREENX(i)      (X[i] * DEPTH / NONZERO(Z[i]) + wXCenter)
#define SCREENY(i)      (Y[i] * DEPTH / NONZERO(Z[i]) + wYCenter)

/*
 * A random coordinate relative to the centre: 0 ... n-1 minus c.  The
 * generator is the one of the C run time (without its 15 bit mask), seeded
 * from the tick count.  Note that it tests the width of the screen, also
 * when it is used for the height.
 */
#define RAND(n, c)      (wScreenX ? (int)((dwRandSeed = dwRandSeed * 214013 + 2531011) >> 16) % (int)(n) - (int)(c) : 0)

#define ID_TIMER        1
#define MIN_ELAPSE      50      /* ms, shortest timer interval */
#define DEF_ELAPSE      50      /* ms, first timer interval */
#define ELAPSE_STEP_UP  10      /* slow down this much when ticks pile up */
#define ELAPSE_STEP_DN  100     /* speed up this much after 100 good ticks */
#define GOOD_TICKS      100

/*-----------------------------------------------------------------------------
 * Globals
 *---------------------------------------------------------------------------*/
TCHAR   szWarpSpeed[]   = TEXT("WarpSpeed");
TCHAR   szDensity[]     = TEXT("Density");

/* context help of the configuration dialog (display.hlp) */
DWORD   aStarsHelpIds[] =
{
    (DWORD)-1,          (DWORD)-1,
    ID_SLOW,            IDH_DISPLAY_SCREENSAVER_STARFIELD_WARPSPEED,
    ID_FAST,            IDH_DISPLAY_SCREENSAVER_STARFIELD_WARPSPEED,
    ID_SPEED,           IDH_DISPLAY_SCREENSAVER_STARFIELD_WARPSPEED,
    ID_DENSITY_LABEL,   IDH_DISPLAY_SCREENSAVER_STARFIELD_DENSITY,
    ID_DENSITY,         IDH_DISPLAY_SCREENSAVER_STARFIELD_DENSITY,
    ID_DENSITYARROW,    IDH_DISPLAY_SCREENSAVER_STARFIELD_DENSITY,
    0, 0
};

WORD    wElapse         = DEF_ELAPSE;   /* current timer interval */
WORD    wMinStars       = MINSTARS;     /* range of the up-down control */
WORD    wMaxStars       = MAXSTARS;

WORD    wWarpSpeed;                     /* settings */
WORD    wNumStars;

WORD    wScreenX;                       /* size of the window */
WORD    wScreenY;
WORD    wXCenter;                       /* its centre */
WORD    wYCenter;

WORD    wSpeed;                         /* current speed, follows wDestSpeed */
WORD    wDestSpeed;                     /* (wWarpSpeed + 1) * 10 */
UINT_PTR wTimer;
int     nGoodTicks;                     /* ticks in a row without backlog */
DWORD   dwRandSeed;

int     X[MAXSTARS];                    /* the stars */
int     Y[MAXSTARS];
int     Z[MAXSTARS];

HWND    hSpeed;                         /* controls of the dialog */
HWND    hOK;                            /* (not used) */
HWND    hDensity;

/*-----------------------------------------------------------------------------
 * Name: GetIniEntries()
 * Desc: Loads the strings and reads the settings.
 *---------------------------------------------------------------------------*/
void GetIniEntries(void)
{
    LoadString(hMainInstance, idsName, szName, TITLEBARNAMELEN);
    LoadString(hMainInstance, idsAppName, szAppName, APPNAMEBUFFERLEN);
    LoadString(hMainInstance, idsIniFile, szIniFile, MAXFILELEN);
    LoadString(hMainInstance, idsScreenSaver, szScreenSaver, SCREENSAVERLEN);
    LoadString(hMainInstance, idsHelpFile, szHelpFile, MAXFILELEN);
    LoadString(hMainInstance, idsNoHelpMemory, szNoHelpMemory, BUFFLEN);

    wWarpSpeed = (WORD)GetPrivateProfileInt(szAppName, szWarpSpeed, DEFWARP, szIniFile);
    if (wWarpSpeed > MAXWARP)
        wWarpSpeed = DEFWARP;

    wNumStars = (WORD)GetPrivateProfileInt(szAppName, szDensity, DEFSTARS, szIniFile);
    if (wNumStars > MAXSTARS)
        wNumStars = MAXSTARS;
    else if (wNumStars < MINSTARS)
        wNumStars = MINSTARS;
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverProc()
 *---------------------------------------------------------------------------*/
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG     msg;
    HDC     hdc;
    WORD    i;
    int     x, y, nSize;

    switch (message)
    {
    case WM_CREATE:
        GetIniEntries();
        dwRandSeed = GetTickCount();

        wScreenX = (WORD)((LPCREATESTRUCT)lParam)->cx;
        wScreenY = (WORD)((LPCREATESTRUCT)lParam)->cy;
        wXCenter = wScreenX / 2;
        wYCenter = wScreenY / 2;

        for (i = 0; i < wNumStars; i++)
        {
            X[i] = RAND(wScreenX, wXCenter);
            Y[i] = RAND(wScreenY, wYCenter);
            Z[i] = DEPTH;
        }

        wDestSpeed = (WORD)((wWarpSpeed + 1) * 10);
        wTimer = SetTimer(hWnd, ID_TIMER, wElapse, NULL);
        break;

    case WM_SIZE:
        wScreenX = LOWORD(lParam);
        wScreenY = HIWORD(lParam);
        break;

    case WM_TIMER:
        hdc = GetDC(hWnd);

        for (i = 0; i < wNumStars; i++)
        {
            /* erase the star */
            nSize = STARSIZE(Z[i]);
            PatBlt(hdc, SCREENX(i), SCREENY(i), nSize, nSize, BLACKNESS);

            /* the speed follows the setting one step per star */
            if (wSpeed < wDestSpeed)
                wSpeed++;
            else if (wSpeed > wDestSpeed)
                wSpeed--;

            /* move it closer; one that has arrived starts again */
            Z[i] -= wSpeed;
            if (Z[i] < 0)
                Z[i] = 0;

            if (Z[i] == 0)
            {
                X[i] = RAND(wScreenX, wXCenter);
                Y[i] = RAND(wScreenY, wYCenter);
                Z[i] = DEPTH;
            }

            /* so does one that has left the screen */
            x = SCREENX(i);
            y = SCREENY(i);
            if (x < 0 || y < 0 || x > wScreenX || y > wScreenY)
            {
                X[i] = RAND(wScreenX, wXCenter);
                Y[i] = RAND(wScreenY, wYCenter);
                Z[i] = DEPTH;
                x = SCREENX(i);
                y = SCREENY(i);
            }

            /* draw it */
            nSize = STARSIZE(Z[i]);
            PatBlt(hdc, x, y, nSize, nSize, WHITENESS);
        }

        ReleaseDC(hWnd, hdc);

        /*
         * Adapt the timer to the speed of the machine: if the next tick is
         * already waiting, we are too slow - wait longer; after 100 ticks
         * without that, try a shorter interval again.  (The interval is
         * compared as a signed number.)
         */
        if (PeekMessage(&msg, hWnd, WM_TIMER, WM_TIMER, PM_REMOVE))
        {
            wElapse += ELAPSE_STEP_UP;
            SetTimer(hWnd, ID_TIMER, wElapse, NULL);
            nGoodTicks = 0;
        }
        else if (++nGoodTicks >= GOOD_TICKS)
        {
            wElapse -= ELAPSE_STEP_DN;
            nGoodTicks = 0;
            if ((SHORT)wElapse < MIN_ELAPSE)
                wElapse = MIN_ELAPSE;
            SetTimer(hWnd, ID_TIMER, wElapse, NULL);
        }
        break;

    case WM_DESTROY:
        if (wTimer)
            KillTimer(hWnd, wTimer);
        break;
    }

    return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverConfigureDialog()
 *---------------------------------------------------------------------------*/
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    TCHAR   szTemp[1024];
    BOOL    fOK;
    UINT    nVal;

    switch (message)
    {
    case WM_INITDIALOG:
        GetIniEntries();

        hSpeed   = GetDlgItem(hDlg, ID_SPEED);
        hOK      = GetDlgItem(hDlg, IDOK);
        hDensity = GetDlgItem(hDlg, ID_DENSITY);

        SendMessage(hDensity, EM_LIMITTEXT, 3, 0);
        SendDlgItemMessage(hDlg, ID_DENSITYARROW, UDM_SETBUDDY, (WPARAM)hDensity, 0);
        SendDlgItemMessage(hDlg, ID_DENSITYARROW, UDM_SETRANGE, 0, MAKELONG(wMaxStars, wMinStars));

        SetScrollRange(hSpeed, SB_CTL, MINWARP, MAXWARP, FALSE);
        SetScrollPos(hSpeed, SB_CTL, wWarpSpeed, TRUE);
        SetDlgItemInt(hDlg, ID_DENSITY, wNumStars, FALSE);
        return TRUE;

    case WM_HSCROLL:
        switch (LOWORD(wParam))
        {
        case SB_LINEUP:
        case SB_PAGEUP:
            --wWarpSpeed;
            break;

        case SB_LINEDOWN:
        case SB_PAGEDOWN:
            ++wWarpSpeed;
            break;

        case SB_THUMBPOSITION:
            wWarpSpeed = HIWORD(wParam);
            break;

        case SB_TOP:
            wWarpSpeed = MINWARP;
            break;

        case SB_BOTTOM:
            wWarpSpeed = MAXWARP;
            break;

        case SB_THUMBTRACK:
        case SB_ENDSCROLL:
            return TRUE;
        }

        /* (compared as a signed number, so that 0 - 1 ends at 0) */
        if ((SHORT)wWarpSpeed <= MINWARP)
            wWarpSpeed = MINWARP;
        else if (wWarpSpeed >= MAXWARP)
            wWarpSpeed = MAXWARP;

        SetScrollPos((HWND)lParam, SB_CTL, wWarpSpeed, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_DENSITY:
            /* only a number in range can be saved */
            if (HIWORD(wParam) == EN_UPDATE)
            {
                nVal = GetDlgItemInt(hDlg, ID_DENSITY, &fOK, FALSE);
                fOK  = (nVal >= MINSTARS && nVal <= MAXSTARS);
                EnableWindow(GetDlgItem(hDlg, ID_DENSITYARROW), fOK);
                EnableWindow(GetDlgItem(hDlg, IDOK), fOK);
            }
            break;

        case IDOK:
            nVal = GetDlgItemInt(hDlg, ID_DENSITY, &fOK, FALSE);
            wsprintf(szTemp, TEXT("%d"), nVal);
            WritePrivateProfileString(szAppName, szDensity, szTemp, szIniFile);

            wsprintf(szTemp, TEXT("%d"), wWarpSpeed);
            WritePrivateProfileString(szAppName, szWarpSpeed, szTemp, szIniFile);
            /* fall through */

        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam) == IDOK);
            return TRUE;
        }
        break;

    case WM_HELP:
        WinHelp((HWND)((LPHELPINFO)lParam)->hItemHandle, szHelpFile, HELP_WM_HELP,
                (ULONG_PTR)(LPVOID)aStarsHelpIds);
        break;

    case WM_CONTEXTMENU:
        WinHelp((HWND)wParam, szHelpFile, HELP_CONTEXTMENU, (ULONG_PTR)(LPVOID)aStarsHelpIds);
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
