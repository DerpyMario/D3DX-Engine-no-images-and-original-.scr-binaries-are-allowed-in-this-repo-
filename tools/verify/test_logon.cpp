//-----------------------------------------------------------------------------
// File: test_logon.cpp
//
// Desc: Differential test of the reconstructed Windows Logo screen saver
//       (logon.cpp + scrnsave.c) against the original logon.scr.
//
//           test_logon <path to the original logon.scr>
//
//       The addresses below belong to logon.scr 6.0.6001.18000 (amd64),
//       link time stamp 0x47919773.
//
//       Checked:
//         * Blur()              - bit-identical bitmaps
//         * OnPaint()           - the whole state machine, tic by tic: mode,
//                                 window position/size and layered alpha
//         * DefScreenSaverProc()- return value and "post WM_CLOSE" decision
//                                 for every interesting message
//-----------------------------------------------------------------------------
#include "origimage.h"
#include <scrnsave.h>

#define LOGON_TIMESTAMP             0x47919773

// functions
#define RVA_OnPaint                 0x1f60
#define RVA_Blur                    0x2164
#define RVA_DefScreenSaverProc      0x32f4

// globals of logon.obj
#define RVA_g_mode                  0x61d0
#define RVA_g_tics                  0x61d4
#define RVA_g_xScreen               0x67c8
#define RVA_g_cyScreen              0x67cc
#define RVA_g_hdcLogon              0x67d0
#define RVA_g_cxScreen              0x67d8
#define RVA_g_prevMode              0x67dc
#define RVA_g_hbmFlare              0x67e0
#define RVA_g_yScreen               0x6948
#define RVA_g_hbmLogo               0x6960
#define RVA_g_cxhwndLogon           0x6970
#define RVA_g_cyhwndLogon           0x6974
#define RVA_g_hwndLogon             0x6978
#define RVA_g_logoWidth             0x6980
#define RVA_g_logoHeight            0x6984
#define RVA_g_flareWidth            0x6988
#define RVA_g_flareHeight           0x698c
#define RVA_g_fServer               0x6990
#define RVA_g_fBatteries            0x6991

// globals of scrnsave.obj
#define RVA_fChildPreview           0x69a8
#define RVA_fClosing                0x69ac
#define RVA_fCheckingPassword       0x69b0
#define RVA_ptMouse                 0x6a18

typedef void     (*PFN_OnPaint)( HWND );
typedef void     (*PFN_Blur)( HWND, HBITMAP );
typedef LRESULT  (*PFN_DefScreenSaverProc)( HWND, UINT, WPARAM, LPARAM );

// the rebuilt code
enum MODE { MODE_BLANK = 0 };
#define NUM_FLARE_FRAMES 45

extern MODE     g_mode, g_prevMode;
extern int      g_tics;
extern HBITMAP  g_hbmFlare[NUM_FLARE_FRAMES];
extern HBITMAP  g_hbmLogo;
extern HDC      g_hdcLogon;
extern HWND     g_hwndLogon;
extern int      g_cxhwndLogon, g_cyhwndLogon, g_logoWidth, g_logoHeight, g_flareWidth, g_flareHeight;
extern int      g_xScreen, g_yScreen, g_cxScreen, g_cyScreen;
extern bool     g_fServer, g_fBatteries;

void OnPaint( HWND hwnd );
void Blur( HWND hwnd, HBITMAP hbm );

extern "C"
{
    extern BOOL  fClosing;
    extern BOOL  fCheckingPassword;
    extern POINT ptMouse;
}


