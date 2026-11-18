/*-----------------------------------------------------------------------------
 * File: bezier.c
 *
 * Desc: The "Beziers" screen saver (ssbezier.scr) of Windows XP Professional
 *       x64 Edition / Windows Server 2003 SP1.
 *
 *       A loop of 1 to 10 joined Bezier curves moves over the screen; its
 *       control points bounce off the edges.  The last 1 to 100 positions
 *       stay on the screen, the oldest one is erased each step.  The colour
 *       changes slowly along a ramp of 216 colours (an animated palette
 *       entry on palette devices, a new pen otherwise).
 *
 *       Settings ([Screen Saver.Bezier] in control.ini):
 *           Length      curves in each loop, 1 ... 10 (default 4)
 *           Width       loops kept on the screen, 1 ... 100 (default 30)
 *           LineSpeed   2 ... 20 (default 10)
 *
 *       Bit 0x4000 of Width selects a hidden second mode: a globe of 1500
 *       dots that turns and swings, drawn by a thread of its own into an
 *       8 bit DIB section.  The configuration dialog clears the bit when it
 *       opens.  In this build the mode cannot work: CreateGlobeBitmaps()
 *       counts to 256 with a BYTE and never leaves its first loop.
 *
 *       The program entry point, the command line handling and the screen
 *       saver window come from the screen saver library, see
 *       ..\common\scrnsavw\scrnsave.c.
 *
 *       Reconstructed from ssbezier.scr 5.2.3790.1830 (amd64).  The symbol
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../common/comstr.h"
#include "bezier.h"

#define MINLENGTH       1
#define MAXLENGTH       10
#define DEFLENGTH       4
#define MINWIDTH        1
#define MAXWIDTH        100
#define DEFWIDTH        30
#define MINSPEED        2
#define MAXSPEED        20
#define DEFSPEED        10

#define WIDTH_GLOBE     0x4000  /* hidden flag in the Width setting */

#define ID_TIMER        1

/* the colour ramp: 9 steps of 24 colours between 10 key colours */
#define NUMKEYCOLORS    10
#define RAMPSTEPS       24
#define NUMRAMPCOLORS   ((NUMKEYCOLORS - 1) * RAMPSTEPS)

/* the random generator: a linear congruential one of its own */
#define NEXTRAND()      (dwRandSeed = dwRandSeed * 69069 + 1)
#define RAND(n)         ((n) ? NEXTRAND() % (DWORD)(n) : 0)

/*
 * A loop: two control points per curve (the curves are joined in the
 * middle of each pair), and whether it is on the screen.
 */
typedef struct _BEZLOOP
{
    POINT   apt[MAXLENGTH * 2];
    BOOL    fDrawn;
} BEZLOOP;

/*-----------------------------------------------------------------------------
 * The globe (the hidden mode)
 *---------------------------------------------------------------------------*/
#define NUMGLOBEPOINTS  1500
#define NUMANGLES       3600            /* tenths of a degree */
#define FIX_ONE         65536           /* 16.16 fixed point */
#define GLOBE_COLOR     0xF5            /* palette index of the dots */
#define GLOBE_RADIUS    37
#define GLOBE_INTERVAL  50              /* ms */

#define PI_APPROX       3.141529        /* (sic) */
#define TENTH_DEGREE    (PI_APPROX / 1800.0)

/* the size of a scan line of an 8 bit DIB */
#define DIBWIDTHBYTES(cx)   ((((cx) * 8 + 31) & ~31) / 8)

typedef struct _GLOBEPOINT
{
    int     x, y, z;
    int     xOffset, yOffset, zOffset;  /* (always 0) */
    int     nReserved;
} GLOBEPOINT;

typedef struct _GLOBE
{
    HWND        hwnd;
    HDC         hdc;            /* the screen saver window */
    HANDLE      hEvent;         /* set by every WM_TIMER */
    int         cx;
    int         cy;
    HPALETTE    hPal;
    HBITMAP     hbm;            /* the picture */
    HDC         hdcMem;
    BYTE       *pBits;
    RECT        rcDirty;        /* what has to be copied to the screen */
    RECT        rcBound;        /* where the dots are */
    POINT       ptSrc;
    HBITMAP     hbmText;        /* a second DIB with a font (not used) */
    HDC         hdcText;
    BYTE       *pBitsText;
} GLOBE;

/*-----------------------------------------------------------------------------
 * Globals
 *---------------------------------------------------------------------------*/
BOOL        fGlobeClear = TRUE;         /* the globe thread clears the screen */
int         nSpeed      = DEFSPEED;     /* settings */
int         nWidth      = DEFWIDTH;
int         nLength     = DEFLENGTH;
int         iUnused     = 255;          /* (not referenced) */
BOOL        fUsePens    = TRUE;

/* the key colours of the ramp */
PALETTEENTRY aKeyColors[NUMKEYCOLORS] =
{
    { 255,   0,   0, 0 },
    { 128,   0,   0, 0 },
    {   0, 128,   0, 0 },
    { 128, 128,   0, 0 },
    {   0,   0, 128, 0 },
    { 128,   0, 128, 0 },
    {   0, 128, 128, 0 },
    { 128, 128, 128, 0 },
    { 192, 192, 192, 0 },
    { 255,   0,   0, 0 },
};

