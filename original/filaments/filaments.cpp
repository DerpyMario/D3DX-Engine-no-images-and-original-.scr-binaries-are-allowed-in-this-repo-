//-----------------------------------------------------------------------------
// filaments.cpp - "Filaments", an original Direct3D 9 screen saver.
//
// Original work for this repository (not a reconstruction of any shipping
// screen saver).  Strands of light are advected through a slowly evolving
// curl-noise flow field and leave fading trails, drawn either as thin lines
// or as wide camera-facing ribbons.  The look is built from the three
// ingredients described in docs/mystify-ribbons-notes.md -- additive
// blending, soft per-vertex falloff, and a feedback buffer for the trails --
// but the design, maths and code here are my own.
//
// Standard screen-saver command line:
//     /s          run full screen
//     /p <hwnd>   preview inside the given window
//     /c[:<hwnd>] configure (modal dialog)
//
// Build: see original/CMakeLists.txt (links d3d9, comctl32; no D3DX).
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <d3d9.h>
#include <stdlib.h>
#include <time.h>
#include "vecmath.h"
#include "flowfield.h"
#include "resource.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

//-----------------------------------------------------------------------------
// Tunables and settings
//-----------------------------------------------------------------------------
#define REG_PATH    L"Software\\D3DXEngine\\Filaments"

static const int   TRAIL_LEN = 48;      // points kept per strand
static const int   MAX_FIL   = 400;

enum { MODE_LINES = 0, MODE_RIBBONS = 1 };

struct Settings
{
    int   mode;         // MODE_LINES or MODE_RIBBONS
    int   nCount;       // number of strands (1 .. MAX_FIL)
    float fWidth;       // strand width, 0.01 .. 1.0
    float fFov;         // camera field of view, 0.5 .. 3.0 radians
    float fFade;        // trail length, 0 (short) .. 1 (long)
    bool  bSpan;        // span the whole virtual desktop
    bool  bRainbow;     // cycle hue across the strands
};

static Settings g_settings = { MODE_RIBBONS, 120, 0.35f, 0.9f, 0.85f, false, true };

//-----------------------------------------------------------------------------
// Vertex formats
//-----------------------------------------------------------------------------
struct RibbonVertex { float x, y, z; D3DCOLOR color; };
#define FVF_RIBBON (D3DFVF_XYZ | D3DFVF_DIFFUSE)

struct ScreenVertex { float x, y, z, rhw; D3DCOLOR color; float u, v; };
#define FVF_SCREEN (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

//-----------------------------------------------------------------------------
// One strand: a ring of recent points plus a moving head
//-----------------------------------------------------------------------------
struct Filament
{
    Vec3  pt[TRAIL_LEN];
    float hue;

    void Seed( int index, int total )
    {
        float a = (float)rand() / RAND_MAX * 6.2831853f;
        float r = 1.0f + (float)rand() / RAND_MAX * 3.0f;
        float y = ( (float)rand() / RAND_MAX - 0.5f ) * 4.0f;
        Vec3 p( cosf( a ) * r, y, sinf( a ) * r );
        for( int i = 0; i < TRAIL_LEN; i++ )
            pt[i] = p;
        hue = (float)index / ( total > 0 ? total : 1 );
    }

    void Advance( FlowField& field, float t, float dt, float speed )
    {
        Vec3 head = pt[0];
        Vec3 v = field.Flow( head, t ) * speed;
        Vec3 next = head + v * dt;

        float len = Length( next );
        if( len > 6.0f )
            next = next * ( 6.0f / len );

        for( int i = TRAIL_LEN - 1; i > 0; i-- )
            pt[i] = pt[i - 1];
        pt[0] = next;
    }
};

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static IDirect3D9*           g_pD3D        = NULL;
static IDirect3DDevice9*     g_pDevice     = NULL;
static IDirect3DSurface9*    g_pBackBuffer = NULL;
static IDirect3DTexture9*    g_pTrailTex[2]  = { NULL, NULL };
static IDirect3DSurface9*    g_pTrailSurf[2] = { NULL, NULL };
static int                   g_cur = 0;
static int                   g_width = 0, g_height = 0;

static FlowField*            g_pField = NULL;
static Filament*             g_pFil   = NULL;
static float                 g_time   = 0.0f;

static HWND                  g_hWnd = NULL;
static bool                  g_preview = false;
static POINT                 g_lastMouse = { -1, -1 };