//-----------------------------------------------------------------------------
// Name: MakeBitmap()
// Desc: Creates a DIB section filled with reproducible noise.
//-----------------------------------------------------------------------------
static HBITMAP MakeBitmap( int cx, int cy, int nBits, unsigned int seed, void** ppBits, int* pcbBits )
{
    BITMAPINFO bi;
    ZeroMemory( &bi, sizeof(bi) );
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = (WORD)nBits;
    bi.bmiHeader.biCompression = BI_RGB;

    HBITMAP hbm = CreateDIBSection( NULL, &bi, DIB_RGB_COLORS, ppBits, NULL, 0 );
    if( hbm == NULL )
        return NULL;

    int cbLine = ( ( cx * nBits + 31 ) / 32 ) * 4;
    *pcbBits = cbLine * cy;

    BYTE* p = (BYTE*)*ppBits;
    for( int i = 0; i < *pcbBits; i++ )
    {
        seed = seed * 1103515245u + 12345u;
        p[i] = (BYTE)( seed >> 16 );
    }
    return hbm;
}


//-----------------------------------------------------------------------------
// Name: TestBlur()
//-----------------------------------------------------------------------------
static void TestBlur( OrigImage& orig )
{
    static const struct { int cx, cy, nBits; } cases[] =
    {
        { 240, 160, 16 }, { 57, 33, 16 }, { 7, 7, 16 }, { 1024, 12, 16 }, { 130, 9, 16 }, { 301, 77, 16 },
        { 6, 40, 16 },          // too narrow: untouched
        { 1025, 8, 16 },        // too wide: untouched
        { 64, 64, 24 },         // wrong depth: untouched
        { 64, 64, 32 },
    };

    PFN_Blur pfnBlur = orig.At<PFN_Blur>( RVA_Blur );
    char msg[128];

    for( size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++ )
    {
        void* pBitsA;
        void* pBitsB;
        int cbA, cbB;
        HBITMAP hbmA = MakeBitmap( cases[i].cx, cases[i].cy, cases[i].nBits, 1234 + (unsigned int)i, &pBitsA, &cbA );
        HBITMAP hbmB = MakeBitmap( cases[i].cx, cases[i].cy, cases[i].nBits, 1234 + (unsigned int)i, &pBitsB, &cbB );

        // the original was specialised for its only caller and reads g_hbmLogo
        *orig.At<HBITMAP*>( RVA_g_hbmLogo ) = hbmA;
        pfnBlur( NULL, hbmA );
        Blur( NULL, hbmB );

        sprintf( msg, "Blur %dx%d %d bpp", cases[i].cx, cases[i].cy, cases[i].nBits );
        Check( cbA == cbB && memcmp( pBitsA, pBitsB, cbA ) == 0, msg );

        DeleteObject( hbmA );
        DeleteObject( hbmB );
    }
}


//-----------------------------------------------------------------------------
// The state machine
//-----------------------------------------------------------------------------
struct TickState
{
    int     mode, prevMode, tics, cx, cy;
    RECT    rc;
    BYTE    alpha;
};

static LRESULT CALLBACK TestWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    return DefWindowProc( hwnd, msg, wParam, lParam );
}

static void Snapshot( HWND hwnd, int mode, int prevMode, int tics, int cx, int cy, TickState* p )
{
    DWORD dwFlags = 0;
    COLORREF cr = 0;
    ZeroMemory( p, sizeof(*p) );
    p->mode = mode;
    p->prevMode = prevMode;
    p->tics = tics;
    p->cx = cx;
    p->cy = cy;
    GetWindowRect( hwnd, &p->rc );
    GetLayeredWindowAttributes( hwnd, &cr, &p->alpha, &dwFlags );
}