TCHAR       szLineSpeed[]   = TEXT("LineSpeed");
TCHAR       szWidth[]       = TEXT("Width");
TCHAR       szLength[]      = TEXT("Length");

/* context help of the configuration dialog (display.hlp) */
DWORD       aBezierHelpIds[] =
{
    0xFFFF,             (DWORD)-1,
    ID_LENGTH_LABEL,    IDH_DISPLAY_SCREENSAVER_BEZIER_LENGTH,
    ID_LENGTH,          IDH_DISPLAY_SCREENSAVER_BEZIER_LENGTH,
    ID_LENGTHARROW,     IDH_DISPLAY_SCREENSAVER_BEZIER_LENGTH,
    ID_WIDTH_LABEL,     IDH_DISPLAY_SCREENSAVER_BEZIER_WIDTH,
    ID_WIDTH,           IDH_DISPLAY_SCREENSAVER_BEZIER_WIDTH,
    ID_WIDTHARROW,      IDH_DISPLAY_SCREENSAVER_BEZIER_WIDTH,
    ID_SPEED,           IDH_DISPLAY_SCREENSAVER_BEZIER_SPEED,
    ID_SLOW,            IDH_DISPLAY_SCREENSAVER_BEZIER_SPEED,
    ID_FAST,            IDH_DISPLAY_SCREENSAVER_BEZIER_SPEED,
    0, 0
};

/* the configuration dialog */
HWND        hLength;
HWND        hOK;                        /* (not used) */
HWND        hSpeed;
HWND        hWidth;

UINT_PTR    wTimer;
BOOL        fPalette;                   /* the display has a palette */
HPALETTE    hPal;
HPALETTE    hOldPal;

/* the globe */
int         nGlobeAngle1;               /* swing */
int         nGlobeAngle2;               /* tilt */
int         nGlobeAngle3;               /* size */
GLOBE      *pGlobe;
GLOBEPOINT *pGlobePoints;
int        *pCos;                       /* 16.16, by tenths of a degree */
int        *pSin;

/* the curves */
BEZLOOP     aLoops[MAXWIDTH];
HPEN        hPenBlack;
HDC         hdc;
BEZLOOP    *pCurLoop;
int         iLoop;                      /* (set, never read) */
HPEN        hPen;
DWORD       dwRandSeed;
HBRUSH      hBrush;
int         cyScreen;
int         cxScreen;
UINT        nWidthSetting;              /* Width as read, with WIDTH_GLOBE */

PALETTEENTRY aColors[NUMRAMPCOLORS];    /* the colour ramp */
int         iColor;
int         nColorCount;
int         nColorDelay;
int         nColors;

POINT       aptVel[MAXLENGTH * 2];      /* speed of each control point */
POINT       aptBez[MAXLENGTH * 3 + 1];  /* points for PolyBezier */

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

    nSpeed = GetPrivateProfileInt(szAppName, szLineSpeed, DEFSPEED, szIniFile);
    if (nSpeed > MAXSPEED || nSpeed < MINSPEED)
        nSpeed = DEFSPEED;

    nWidthSetting = GetPrivateProfileInt(szAppName, szWidth, DEFWIDTH, szIniFile);
    nWidth = nWidthSetting & ~WIDTH_GLOBE;
    if (nWidth > MAXWIDTH)
        nWidth = MAXWIDTH;
    else if (nWidth < MINWIDTH)
        nWidth = MINWIDTH;

    nLength = GetPrivateProfileInt(szAppName, szLength, DEFLENGTH, szIniFile);
    if (nLength > MAXLENGTH)
        nLength = MAXLENGTH;
    else if (nLength < MINLENGTH)
        nLength = MINLENGTH;
}

/*-----------------------------------------------------------------------------
 * Name: NewVelocity()
 * Desc: A speed of 2 ... nSpeed + 1 pixels per step.
 *---------------------------------------------------------------------------*/
#define NewVelocity()   (int)(NEXTRAND() % (DWORD)nSpeed + 2)

/*-----------------------------------------------------------------------------
 * Name: InitBezier()
 * Desc: The first loop: random control points, moving in random directions.
 *---------------------------------------------------------------------------*/
void InitBezier(void)
{
    int i, j;

    for (i = 0; i < MAXLENGTH; i++)
    {
        for (j = 2 * i; j < 2 * i + 2; j++)
        {
            aLoops[0].apt[j].x = RAND(cxScreen);
            aLoops[0].apt[j].y = RAND(cyScreen);
        }

        /*
         * (With one curve, its two inner control points take a different
         * branch here - which does exactly the same.)  The sign comes from
         * bit 4 of the next random number; for the third coordinate the
         * original tests bits 0 and 4.
         */
        for (j = 2 * i; j < 2 * i + 2; j++)
        {
            int v;

            if (nLength == 1 && i != 1 && i != 2)
                v = NewVelocity();
            else
                v = NewVelocity();
            aptVel[j].x = ((NEXTRAND() & (j == 2 * i ? 0x10 : 0x11)) ? 1 : -1) * v;

            if (nLength == 1 && i != 1 && i != 2)
                v = NewVelocity();
            else
                v = NewVelocity();
            aptVel[j].y = ((NEXTRAND() & 0x10) ? 1 : -1) * v;
        }
    }

    pCurLoop = &aLoops[0];
}