//-----------------------------------------------------------------------------
// Settings persistence (my own value names, under my own key)
//-----------------------------------------------------------------------------
static void Clamp()
{
    if( g_settings.mode != MODE_LINES && g_settings.mode != MODE_RIBBONS )
        g_settings.mode = MODE_RIBBONS;
    if( g_settings.nCount < 1 )       g_settings.nCount = 1;
    if( g_settings.nCount > MAX_FIL ) g_settings.nCount = MAX_FIL;
    if( g_settings.fWidth < 0.01f )   g_settings.fWidth = 0.01f;
    if( g_settings.fWidth > 1.0f )    g_settings.fWidth = 1.0f;
    if( g_settings.fFov < 0.5f )      g_settings.fFov = 0.5f;
    if( g_settings.fFov > 3.0f )      g_settings.fFov = 3.0f;
    if( g_settings.fFade < 0.0f )     g_settings.fFade = 0.0f;
    if( g_settings.fFade > 1.0f )     g_settings.fFade = 1.0f;
}

static void LoadSettings()
{
    HKEY hKey;
    if( RegOpenKeyExW( HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey ) != ERROR_SUCCESS )
        return;

    DWORD v, cb;
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"Mode",        NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.mode    = (int)v;
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"Count",       NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.nCount  = (int)v;
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"Width",       NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.fWidth  = (int)v / 100.0f;
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"FieldOfView", NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.fFov    = (int)v / 100.0f;
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"TrailLength", NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.fFade   = (int)v / 100.0f;
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"SpanMonitors",NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.bSpan   = ( v != 0 );
    cb = sizeof(v); if( RegQueryValueExW( hKey, L"CycleColours",NULL, NULL, (LPBYTE)&v, &cb ) == ERROR_SUCCESS ) g_settings.bRainbow = ( v != 0 );
    RegCloseKey( hKey );
    Clamp();
}

static void SaveSettings()
{
    HKEY hKey;
    if( RegCreateKeyExW( HKEY_CURRENT_USER, REG_PATH, 0, NULL, 0, KEY_WRITE, NULL,
                         &hKey, NULL ) != ERROR_SUCCESS )
        return;

    DWORD v;
    v = (DWORD)g_settings.mode;                        RegSetValueExW( hKey, L"Mode",         0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    v = (DWORD)g_settings.nCount;                      RegSetValueExW( hKey, L"Count",        0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    v = (DWORD)( g_settings.fWidth * 100.0f + 0.5f );  RegSetValueExW( hKey, L"Width",        0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    v = (DWORD)( g_settings.fFov   * 100.0f + 0.5f );  RegSetValueExW( hKey, L"FieldOfView",  0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    v = (DWORD)( g_settings.fFade  * 100.0f + 0.5f );  RegSetValueExW( hKey, L"TrailLength",  0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    v = g_settings.bSpan ? 1 : 0;                      RegSetValueExW( hKey, L"SpanMonitors", 0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    v = g_settings.bRainbow ? 1 : 0;                   RegSetValueExW( hKey, L"CycleColours", 0, REG_DWORD, (LPBYTE)&v, sizeof(v) );
    RegCloseKey( hKey );
}

//-----------------------------------------------------------------------------
// Scene
//-----------------------------------------------------------------------------
static void InitScene()
{
    srand( (unsigned)time( NULL ) );
    delete g_pField;
    g_pField = new FlowField( (unsigned)rand() * 2654435761u + 1u );

    delete[] g_pFil;
    g_pFil = new Filament[g_settings.nCount];
    for( int i = 0; i < g_settings.nCount; i++ )
        g_pFil[i].Seed( i, g_settings.nCount );

    g_time = 0.0f;
}

static void UpdateScene( float dt )
{
    g_time += dt;
    const float speed = 1.2f;
    for( int i = 0; i < g_settings.nCount; i++ )
        g_pFil[i].Advance( *g_pField, g_time, dt, speed );
}

//-----------------------------------------------------------------------------
// Render-target textures for the trail feedback
//-----------------------------------------------------------------------------
static bool CreateTargets()
{
    for( int i = 0; i < 2; i++ )
    {
        if( FAILED( g_pDevice->CreateTexture( g_width, g_height, 1, D3DUSAGE_RENDERTARGET,
                                              D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                              &g_pTrailTex[i], NULL ) ) )
            return false;
        g_pTrailTex[i]->GetSurfaceLevel( 0, &g_pTrailSurf[i] );

        g_pDevice->SetRenderTarget( 0, g_pTrailSurf[i] );
        g_pDevice->SetDepthStencilSurface( NULL );
        g_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB( 255, 0, 0, 0 ), 1.0f, 0 );
    }
    g_pDevice->SetRenderTarget( 0, g_pBackBuffer );
    return true;
}

static void ReleaseTargets()
{
    for( int i = 0; i < 2; i++ )
    {
        if( g_pTrailSurf[i] ) { g_pTrailSurf[i]->Release(); g_pTrailSurf[i] = NULL; }
        if( g_pTrailTex[i] )  { g_pTrailTex[i]->Release();  g_pTrailTex[i]  = NULL; }
    }
}

//-----------------------------------------------------------------------------
// Device init / teardown
//-----------------------------------------------------------------------------
static bool InitDevice( HWND hWnd )
{
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !g_pD3D )
        return false;

    D3DDISPLAYMODE mode;
    g_pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &mode );

    RECT rc;
    GetClientRect( hWnd, &rc );
    g_width  = rc.right  - rc.left;
    g_height = rc.bottom - rc.top;
    if( g_width < 1 )  g_width = 1;
    if( g_height < 1 ) g_height = 1;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed             = TRUE;
    pp.SwapEffect           = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat     = mode.Format;
    pp.BackBufferWidth      = g_width;
    pp.BackBufferHeight     = g_height;
    pp.hDeviceWindow        = hWnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, flags, &pp,
                                      &g_pDevice ) ) )
    {
        flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        if( FAILED( g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, flags, &pp,
                                          &g_pDevice ) ) )
            return false;
    }

    g_pDevice->GetRenderTarget( 0, &g_pBackBuffer );
    return CreateTargets();
}