static void TestStateMachine( OrigImage& orig, HWND hwnd, bool bBatteries, bool bServer, BOOL bPreview,
                              unsigned int seed, const char* pszName )
{
    const int N = 3000;
    TickState* pA = new TickState[N];
    TickState* pB = new TickState[N];

    void* pBits;
    int cb;
    HBITMAP hbmFlare = MakeBitmap( 274, 144, 24, 1, &pBits, &cb );
    HBITMAP hbmLogo  = MakeBitmap( 200, 120, 16, 2, &pBits, &cb );
    HDC hdc = CreateCompatibleDC( NULL );

    PFN_OnPaint pfnOnPaint = orig.At<PFN_OnPaint>( RVA_OnPaint );

    // ---- original ----
    for( int i = 0; i < NUM_FLARE_FRAMES; i++ )
        orig.At<HBITMAP*>( RVA_g_hbmFlare )[i] = hbmFlare;
    *orig.At<HBITMAP*>( RVA_g_hbmLogo ) = hbmLogo;
    *orig.At<HDC*>( RVA_g_hdcLogon ) = hdc;
    *orig.At<HWND*>( RVA_g_hwndLogon ) = hwnd;
    *orig.At<int*>( RVA_g_mode ) = 1;
    *orig.At<int*>( RVA_g_prevMode ) = 0;
    *orig.At<int*>( RVA_g_tics ) = -1;
    *orig.At<int*>( RVA_g_cxhwndLogon ) = 1;
    *orig.At<int*>( RVA_g_cyhwndLogon ) = 1;
    *orig.At<int*>( RVA_g_logoWidth ) = 200;
    *orig.At<int*>( RVA_g_logoHeight ) = 120;
    *orig.At<int*>( RVA_g_flareWidth ) = 274;
    *orig.At<int*>( RVA_g_flareHeight ) = 144;
    *orig.At<int*>( RVA_g_xScreen ) = -100;
    *orig.At<int*>( RVA_g_yScreen ) = 20;
    *orig.At<int*>( RVA_g_cxScreen ) = 1920;
    *orig.At<int*>( RVA_g_cyScreen ) = 1080;
    *orig.At<bool*>( RVA_g_fServer ) = bServer;
    *orig.At<bool*>( RVA_g_fBatteries ) = bBatteries;
    *orig.At<BOOL*>( RVA_fChildPreview ) = bPreview;

    SetWindowPos( hwnd, NULL, 5, 5, 1, 1, SWP_NOZORDER | SWP_NOACTIVATE );
    SetLayeredWindowAttributes( hwnd, 0, 0, LWA_ALPHA );
    srand( seed );
    for( int i = 0; i < N; i++ )
    {
        InvalidateRect( hwnd, NULL, TRUE );
        pfnOnPaint( hwnd );
        Snapshot( hwnd, *orig.At<int*>( RVA_g_mode ), *orig.At<int*>( RVA_g_prevMode ), *orig.At<int*>( RVA_g_tics ),
                  *orig.At<int*>( RVA_g_cxhwndLogon ), *orig.At<int*>( RVA_g_cyhwndLogon ), &pA[i] );
    }

    // ---- rebuilt ----
    for( int i = 0; i < NUM_FLARE_FRAMES; i++ )
        g_hbmFlare[i] = hbmFlare;
    g_hbmLogo = hbmLogo;
    g_hdcLogon = hdc;
    g_hwndLogon = hwnd;
    g_mode = (MODE)1;
    g_prevMode = (MODE)0;
    g_tics = -1;
    g_cxhwndLogon = 1;
    g_cyhwndLogon = 1;
    g_logoWidth = 200;
    g_logoHeight = 120;
    g_flareWidth = 274;
    g_flareHeight = 144;
    g_xScreen = -100;
    g_yScreen = 20;
    g_cxScreen = 1920;
    g_cyScreen = 1080;
    g_fServer = bServer;
    g_fBatteries = bBatteries;
    fChildPreview = bPreview;

    SetWindowPos( hwnd, NULL, 5, 5, 1, 1, SWP_NOZORDER | SWP_NOACTIVATE );
    SetLayeredWindowAttributes( hwnd, 0, 0, LWA_ALPHA );
    srand( seed );
    for( int i = 0; i < N; i++ )
    {
        InvalidateRect( hwnd, NULL, TRUE );
        OnPaint( hwnd );
        Snapshot( hwnd, g_mode, g_prevMode, g_tics, g_cxhwndLogon, g_cyhwndLogon, &pB[i] );
    }

    int nFirstDiff = -1;
    int nModeChanges = 0;
    for( int i = 0; i < N; i++ )
    {
        if( i > 0 && pA[i].mode != pA[i - 1].mode )
            nModeChanges++;
        if( nFirstDiff < 0 && memcmp( &pA[i], &pB[i], sizeof(TickState) ) != 0 )
            nFirstDiff = i;
    }

    char msg[256];
    if( nFirstDiff >= 0 )
    {
        sprintf( msg, "state machine (%s): first difference at tic %d: mode %d/%d tics %d/%d pos %ld,%ld / %ld,%ld alpha %d/%d",
                 pszName, nFirstDiff, pA[nFirstDiff].mode, pB[nFirstDiff].mode, pA[nFirstDiff].tics, pB[nFirstDiff].tics,
                 pA[nFirstDiff].rc.left, pA[nFirstDiff].rc.top, pB[nFirstDiff].rc.left, pB[nFirstDiff].rc.top,
                 pA[nFirstDiff].alpha, pB[nFirstDiff].alpha );
    }
    else
    {
        sprintf( msg, "state machine (%s): %d tics, %d mode changes", pszName, N, nModeChanges );
    }
    Check( nFirstDiff < 0 && nModeChanges > 10, msg );

    fChildPreview = FALSE;
    *orig.At<BOOL*>( RVA_fChildPreview ) = FALSE;
    DeleteDC( hdc );
    DeleteObject( hbmFlare );
    DeleteObject( hbmLogo );
    delete[] pA;
    delete[] pB;
}