/*-----------------------------------------------------------------------------
 * Name: DrawBezier()
 * Desc: Draws a loop: the curves run from the middle of one pair of control
 *       points to the middle of the next.
 *---------------------------------------------------------------------------*/
void DrawBezier(BEZLOOP *pLoop)
{
    int n = nLength;
    int i, j;

    if (n == 1)
    {
        aptBez[0] = pLoop->apt[0];
        aptBez[1] = pLoop->apt[1];
        aptBez[2] = pLoop->apt[2];
        aptBez[3] = pLoop->apt[3];
    }
    else
    {
        aptBez[0].x = (pLoop->apt[0].x + pLoop->apt[1].x) >> 1;
        aptBez[0].y = (pLoop->apt[0].y + pLoop->apt[1].y) >> 1;

        for (i = 1; i <= n; i++)
        {
            j = i % n;
            aptBez[3 * i - 2] = pLoop->apt[2 * i - 1];
            aptBez[3 * i - 1] = pLoop->apt[2 * j];
            aptBez[3 * i].x   = (pLoop->apt[2 * j].x + pLoop->apt[2 * j + 1].x) >> 1;
            aptBez[3 * i].y   = (pLoop->apt[2 * j].y + pLoop->apt[2 * j + 1].y) >> 1;
        }
    }

    PolyBezier(hdc, aptBez, n * 3 + 1);
}

/*-----------------------------------------------------------------------------
 * Name: InitColors()
 * Desc: Sets up the colours: an animated palette entry on palette devices,
 *       the ten system colours on 16 colour devices, a ramp of 216 colours
 *       otherwise.
 *---------------------------------------------------------------------------*/
void InitColors(HDC hdcScreen)
{
    LOGPALETTE  lp;
    int         nPlanes, nBitsPixel;
    int         c, k, n;

    nPlanes    = GetDeviceCaps(hdcScreen, PLANES);
    nBitsPixel = GetDeviceCaps(hdcScreen, BITSPIXEL);
    fPalette   = (GetDeviceCaps(hdcScreen, RASTERCAPS) & RC_PALETTE) != 0;

    if (fPalette)
    {
        /* one animated entry; its colour is set before it is used */
        lp.palVersion = 0x300;
        lp.palNumEntries = 1;
        lp.palPalEntry[0].peFlags = PC_RESERVED;
        hPal = CreatePalette(&lp);
        if (hPal)
        {
            hOldPal = SelectPalette(hdcScreen, hPal, FALSE);
            RealizePalette(hdc);
        }
    }

    if (!fPalette && nPlanes * nBitsPixel < 5)
    {
        nColors = GetSystemPaletteEntries(hdcScreen, 1, 10, aColors);
        if (nColors == 0)
        {
            aColors[0].peRed   = 255;
            aColors[0].peGreen = 255;
            aColors[0].peBlue  = 255;
            aColors[0].peFlags = 0;
            nColors = 1;
        }
        nColorDelay = 1000;
        nColorCount = 0;
        iColor = 8 % nColors;
        return;
    }

    for (n = 0, c = 0; c < NUMKEYCOLORS - 1; c++)
    {
        for (k = 0; k < RAMPSTEPS; k++, n++)
        {
            aColors[n].peFlags = PC_RESERVED;
            aColors[n].peRed   = (BYTE)(aKeyColors[c].peRed +
                                 (k * (aKeyColors[c + 1].peRed - aKeyColors[c].peRed)) / RAMPSTEPS);
            aColors[n].peGreen = (BYTE)(aKeyColors[c].peGreen +
                                 (k * (aKeyColors[c + 1].peGreen - aKeyColors[c].peGreen)) / RAMPSTEPS);
            aColors[n].peBlue  = (BYTE)(aKeyColors[c].peBlue +
                                 (k * (aKeyColors[c + 1].peBlue - aKeyColors[c].peBlue)) / RAMPSTEPS);
        }
    }

    nColors = NUMRAMPCOLORS;
    nColorDelay = 100;
    iColor = 0;
    nColorCount = 0;

    if (fPalette)
    {
        SelectObject(hdcScreen, GetStockObject(BLACK_PEN));
        DeleteObject(hPen);
        hPen = CreatePen(PS_SOLID, 0, PALETTEINDEX(0));
        SelectObject(hdcScreen, hPen);
    }
}

/*-----------------------------------------------------------------------------
 * Name: StepBezier()
 * Desc: One step: erase the oldest loop, move the newest one on and draw it
 *       as the next loop, and change the colour now and then.
 *---------------------------------------------------------------------------*/