static void CleanupDevice()
{
    ReleaseTargets();
    if( g_pBackBuffer ) { g_pBackBuffer->Release(); g_pBackBuffer = NULL; }
    if( g_pDevice )     { g_pDevice->Release();     g_pDevice = NULL; }
    if( g_pD3D )        { g_pD3D->Release();         g_pD3D = NULL; }
}

//-----------------------------------------------------------------------------
// A screen-space quad covering the whole target
//-----------------------------------------------------------------------------
static void DrawFullScreenQuad( IDirect3DTexture9* tex, D3DCOLOR color )
{
    float w = (float)g_width, h = (float)g_height;
    ScreenVertex v[4] =
    {
        { -0.5f,     -0.5f,     0.0f, 1.0f, color, 0.0f, 0.0f },
        { w - 0.5f,  -0.5f,     0.0f, 1.0f, color, 1.0f, 0.0f },
        { w - 0.5f,  h - 0.5f,  0.0f, 1.0f, color, 1.0f, 1.0f },
        { -0.5f,     h - 0.5f,  0.0f, 1.0f, color, 0.0f, 1.0f },
    };

    g_pDevice->SetTexture( 0, tex );
    g_pDevice->SetFVF( FVF_SCREEN );
    g_pDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, v, sizeof(ScreenVertex) );
}

//-----------------------------------------------------------------------------
// Draw one strand as a camera-facing triangle strip.  The strand width scale
// is what distinguishes the two modes: narrow threads for Lines, broad bands
// for Ribbons.
//-----------------------------------------------------------------------------
static void DrawFilament( const Filament& f, const Vec3& eye, float halfWidth )
{
    RibbonVertex strip[TRAIL_LEN * 2];

    for( int i = 0; i < TRAIL_LEN; i++ )
    {
        const Vec3& p = f.pt[i];

        Vec3 tangent;
        if( i == 0 )                  tangent = f.pt[0] - f.pt[1];
        else if( i == TRAIL_LEN - 1 ) tangent = f.pt[i - 1] - f.pt[i];
        else                          tangent = f.pt[i - 1] - f.pt[i + 1];
        tangent = Normalize( tangent );

        Vec3 toEye = Normalize( eye - p );
        Vec3 side  = Normalize( Cross( tangent, toEye ) ) * halfWidth;

        float age = (float)i / ( TRAIL_LEN - 1 );   // 0 head .. 1 tail
        float bright = ( 1.0f - age );
        bright = bright * bright;                    // sharper falloff toward the tail

        float hue = f.hue;
        if( g_settings.bRainbow )
            hue = f.hue + g_time * 0.03f;

        D3DCOLOR c = HsvToColor( hue, 0.55f, 1.0f, bright );

        strip[i * 2 + 0] = RibbonVertex{ p.x + side.x, p.y + side.y, p.z + side.z, c };
        strip[i * 2 + 1] = RibbonVertex{ p.x - side.x, p.y - side.y, p.z - side.z, c };
    }

    g_pDevice->SetFVF( FVF_RIBBON );
    g_pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, TRAIL_LEN * 2 - 2, strip, sizeof(RibbonVertex) );
}