//-----------------------------------------------------------------------------
// Name: TestDefScreenSaverProc()
//-----------------------------------------------------------------------------
static bool ClosePosted( HWND hwnd )
{
    MSG msg;
    bool bPosted = false;
    while( PeekMessage( &msg, hwnd, WM_CLOSE, WM_CLOSE, PM_REMOVE ) )
        bPosted = true;
    return bPosted;
}

static void TestDefScreenSaverProc( OrigImage& orig, HWND hwnd )
{
    PFN_DefScreenSaverProc pfnDef = orig.At<PFN_DefScreenSaverProc>( RVA_DefScreenSaverProc );

    struct Case { UINT msg; WPARAM wParam; LPARAM lParam; };
    Case cases[200];
    int n = 0;

    static const UINT simple[] = { WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP, WM_CHAR, WM_LBUTTONDOWN, WM_LBUTTONUP,
                                   WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_MOUSEWHEEL, WM_SETCURSOR, WM_NCHITTEST, WM_TIMER,
                                   WM_USER, WM_APP + 1, WM_ERASEBKGND, WM_GETMINMAXINFO, WM_ENABLE, WM_CANCELMODE };
    for( size_t i = 0; i < sizeof(simple) / sizeof(simple[0]); i++ )
        cases[n++] = { simple[i], 0, 0 };
    for( WPARAM w = 0; w < 2; w++ )
    {
        cases[n++] = { WM_ACTIVATEAPP, w, 0 };
        cases[n++] = { WM_SHOWWINDOW, w, 0 };
        cases[n++] = { WM_ACTIVATE, w, 0 };
    }
    for( WPARAM w = 0; w < 6; w++ )
        cases[n++] = { WM_POWER, w, 0 };
    for( WPARAM w = 0; w < 0x14; w++ )
    {
        cases[n++] = { WM_POWERBROADCAST, w, 0 };
        cases[n++] = { WM_POWERBROADCAST, w, 1 };
    }
    cases[n++] = { WM_POWERBROADCAST, 0x8013, 0 };
    cases[n++] = { WM_POWERBROADCAST, 0x8014, 0 };
    cases[n++] = { WM_SYSCOMMAND, SC_SCREENSAVE, 0 };
    cases[n++] = { SCRM_VERIFYPW, 0, 0 };

    char msg[128];
    int nBad = 0;

    // three configurations: normal, preview, while checking the password
    for( int config = 0; config < 3; config++ )
    {
        BOOL bPreview = ( config == 1 );
        BOOL bChecking = ( config == 2 );

        for( int i = 0; i < n; i++ )
        {
            *orig.At<BOOL*>( RVA_fChildPreview ) = bPreview;
            *orig.At<BOOL*>( RVA_fClosing ) = FALSE;
            *orig.At<BOOL*>( RVA_fCheckingPassword ) = bChecking;
            LRESULT lA = pfnDef( hwnd, cases[i].msg, cases[i].wParam, cases[i].lParam );
            bool bCloseA = ClosePosted( hwnd );

            fChildPreview = bPreview;
            fClosing = FALSE;
            fCheckingPassword = bChecking;
            LRESULT lB = DefScreenSaverProc( hwnd, cases[i].msg, cases[i].wParam, cases[i].lParam );
            bool bCloseB = ClosePosted( hwnd );

            if( lA != lB || bCloseA != bCloseB )
            {
                if( nBad++ < 5 )
                    printf( "    config %d msg 0x%x wParam 0x%x lParam %d: ret %d/%d close %d/%d\n", config, cases[i].msg,
                            (unsigned)cases[i].wParam, (int)cases[i].lParam, (int)lA, (int)lB, bCloseA, bCloseB );
            }
        }
    }
    sprintf( msg, "DefScreenSaverProc: %d messages x 3 configurations", n );
    Check( nBad == 0, msg );

    // mouse movement threshold
    static const POINT offsets[] = { { 0, 0 }, { 2, 2 }, { 3, 1 }, { 3, 2 }, { -5, 0 }, { 0, 4 }, { 0, -5 }, { -2, -2 }, { 40, 40 } };
    nBad = 0;
    for( size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++ )
    {
        POINT ptNow;
        GetCursorPos( &ptNow );
        POINT ptRef = { ptNow.x + offsets[i].x, ptNow.y + offsets[i].y };

        *orig.At<BOOL*>( RVA_fChildPreview ) = FALSE;
        *orig.At<BOOL*>( RVA_fClosing ) = FALSE;
        *orig.At<BOOL*>( RVA_fCheckingPassword ) = FALSE;
        *orig.At<POINT*>( RVA_ptMouse ) = ptRef;
        LRESULT lA = pfnDef( hwnd, WM_MOUSEMOVE, 0, 0 );
        bool bCloseA = ClosePosted( hwnd );
        POINT ptA = *orig.At<POINT*>( RVA_ptMouse );

        fChildPreview = FALSE;
        fClosing = FALSE;
        fCheckingPassword = FALSE;
        ptMouse = ptRef;
        LRESULT lB = DefScreenSaverProc( hwnd, WM_MOUSEMOVE, 0, 0 );
        bool bCloseB = ClosePosted( hwnd );

        if( lA != lB || bCloseA != bCloseB || ptA.x != ptMouse.x || ptA.y != ptMouse.y )
            nBad++;
    }
    Check( nBad == 0, "DefScreenSaverProc: mouse wake threshold" );
}