void StepBezier(void)
{
    BEZLOOP *pOld = pCurLoop;
    int      nPairs, i, j, k;
    int      x, y;
    HPEN     hNewPen;

    pCurLoop = &aLoops[0];
    if (pOld + 1 < &aLoops[nWidth])
        pCurLoop = pOld + 1;

    if (pCurLoop->fDrawn)
    {
        if (fUsePens)
            SelectObject(hdc, hPenBlack);
        DrawBezier(pCurLoop);
    }

    /* one curve needs two pairs of control points */
    nPairs = (nLength > 1) ? nLength : 2;

    for (i = 0; i < nPairs; i++)
    {
        for (j = 0; j < 2; j++)
        {
            k = 2 * i + j;
            x = pOld->apt[k].x + aptVel[k].x;
            y = pOld->apt[k].y + aptVel[k].y;

            if (x >= cxScreen)
            {
                x = 2 * cxScreen - x - 1;
                if (nLength == 1 && i != 1 && i != 2)
                    aptVel[k].x = -NewVelocity();
                else
                    aptVel[k].x = -NewVelocity();
            }
            if (x < 0)
            {
                x = -x;
                aptVel[k].x = NewVelocity();
            }
            if (y >= cyScreen)
            {
                y = 2 * cyScreen - y - 1;
                aptVel[k].y = -NewVelocity();
            }
            if (y < 0)
            {
                y = -y;
                aptVel[k].y = NewVelocity();
            }

            pCurLoop->apt[k].x = x;
            pCurLoop->apt[k].y = y;
        }
    }

    if (--nColorCount < 1)
    {
        if (!fPalette)
        {
            if (fUsePens)
            {
                hNewPen = CreatePen(PS_SOLID, 0, RGB(aColors[iColor].peRed,
                                                     aColors[iColor].peGreen,
                                                     aColors[iColor].peBlue));
                SelectObject(hdc, hNewPen);
                DeleteObject(hPen);
                hPen = hNewPen;
            }
        }
        else
        {
            AnimatePalette(hPal, 0, 1, &aColors[iColor]);
        }

        iColor--;
        nColorCount = nColorDelay;
        if (iColor < 0)
            iColor = nColors - 1;
    }

    if (fUsePens)
        SelectObject(hdc, hPen);
    DrawBezier(pCurLoop);
    pCurLoop->fDrawn = TRUE;
}

/*-----------------------------------------------------------------------------
 * Name: RandomUpTo()
 * Desc: A random number 0 ... nMax (from the C run time's rand()).
 *---------------------------------------------------------------------------*/
static int RandomUpTo(int nMax)
{
    int nMask = 1;
    int n;

    do
        nMask = nMask * 2 + 1;
    while (nMask < nMax);

    do
        n = rand() & nMask;
    while (n > nMax);

    return n;
}

/*-----------------------------------------------------------------------------
 * Name: InitGlobePoints()
 * Desc: Puts the dots at random places on a sphere.
 *---------------------------------------------------------------------------*/
void InitGlobePoints(GLOBE *pg)
{
    GLOBEPOINT *pp;
    int         a1, a2, t, x, y, z;

    pg->rcBound.left   = 0;
    pg->rcBound.bottom = 0;
    pg->rcBound.top    = 0;
    pg->rcBound.right  = 0;

    for (pp = pGlobePoints; pp < pGlobePoints + NUMGLOBEPOINTS; pp++)
    {
        pp->x = GLOBE_RADIUS;
        pp->y = 0;
        pp->z = 0;

        a1 = RandomUpTo(NUMANGLES / 2);
        a2 = RandomUpTo(NUMANGLES - 1);

        x = pp->x;
        y = pp->y;
        z = pp->z;
        pp->xOffset = 0;
        pp->yOffset = 0;
        pp->zOffset = 0;

        /* turn (x, y, z) by a1 about the y axis, then by a2 about the x axis */
        t     = (z * pCos[a1] - x * pSin[a1]) / FIX_ONE;
        pp->y = (y * pCos[a2] + t * pSin[a2]) / FIX_ONE;
        pp->z = (t * pCos[a2] - y * pSin[a2]) / FIX_ONE;
        pp->x = (z * pSin[a1] + x * pCos[a1]) / FIX_ONE;
    }
}

/*-----------------------------------------------------------------------------
 * Name: DrawGlobe()
 * Desc: Draws the globe into the DIB section: erases the dots of the last
 *       picture, turns the globe on and draws the dots as 2 x 2 squares.
 *---------------------------------------------------------------------------*/