//-----------------------------------------------------------------------------
// One rendered frame
//-----------------------------------------------------------------------------
static void RenderFrame()
{
    if( !g_pDevice )
        return;

    float t = g_time;
    Vec3 eye( cosf( t * 0.07f ) * 6.0f, sinf( t * 0.05f ) * 2.5f, sinf( t * 0.07f ) * 6.0f );
    Vec3 at( 0, 0, 0 ), up( 0, 1, 0 );

    D3DMATRIX world = MatIdentity();
    D3DMATRIX view  = MatLookAtLH( eye, at, up );
    D3DMATRIX proj  = MatPerspectiveFovLH( g_settings.fFov, (float)g_width / g_height, 0.1f, 100.0f );

    // trail persistence: longer trail -> less darkening of the fed-back frame
    int fadeByte = 200 + (int)( g_settings.fFade * 54.0f );
    if( fadeByte > 254 ) fadeByte = 254;
    D3DCOLOR fade = D3DCOLOR_ARGB( 255, fadeByte, fadeByte, fadeByte );

    float halfWidth = ( g_settings.mode == MODE_LINES )
                        ? g_settings.fWidth * 0.015f
                        : g_settings.fWidth * 0.07f;

    int prev = g_cur ^ 1;

    g_pDevice->SetRenderTarget( 0, g_pTrailSurf[g_cur] );
    g_pDevice->SetDepthStencilSurface( NULL );

    g_pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    g_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    g_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    g_pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    g_pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    if( g_pDevice->BeginScene() >= 0 )
    {
        // feedback: the previous frame, slightly darkened
        g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        g_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
        g_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        DrawFullScreenQuad( g_pTrailTex[prev], fade );

        // the strands, additively, in 3D
        g_pDevice->SetTransform( D3DTS_WORLD, &world );
        g_pDevice->SetTransform( D3DTS_VIEW, &view );
        g_pDevice->SetTransform( D3DTS_PROJECTION, &proj );
        g_pDevice->SetTexture( 0, NULL );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
        g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        g_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
        g_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

        for( int i = 0; i < g_settings.nCount; i++ )
            DrawFilament( g_pFil[i], eye, halfWidth );

        g_pDevice->EndScene();
    }

    // present the current trail texture to the back buffer
    g_pDevice->SetRenderTarget( 0, g_pBackBuffer );
    if( g_pDevice->BeginScene() >= 0 )
    {
        g_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        g_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        DrawFullScreenQuad( g_pTrailTex[g_cur], D3DCOLOR_ARGB( 255, 255, 255, 255 ) );
        g_pDevice->EndScene();
    }

    g_cur ^= 1;

    if( g_pDevice->Present( NULL, NULL, NULL, NULL ) == D3DERR_DEVICELOST )
    {
        if( g_pDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET )
        {
            ReleaseTargets();
            if( g_pBackBuffer ) { g_pBackBuffer->Release(); g_pBackBuffer = NULL; }

            D3DDISPLAYMODE mode;
            g_pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &mode );
            D3DPRESENT_PARAMETERS pp = {};
            pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.BackBufferFormat = mode.Format;
            pp.BackBufferWidth = g_width; pp.BackBufferHeight = g_height;
            pp.hDeviceWindow = g_hWnd; pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
            if( SUCCEEDED( g_pDevice->Reset( &pp ) ) )
            {
                g_pDevice->GetRenderTarget( 0, &g_pBackBuffer );
                CreateTargets();
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Saver window
//-----------------------------------------------------------------------------
static LRESULT CALLBACK SaverWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_MOUSEMOVE:
            if( !g_preview )
            {
                POINT pt;
                GetCursorPos( &pt );
                if( g_lastMouse.x < 0 )
                    g_lastMouse = pt;
                else if( abs( pt.x - g_lastMouse.x ) > 3 || abs( pt.y - g_lastMouse.y ) > 3 )
                    PostMessageW( hWnd, WM_CLOSE, 0, 0 );
            }
            return 0;

        case WM_KEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if( !g_preview )
                PostMessageW( hWnd, WM_CLOSE, 0, 0 );
            return 0;

        case WM_ACTIVATE:
            if( !g_preview && LOWORD( wParam ) == WA_INACTIVE )
                PostMessageW( hWnd, WM_CLOSE, 0, 0 );
            return 0;

        case WM_SYSCOMMAND:
            if( !g_preview && ( wParam == SC_SCREENSAVE || wParam == SC_CLOSE ) )
                return 0;
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            return 0;
    }
    return DefWindowProcW( hWnd, msg, wParam, lParam );
}

//-----------------------------------------------------------------------------
// The common run loop (full screen or preview)
//-----------------------------------------------------------------------------
static int RunSaver( HINSTANCE hInst, HWND hwndParent, bool preview )
{
    g_preview = preview;

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = SaverWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = preview ? LoadCursor( NULL, IDC_ARROW ) : NULL;
    wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
    wc.lpszClassName = L"FilamentsSaverWnd";
    RegisterClassW( &wc );

    if( preview )
    {
        RECT rc;
        GetClientRect( hwndParent, &rc );
        g_hWnd = CreateWindowExW( 0, wc.lpszClassName, L"Filaments",
                                  WS_CHILD | WS_VISIBLE, 0, 0, rc.right, rc.bottom,
                                  hwndParent, NULL, hInst, NULL );
    }
    else
    {
        int x, y, cx, cy;
        if( g_settings.bSpan )
        {
            x  = GetSystemMetrics( SM_XVIRTUALSCREEN );
            y  = GetSystemMetrics( SM_YVIRTUALSCREEN );
            cx = GetSystemMetrics( SM_CXVIRTUALSCREEN );
            cy = GetSystemMetrics( SM_CYVIRTUALSCREEN );
        }
        else
        {
            x = 0; y = 0;
            cx = GetSystemMetrics( SM_CXSCREEN );
            cy = GetSystemMetrics( SM_CYSCREEN );
        }
        g_hWnd = CreateWindowExW( WS_EX_TOPMOST, wc.lpszClassName, L"Filaments",
                                  WS_POPUP | WS_VISIBLE, x, y, cx, cy,
                                  NULL, NULL, hInst, NULL );
        ShowCursor( FALSE );
        SetForegroundWindow( g_hWnd );
    }

    if( !g_hWnd || !InitDevice( g_hWnd ) )
    {
        CleanupDevice();
        if( !preview ) ShowCursor( TRUE );
        return 1;
    }

    InitScene();

    ULONGLONG last = GetTickCount64();
    MSG msg;
    bool running = true;
    while( running )
    {
        while( PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            if( msg.message == WM_QUIT )
                running = false;
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        if( !running )
            break;

        if( preview && !IsWindow( hwndParent ) )
            break;

        ULONGLONG now = GetTickCount64();
        float dt = ( now - last ) * 0.001f;
        last = now;
        if( dt > 0.1f ) dt = 0.1f;

        UpdateScene( dt );
        RenderFrame();
        Sleep( 5 );
    }

    CleanupDevice();
    delete[] g_pFil;  g_pFil = NULL;
    delete g_pField;  g_pField = NULL;
    if( !preview ) ShowCursor( TRUE );
    return 0;
}

//-----------------------------------------------------------------------------
// Configuration dialog
//-----------------------------------------------------------------------------
static INT_PTR CALLBACK ConfigProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_INITDIALOG:
            LoadSettings();
            CheckRadioButton( hDlg, IDC_MODE_LINES, IDC_MODE_RIBBONS,
                              g_settings.mode == MODE_LINES ? IDC_MODE_LINES : IDC_MODE_RIBBONS );
            SendDlgItemMessageW( hDlg, IDC_COUNT, TBM_SETRANGE, TRUE, MAKELONG( 1, MAX_FIL ) );
            SendDlgItemMessageW( hDlg, IDC_COUNT, TBM_SETPOS, TRUE, g_settings.nCount );
            SendDlgItemMessageW( hDlg, IDC_WIDTH, TBM_SETRANGE, TRUE, MAKELONG( 1, 100 ) );
            SendDlgItemMessageW( hDlg, IDC_WIDTH, TBM_SETPOS, TRUE, (int)( g_settings.fWidth * 100.0f + 0.5f ) );
            SendDlgItemMessageW( hDlg, IDC_FOV, TBM_SETRANGE, TRUE, MAKELONG( 50, 300 ) );
            SendDlgItemMessageW( hDlg, IDC_FOV, TBM_SETPOS, TRUE, (int)( g_settings.fFov * 100.0f + 0.5f ) );
            SendDlgItemMessageW( hDlg, IDC_FADE, TBM_SETRANGE, TRUE, MAKELONG( 0, 100 ) );
            SendDlgItemMessageW( hDlg, IDC_FADE, TBM_SETPOS, TRUE, (int)( g_settings.fFade * 100.0f + 0.5f ) );
            CheckDlgButton( hDlg, IDC_SPAN, g_settings.bSpan ? BST_CHECKED : BST_UNCHECKED );
            CheckDlgButton( hDlg, IDC_RAINBOW, g_settings.bRainbow ? BST_CHECKED : BST_UNCHECKED );
            return TRUE;

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDOK:
                    g_settings.mode    = ( IsDlgButtonChecked( hDlg, IDC_MODE_LINES ) == BST_CHECKED )
                                            ? MODE_LINES : MODE_RIBBONS;
                    g_settings.nCount  = (int)SendDlgItemMessageW( hDlg, IDC_COUNT, TBM_GETPOS, 0, 0 );
                    g_settings.fWidth  = (int)SendDlgItemMessageW( hDlg, IDC_WIDTH, TBM_GETPOS, 0, 0 ) / 100.0f;
                    g_settings.fFov    = (int)SendDlgItemMessageW( hDlg, IDC_FOV, TBM_GETPOS, 0, 0 ) / 100.0f;
                    g_settings.fFade   = (int)SendDlgItemMessageW( hDlg, IDC_FADE, TBM_GETPOS, 0, 0 ) / 100.0f;
                    g_settings.bSpan    = ( IsDlgButtonChecked( hDlg, IDC_SPAN ) == BST_CHECKED );
                    g_settings.bRainbow = ( IsDlgButtonChecked( hDlg, IDC_RAINBOW ) == BST_CHECKED );
                    Clamp();
                    SaveSettings();
                    EndDialog( hDlg, IDOK );
                    return TRUE;

                case IDCANCEL:
                    EndDialog( hDlg, IDCANCEL );
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

//-----------------------------------------------------------------------------
// Command line
//-----------------------------------------------------------------------------
static HWND ParsePreviewParent( LPCWSTR cmd )
{
    const WCHAR* p = cmd;
    const WCHAR* lastDigit = NULL;
    for( ; *p; p++ )
        if( ( *p >= L'0' && *p <= L'9' ) && ( lastDigit == NULL || !( p[-1] >= L'0' && p[-1] <= L'9' ) ) )
            lastDigit = p;
    if( !lastDigit )
        return NULL;
    return (HWND)(ULONG_PTR)_wtoi64( lastDigit );
}

int WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, int )
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx( &icc );
    LoadSettings();

    // the arguments, as wide text, skipping the program name
    LPCWSTR lpCmdLine = GetCommandLineW();
    if( *lpCmdLine == L'"' )
    {
        lpCmdLine++;
        while( *lpCmdLine && *lpCmdLine != L'"' ) lpCmdLine++;
        if( *lpCmdLine ) lpCmdLine++;
    }
    else
    {
        while( *lpCmdLine && *lpCmdLine != L' ' ) lpCmdLine++;
    }

    WCHAR mode = L'c';
    for( const WCHAR* p = lpCmdLine; *p; p++ )
    {
        if( *p == L'/' || *p == L'-' )
        {
            WCHAR c = p[1];
            if( c == L's' || c == L'S' ) { mode = L's'; break; }
            if( c == L'p' || c == L'P' ) { mode = L'p'; break; }
            if( c == L'c' || c == L'C' ) { mode = L'c'; break; }
        }
    }

    if( mode == L's' )
        return RunSaver( hInst, NULL, false );

    if( mode == L'p' )
    {
        HWND parent = ParsePreviewParent( lpCmdLine );
        if( parent && IsWindow( parent ) )
            return RunSaver( hInst, parent, true );
        return 0;
    }

    DialogBoxParamW( hInst, MAKEINTRESOURCEW( IDD_CONFIG ), GetForegroundWindow(), ConfigProc, 0 );
    return 0;
}