//-----------------------------------------------------------------------------
// Name: main()
//-----------------------------------------------------------------------------
int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        printf( "usage: test_logon <original logon.scr>\n" );
        return 2;
    }

    OrigImage orig;
    if( !orig.Load( argv[1], LOGON_TIMESTAMP ) )
        return 2;

    WNDCLASS wc;
    ZeroMemory( &wc, sizeof(wc) );
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = GetModuleHandle( NULL );
    wc.lpszClassName = L"TestLogon";
    RegisterClass( &wc );
    HWND hwnd = CreateWindowEx( WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"TestLogon", NULL, WS_POPUP, 5, 5, 1, 1,
                                NULL, NULL, wc.hInstance, NULL );
    if( hwnd == NULL )
    {
        printf( "cannot create the test window\n" );
        return 2;
    }

    TestBlur( orig );

    TestStateMachine( orig, hwnd, false, false, FALSE, 1, "full screen" );
    TestStateMachine( orig, hwnd, false, false, FALSE, 77, "full screen, other seed" );
    TestStateMachine( orig, hwnd, true, false, FALSE, 2, "on batteries" );
    TestStateMachine( orig, hwnd, false, true, FALSE, 3, "server" );
    TestStateMachine( orig, hwnd, false, false, TRUE, 4, "preview" );

    TestDefScreenSaverProc( orig, hwnd );

    DestroyWindow( hwnd );
    return Summary( "test_logon" );
}