void DrawGlobe(GLOBE *pg)
{
    GLOBEPOINT *pp;
    BYTE       *pRow;
    BYTE       *pDot;
    int         y, a, b, c;
    int         sinA, cosA, sinB, cosB, s1, s2;
    int         t1, t2, sx, sy;

    /* erase the last picture */
    pRow = pg->pBits + DIBWIDTHBYTES(cxScreen) * pg->rcBound.top + pg->rcBound.left;
    for (y = pg->rcBound.top; y < pg->rcBound.bottom; y++)
    {
        memset(pRow, 0, pg->rcBound.right - pg->rcBound.left);
        pRow += DIBWIDTHBYTES(cxScreen);
    }

    pg->rcDirty = pg->rcBound;
    pg->rcBound.left   = cxScreen;
    pg->rcBound.top    = cyScreen;
    pg->rcBound.right  = 0;
    pg->rcBound.bottom = 0;

    /* the swing angle follows a cosine of angle 1 */
    a = nGlobeAngle1;
    if (a > NUMANGLES - 1)
        a -= NUMANGLES;
    a = (pCos[a] * NUMANGLES / 2) / FIX_ONE;
    if (a > NUMANGLES - 1)
        a -= NUMANGLES;
    if (a < 0)
        a += NUMANGLES;
    sinA = pSin[a];
    cosA = pCos[a];

    b = nGlobeAngle2;
    if (b > NUMANGLES - 1)
        b -= NUMANGLES;
    cosB = pCos[b];
    sinB = pSin[b];

    /* the size: (1 + cos) / 2 */
    c = nGlobeAngle3 + 2700;
    if (c > NUMANGLES - 1)
        c = nGlobeAngle3 - 900;
    s1 = (pCos[c] + FIX_ONE) / 2;
    s2 = (pSin[c] + FIX_ONE) / 2;

    for (pp = pGlobePoints; pp < pGlobePoints + NUMGLOBEPOINTS; pp++)
    {
        t1 = pp->z * cosA - pp->x * sinA;
        t2 = pp->z * sinA + pp->x * cosA;

        sx = (t2 / FIX_ONE) * s1 + pp->xOffset * s2;
        sx = sx / FIX_ONE + cxScreen / 2;

        t1 = (t1 / FIX_ONE) * sinB + pp->y * cosB;
        t1 = (t1 / FIX_ONE) * s1 + pp->yOffset * s2;
        sy = t1 / FIX_ONE + cyScreen / 2;

        if (sx < 0)
            sx = 0;
        if (sx > cxScreen - 2)
            sx = cxScreen - 2;
        if (sy < 0)
            sy = 0;
        if (sy > cyScreen - 2)
            sy = cyScreen - 2;

        if (sx < pg->rcBound.left)
            pg->rcBound.left = sx;
        if (pg->rcBound.right < sx + 2)
            pg->rcBound.right = sx + 2;
        if (sy < pg->rcBound.top)
            pg->rcBound.top = sy;
        if (pg->rcBound.bottom < sy + 2)
            pg->rcBound.bottom = sy + 2;

        pDot = pg->pBits + DIBWIDTHBYTES(cxScreen) * sy + sx;
        pDot[0] = GLOBE_COLOR;
        pDot[1] = GLOBE_COLOR;
        pDot[DIBWIDTHBYTES(cxScreen)] = GLOBE_COLOR;
        pDot[DIBWIDTHBYTES(cxScreen) + 1] = GLOBE_COLOR;
    }

    nGlobeAngle1 += 47;
    if (nGlobeAngle1 > NUMANGLES - 1)
        nGlobeAngle1 -= NUMANGLES;
    nGlobeAngle2 += 30;
    if (nGlobeAngle2 > NUMANGLES - 1)
        nGlobeAngle2 -= NUMANGLES;
    nGlobeAngle3 -= 40;
    if (nGlobeAngle3 < 0)
        nGlobeAngle3 += NUMANGLES;

    /* copy what was there and what is there now */
    if (pg->rcBound.left < pg->rcDirty.left)
        pg->rcDirty.left = pg->rcBound.left;
    if (pg->rcDirty.right < pg->rcBound.right)
        pg->rcDirty.right = pg->rcBound.right;
    if (pg->rcBound.top < pg->rcDirty.top)
        pg->rcDirty.top = pg->rcBound.top;
    if (pg->rcDirty.bottom < pg->rcBound.bottom)
        pg->rcDirty.bottom = pg->rcBound.bottom;

    pg->ptSrc.x = pg->rcDirty.left;
    pg->ptSrc.y = pg->rcDirty.top;
}

/*-----------------------------------------------------------------------------
 * Name: GlobeThread()
 * Desc: Draws a new picture of the globe after every WM_TIMER.
 *---------------------------------------------------------------------------*/
DWORD WINAPI GlobeThread(LPVOID pv)
{
    GLOBE  *pg = (GLOBE *)pv;
    RECT    rc;
    double  dAngle;
    int     i;

    GdiSetBatchLimit(1);

    for (i = 0; i < NUMANGLES; i++)
    {
        dAngle = (double)i * TENTH_DEGREE;
        pCos[i] = (int)(cos(dAngle) * 65536.0);
        pSin[i] = (int)(sin(dAngle) * 65536.0);
    }

    InitGlobePoints(pg);

    for (;;)
    {
        if (fGlobeClear)
        {
            rc.left   = 0;
            rc.top    = 0;
            rc.right  = pg->cx;
            rc.bottom = pg->cy;
            FillRect(pg->hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            fGlobeClear = FALSE;
        }

        DrawGlobe(pg);

        BitBlt(pg->hdc, pg->rcDirty.left, pg->rcDirty.top,
               pg->rcDirty.right - pg->rcDirty.left, pg->rcDirty.bottom - pg->rcDirty.top,
               pg->hdcMem, pg->ptSrc.x, pg->ptSrc.y, SRCCOPY);

        WaitForSingleObject(pg->hEvent, INFINITE);
    }
}

/*-----------------------------------------------------------------------------
 * Name: CreateGlobeBitmaps()
 * Desc: Creates the palette (the ten static colours at both ends, the
 *       colours of prgb in between) and the DIB section the globe is drawn
 *       into.  With fNoStatic the static colours are given up as well.
 *
 *       Note: the loops over the 256 palette entries count with a BYTE and
 *       so never end (as in the original binary).
 *---------------------------------------------------------------------------*/
BOOL CreateGlobeBitmaps(GLOBE *pg, RGBQUAD *prgb, BOOL fNoStatic)
{
    LPLOGPALETTE    plp;
    LPBITMAPINFO    pbmi;
    HPALETTE        hpalFlush, hpalOld;
    HFONT           hFont;
    LOGFONT         lf;
    BYTE            i;
    int             n;
    struct
    {
        WORD            palVersion;
        WORD            palNumEntries;
        PALETTEENTRY    palPalEntry[256];
    }               lpFlush;

    plp  = (LPLOGPALETTE)LocalAlloc(LPTR, sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY));
    pbmi = (LPBITMAPINFO)LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
    if (plp == NULL || pbmi == NULL)
        return FALSE;

    plp->palVersion    = 0x300;
    plp->palNumEntries = 256;

    if (!fNoStatic)
    {
        /* (the second call asks for entries 246..255 of the 20 entry
           default palette and would write them from entry 255 on) */
        GetPaletteEntries((HPALETTE)GetStockObject(DEFAULT_PALETTE), 0, 10, &plp->palPalEntry[0]);
        GetPaletteEntries((HPALETTE)GetStockObject(DEFAULT_PALETTE), 246, 10, &plp->palPalEntry[255]);
        for (n = 10; n < 246; n++)
        {
            plp->palPalEntry[n].peRed   = prgb[n].rgbRed;
            plp->palPalEntry[n].peGreen = prgb[n].rgbGreen;
            plp->palPalEntry[n].peBlue  = prgb[n].rgbBlue;
            plp->palPalEntry[n].peFlags = PC_NOCOLLAPSE;
        }
    }
    else
    {
        GetPaletteEntries((HPALETTE)GetStockObject(DEFAULT_PALETTE), 0, 1, &plp->palPalEntry[0]);
        GetPaletteEntries((HPALETTE)GetStockObject(DEFAULT_PALETTE), 255, 1, &plp->palPalEntry[255]);
        for (n = 1; n < 254; n++)
        {
            plp->palPalEntry[n].peRed   = prgb[n].rgbRed;
            plp->palPalEntry[n].peGreen = prgb[n].rgbGreen;
            plp->palPalEntry[n].peBlue  = prgb[n].rgbBlue;
            plp->palPalEntry[n].peFlags = PC_NOCOLLAPSE;
        }
        SetSystemPaletteUse(pg->hdc, SYSPAL_NOSTATIC);
    }

    pg->hPal = CreatePalette(plp);

    /* flush the system palette with a palette of reserved entries */
    lpFlush.palVersion    = 0x300;
    lpFlush.palNumEntries = 256;
    for (i = 0; i < 256; i++)
    {
        lpFlush.palPalEntry[i].peRed   = 0;
        lpFlush.palPalEntry[i].peGreen = 0;
        lpFlush.palPalEntry[i].peBlue  = i;
        lpFlush.palPalEntry[i].peFlags = PC_RESERVED;
    }
    hpalFlush = CreatePalette((LPLOGPALETTE)&lpFlush);
    hpalOld = SelectPalette(pg->hdc, hpalFlush, FALSE);
    RealizePalette(pg->hdc);
    SelectPalette(pg->hdc, hpalOld, FALSE);
    DeleteObject(hpalFlush);

    SelectPalette(pg->hdc, pg->hPal, FALSE);
    RealizePalette(pg->hdc);

    /* an 8 bit top-down DIB section that uses the palette */
    pbmi->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth         = cxScreen;
    pbmi->bmiHeader.biHeight        = -cyScreen;
    pbmi->bmiHeader.biPlanes        = 1;
    pbmi->bmiHeader.biBitCount      = 8;
    pbmi->bmiHeader.biCompression   = BI_RGB;
    pbmi->bmiHeader.biSizeImage     = 0;
    pbmi->bmiHeader.biXPelsPerMeter = 0;
    pbmi->bmiHeader.biYPelsPerMeter = 0;
    pbmi->bmiHeader.biClrUsed       = 0;
    pbmi->bmiHeader.biClrImportant  = 0;
    for (i = 0; i < 256; i++)
        ((WORD *)pbmi->bmiColors)[i] = i;

    pg->hbm = CreateDIBSection(pg->hdc, pbmi, DIB_PAL_COLORS, (void **)&pg->pBits, NULL, 0);
    pg->hdcMem = CreateCompatibleDC(pg->hdc);
    SelectObject(pg->hdcMem, pg->hbm);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight         = 30;
    lf.lfWeight         = FW_NORMAL;
    lf.lfPitchAndFamily = VARIABLE_PITCH;
    lstrcpy(lf.lfFaceName, TEXT("Arial"));
    hFont = CreateFontIndirect(&lf);

    pg->hbmText = CreateDIBSection(pg->hdc, pbmi, DIB_PAL_COLORS, (void **)&pg->pBitsText, NULL, 0);
    pg->hdcText = CreateCompatibleDC(pg->hdcText);  /* (sic: still NULL here) */
    SelectObject(pg->hdcText, pg->hbmText);
    SelectObject(pg->hdcText, hFont);

    return TRUE;
}

/*-----------------------------------------------------------------------------
 * Name: InitGlobe()
 * Desc: Sets up the globe mode and starts its thread.
 *---------------------------------------------------------------------------*/
BOOL InitGlobe(HWND hWnd)
{
    RGBQUAD argb[256];
    DWORD   dwThreadId;
    int     n;

    srand(GetTickCount());

    pGlobePoints = (GLOBEPOINT *)LocalAlloc(LPTR, NUMGLOBEPOINTS * sizeof(GLOBEPOINT));
    pSin   = (int *)LocalAlloc(LPTR, NUMANGLES * sizeof(int));
    pCos   = (int *)LocalAlloc(LPTR, NUMANGLES * sizeof(int));
    pGlobe = (GLOBE *)LocalAlloc(LPTR, sizeof(GLOBE));
    if (pGlobePoints == NULL || pSin == NULL || pCos == NULL || pGlobe == NULL)
        return FALSE;

    pGlobe->hwnd = hWnd;
    pGlobe->cx   = cxScreen;
    pGlobe->cy   = cyScreen;
    pGlobe->hdc  = hdc;
    SetBkColor(pGlobe->hdc, RGB(0, 0, 0));
    SetTextColor(pGlobe->hdc, RGB(255, 255, 255));
    pGlobe->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    SetTimer(pGlobe->hwnd, ID_TIMER, GLOBE_INTERVAL, NULL);

    /* a grey ramp for palette entries 10 ... 245 */
    for (n = 0; n < 236; n++)
    {
        argb[10 + n].rgbBlue  = (BYTE)(n * 255 / 235);
        argb[10 + n].rgbGreen = (BYTE)(n * 255 / 235);
        argb[10 + n].rgbRed   = (BYTE)(n * 255 / 235);
    }

    if (!CreateGlobeBitmaps(pGlobe, argb, FALSE))
        return FALSE;

    CreateThread(NULL, 0, GlobeThread, pGlobe, 0, &dwThreadId);
    return TRUE;
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverProc()
 *---------------------------------------------------------------------------*/
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    int         i;

    switch (message)
    {
    case WM_CREATE:
        GetIniEntries();
        dwRandSeed = GetTickCount();
        hdc = GetDC(hWnd);
        cxScreen = ((LPCREATESTRUCT)lParam)->cx;
        cyScreen = ((LPCREATESTRUCT)lParam)->cy;
        InitBezier();

        if (!(nWidthSetting & WIDTH_GLOBE))
        {
            hBrush    = (HBRUSH)GetStockObject(BLACK_BRUSH);
            hPen      = CreatePen(PS_SOLID, 0, RGB(255, 0, 0));
            hPenBlack = CreatePen(PS_SOLID, 0, RGB(0, 0, 0));
            SelectObject(hdc, hPen);
            SelectObject(hdc, hBrush);
            InitColors(hdc);
            wTimer = SetTimer(hWnd, ID_TIMER, 1, NULL);
        }
        else
        {
            InitGlobe(hWnd);
        }
        break;

    case WM_SIZE:
        cxScreen = LOWORD(lParam);
        cyScreen = HIWORD(lParam);
        break;

    case WM_PAINT:
        if (!(nWidthSetting & WIDTH_GLOBE))
        {
            /* start again on a black screen */
            for (i = 0; i < nWidth; i++)
                aLoops[i].fDrawn = FALSE;
            PatBlt(hdc, 0, 0, cxScreen, cyScreen, PATCOPY);
            iLoop = 0;
            pCurLoop = &aLoops[0];
        }
        else
        {
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            fGlobeClear = TRUE;
        }
        break;

    case WM_TIMER:
        if (!(nWidthSetting & WIDTH_GLOBE))
            StepBezier();
        else
            SetEvent(pGlobe->hEvent);
        break;

    case WM_QUERYNEWPALETTE:
        if (hPal)
        {
            SelectPalette(hdc, hPal, FALSE);
            RealizePalette(hdc);
            InvalidateRect(hWnd, NULL, TRUE);
            return TRUE;
        }
        return FALSE;

    case WM_PALETTECHANGED:
        RealizePalette(hdc);
        break;

    case WM_DESTROY:
        if (wTimer)
            KillTimer(hWnd, wTimer);
        if (hPal)
        {
            SelectPalette(hdc, hOldPal, FALSE);
            DeleteObject(hPal);
        }
        ReleaseDC(hWnd, hdc);
        break;
    }

    return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

/*-----------------------------------------------------------------------------
 * Name: ScreenSaverConfigureDialog()
 *---------------------------------------------------------------------------*/
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    TCHAR   szTemp[20];
    BOOL    fOK;
    UINT    nVal;
    int     idArrow;

    switch (message)
    {
    case WM_INITDIALOG:
        GetIniEntries();

        hSpeed  = GetDlgItem(hDlg, ID_SPEED);
        hOK     = GetDlgItem(hDlg, IDOK);
        hWidth  = GetDlgItem(hDlg, ID_WIDTH);
        hLength = GetDlgItem(hDlg, ID_LENGTH);

        SendMessage(hWidth, EM_LIMITTEXT, 3, 0);
        SendMessage(hLength, EM_LIMITTEXT, 3, 0);

        SetScrollRange(hSpeed, SB_CTL, MINSPEED, MAXSPEED, FALSE);
        SetScrollPos(hSpeed, SB_CTL, nSpeed, TRUE);

        SetDlgItemInt(hDlg, ID_WIDTH, nWidth, FALSE);
        SetDlgItemInt(hDlg, ID_LENGTH, nLength, FALSE);
        SendDlgItemMessage(hDlg, ID_LENGTHARROW, UDM_SETRANGE, 0, MAKELONG(MAXLENGTH, MINLENGTH));
        SendDlgItemMessage(hDlg, ID_WIDTHARROW, UDM_SETRANGE, 0, MAKELONG(MAXWIDTH, MINWIDTH));

        /* this writes Width without the globe flag */
        wsprintf(szTemp, TEXT("%d"), nWidth);
        WritePrivateProfileString(szAppName, szWidth, szTemp, szIniFile);
        return TRUE;

    case WM_HSCROLL:
        switch (LOWORD(wParam))
        {
        case SB_LINEUP:
        case SB_PAGEUP:
            --nSpeed;
            break;

        case SB_LINEDOWN:
        case SB_PAGEDOWN:
            ++nSpeed;
            break;

        case SB_THUMBPOSITION:
            nSpeed = HIWORD(wParam);
            break;

        case SB_TOP:
            nSpeed = MINSPEED;
            break;

        case SB_BOTTOM:
            nSpeed = MAXSPEED;
            break;

        case SB_THUMBTRACK:
        case SB_ENDSCROLL:
            return TRUE;
        }

        if (nSpeed <= MINSPEED)
            nSpeed = MINSPEED;
        else if (nSpeed >= MAXSPEED)
            nSpeed = MAXSPEED;

        SetScrollPos((HWND)lParam, SB_CTL, nSpeed, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_LENGTH:
        case ID_WIDTH:
            if (HIWORD(wParam) == EN_UPDATE)
            {
                if (LOWORD(wParam) == ID_LENGTH)
                {
                    nVal = GetDlgItemInt(hDlg, ID_LENGTH, &fOK, FALSE);
                    fOK = (nVal >= MINLENGTH && nVal <= MAXLENGTH);
                    idArrow = ID_LENGTHARROW;
                }
                else
                {
                    nVal = GetDlgItemInt(hDlg, ID_WIDTH, &fOK, FALSE);
                    fOK = (nVal >= MINWIDTH && nVal <= MAXWIDTH);
                    idArrow = ID_WIDTHARROW;
                }
                EnableWindow(GetDlgItem(hDlg, idArrow), fOK);
                EnableWindow(GetDlgItem(hDlg, IDOK), fOK);
            }
            break;

        case IDOK:
            /* keep the globe flag of the setting */
            nVal = GetDlgItemInt(hDlg, ID_WIDTH, &fOK, FALSE);
            nVal |= GetPrivateProfileInt(szAppName, szWidth, DEFWIDTH, szIniFile) & WIDTH_GLOBE;
            wsprintf(szTemp, TEXT("%d"), nVal);
            WritePrivateProfileString(szAppName, szWidth, szTemp, szIniFile);

            wsprintf(szTemp, TEXT("%d"), GetDlgItemInt(hDlg, ID_LENGTH, &fOK, FALSE));
            WritePrivateProfileString(szAppName, szLength, szTemp, szIniFile);

            wsprintf(szTemp, TEXT("%d"), nSpeed);
            WritePrivateProfileString(szAppName, szLineSpeed, szTemp, szIniFile);
            /* fall through */

        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam) == IDOK);
            return TRUE;
        }
        break;

    case WM_HELP:
        WinHelp((HWND)((LPHELPINFO)lParam)->hItemHandle, szHelpFile, HELP_WM_HELP,
                (ULONG_PTR)(LPVOID)aBezierHelpIds);
        break;

    case WM_CONTEXTMENU:
        WinHelp((HWND)wParam, szHelpFile, HELP_CONTEXTMENU, (ULONG_PTR)(LPVOID)aBezierHelpIds);
        break;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
 * Name: RegisterDialogClasses()
 * Desc: The dialog uses up-down controls.
 *---------------------------------------------------------------------------*/
BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
    InitCommonControls();
    return TRUE;
}
