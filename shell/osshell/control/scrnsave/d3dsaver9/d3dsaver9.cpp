//-----------------------------------------------------------------------------
// File: d3dsaver9.cpp
//
// Desc: Framework for screen savers that use Direct3D 9.
//
//       Reconstructed from d3dsaver9.obj as linked into Aurora.scr / Bubbles.scr
//       (6.0.6000.16386) and ssText3d.scr / ssBranded.scr (6.0.6001.18000).
//       The code generated for this object is byte-for-byte the same size in
//       all four images; ssText3d.scr additionally keeps the display-settings
//       dialog (WriteScreenSettings, ScreenSettingsDlgProc, SetupAdapterPage),
//       which the linker discarded from the other three.
//
//       Original object:
//           shell\osshell\control\scrnsave\d3dsaver9\objfre\amd64\d3dsaver9.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <commctrl.h>
#include <objbase.h>
#define COMPILE_MULTIMON_STUBS
#include <multimon.h>
#include <strsafe.h>
#include <d3d9.h>
#include "d3dsaver9.h"
#include "d3dsaver9res.h"
#include "dxutil9.h"

#ifndef UNICODE
#error D3DSaver9 is a Unicode-only library
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif

// Resource ID for the tab names / mode list; see d3dsaver9res.h
#define ERRORBOX_WIDTH      500.0f
#define ERRORBOX_HEIGHT     200.0f
#define ERRORBOX_FADEIN       1.0f      // seconds
#define ERRORBOX_HOLD        11.0f      // seconds until the fade-out starts
#define ERRORBOX_FADEOUT      1.5f      // seconds
#define ERRORBOX_PERIOD      12.5f      // seconds until the text jumps somewhere else

static CD3DScreensaver* s_pD3DScreensaver = NULL;




//-----------------------------------------------------------------------------
// Name: CD3DScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
CD3DScreensaver::CD3DScreensaver()
{
    s_pD3DScreensaver = this;

    m_SaverMode             = sm_config;
    m_bAllScreensSame       = FALSE;
    m_hWnd                  = NULL;
    m_hWndParent            = NULL;
    m_hInstance             = NULL;
    m_bWaitForInputIdle     = false;
    m_dwSaverMouseMoveCount = 0;
    m_bWindowed             = false;

    m_bErrorMode            = false;
    m_hrError               = S_OK;
    m_szError[0]            = TEXT('\0');

    m_dwNumMonitors         = 0;
    m_dwNumRenderUnits      = 0;
    m_dwNumAdapters         = 0;
    m_pD3D                  = NULL;
    m_pd3dDevice            = NULL;

    m_bAllowRef             = false;
    m_bUseDepthBuffer       = false;
    m_bMultithreaded        = false;
    m_bOneScreenOnly        = false;
    m_dwMinDepthBits        = 16;
    m_dwMinStencilBits      = 0;
    m_SwapEffectFullscreen  = D3DSWAPEFFECT_DISCARD;
    m_SwapEffectWindowed    = D3DSWAPEFFECT_COPY;

    m_fTime                 = 0.0f;
    m_fElapsedTime          = 0.0f;
    m_fFPS                  = 0.0f;
    m_strDeviceStats[0]     = TEXT('\0');
    m_strFrameStats[0]      = TEXT('\0');

    StringCchCopy( m_strWindowTitle, 200, TEXT("Screen Saver") );
    m_strRegPath[0]         = TEXT('\0');

    SetRectEmpty( &m_rcRenderTotal );
    SetRectEmpty( &m_rcRenderCurDevice );

    ZeroMemory( m_Monitors, sizeof(m_Monitors) );
    ZeroMemory( m_Adapters, sizeof(m_Adapters) );
    ZeroMemory( m_RenderUnits, sizeof(m_RenderUnits) );
    ZeroMemory( m_pReserved, sizeof(m_pReserved) );
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Have the client program call this function before calling Run().
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::Create( HINSTANCE hInstance )
{
    HRESULT hr;

    if( FAILED( hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED ) ) )
        return hr;

    m_hInstance = hInstance;

    // Parse the command line and do the appropriate thing
    m_SaverMode = ParseCommandLine( GetCommandLine() );

    // Read any settings we need.  (Unlike the DirectX SDK framework this
    // happens before the adapters are enumerated.)
    ReadSettings();
    FullModeHack();

    EnumMonitors();

    // Create the screen saver window(s)
    if( m_SaverMode == sm_preview ||
        m_SaverMode == sm_test    ||
        m_SaverMode == sm_full )
    {
        if( FAILED( hr = CreateSaverWindow() ) )
        {
            m_bErrorMode = true;
            m_hrError = hr;
        }
    }

    if( m_SaverMode == sm_preview )
    {
        // In preview mode, "pause" (enter a limited message loop) briefly
        // before proceeding, so the display control panel knows to update itself.
        m_bWaitForInputIdle = true;

        // Post a message to mark the end of the initial group of window messages
        PostMessage( m_hWnd, WM_USER, 0, 0 );

        MSG msg;
        while( m_bWaitForInputIdle )
        {
            // If GetMessage returns FALSE, it's quitting time.
            if( !GetMessage( &msg, m_hWnd, 0, 0 ) )
            {
                // Post the quit message to handle it later
                PostQuitMessage( 0 );
                break;
            }

            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    // Create Direct3D object
    if( ( m_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) == NULL )
    {
        m_hrError = D3DAPPERR_NODIRECT3D;
        m_bErrorMode = true;
        return S_OK;
    }

    // Give the app the opportunity to register a pluggable SW D3D Device, then
    // build a list of Direct3D adapters, modes and devices.  The ConfirmDevice()
    // callback is used to confirm that only devices that meet the app's
    // requirements are considered.
    if( FAILED( hr = RegisterSoftwareDevice() ) ||
        FAILED( hr = BuildDeviceList() ) )
    {
        m_hrError = hr;
        m_bErrorMode = true;
        return S_OK;
    }

    // Make sure that at least one valid usable D3D device was found
    for( DWORD iAdapter = 0; iAdapter < m_dwNumAdapters; iAdapter++ )
    {
        if( m_Adapters[iAdapter]->bHasAppCompatHAL ||
            m_Adapters[iAdapter]->bHasAppCompatSW )
        {
            return S_OK;
        }
    }

    m_hrError = D3DAPPERR_NOCOMPATIBLEDEVICES;
    m_bErrorMode = true;
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: EnumMonitors()
// Desc: Determine HMONITOR, desktop rect, and other info for each monitor.
//       Note that EnumDisplayDevices enumerates monitors in the order
//       indicated on the Settings page of the Display control panel, which
//       is the order we want to list monitors in, as opposed to the order
//       used by D3D's GetAdapterInfo.
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::EnumMonitors( VOID )
{
    DWORD iDevice = 0;
    DISPLAY_DEVICE dispdev;
    DISPLAY_DEVICE dispdev2;
    DEVMODE devmode;
    dispdev.cb = sizeof(dispdev);
    dispdev2.cb = sizeof(dispdev2);
    devmode.dmSize = sizeof(devmode);
    devmode.dmDriverExtra = 0;
    MonitorInfo* pMonitorInfoNew;

    while( EnumDisplayDevices( NULL, iDevice, &dispdev, 0 ) )
    {
        // Ignore NetMeeting's mirrored displays
        if( ( dispdev.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER ) == 0 )
        {
            // To get monitor info for a display device, call EnumDisplayDevices
            // a second time, passing dispdev.DeviceName (from the first call) as
            // the first parameter.
            EnumDisplayDevices( dispdev.DeviceName, 0, &dispdev2, 0 );

            pMonitorInfoNew = &m_Monitors[m_dwNumMonitors];
            ZeroMemory( pMonitorInfoNew, sizeof(MonitorInfo) );
            StringCchCopy( pMonitorInfoNew->strDeviceName, 128, dispdev.DeviceString );
            StringCchCopy( pMonitorInfoNew->strMonitorName, 128, dispdev2.DeviceString );
            pMonitorInfoNew->iAdapter = NO_ADAPTER;

            if( dispdev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP )
            {
                EnumDisplaySettings( dispdev.DeviceName, ENUM_CURRENT_SETTINGS, &devmode );
                if( dispdev.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE )
                {
                    // For some reason devmode.dmPosition is not always (0, 0)
                    // for the primary display, so force it.
                    pMonitorInfoNew->rcScreen.left = 0;
                    pMonitorInfoNew->rcScreen.top = 0;
                }
                else
                {
                    pMonitorInfoNew->rcScreen.left = devmode.dmPosition.x;
                    pMonitorInfoNew->rcScreen.top = devmode.dmPosition.y;
                }
                pMonitorInfoNew->rcScreen.right = pMonitorInfoNew->rcScreen.left + devmode.dmPelsWidth;
                pMonitorInfoNew->rcScreen.bottom = pMonitorInfoNew->rcScreen.top + devmode.dmPelsHeight;
                pMonitorInfoNew->hMonitor = MonitorFromRect( &pMonitorInfoNew->rcScreen, MONITOR_DEFAULTTONULL );
            }
            m_dwNumMonitors++;
            if( m_dwNumMonitors == MAX_DISPLAYS )
                break;
        }
        iDevice++;
    }
}




//-----------------------------------------------------------------------------
// Name: Run()
// Desc: Starts main execution of the screen saver.
//-----------------------------------------------------------------------------
INT CD3DScreensaver::Run()
{
    HRESULT hr;

    // Parse the command line and do the appropriate thing
    switch( m_SaverMode )
    {
        case sm_config:
            if( m_bErrorMode )
                DisplayErrorMsg( m_hrError, 0 );
            else
                DoConfig();
            break;

        case sm_preview:
        case sm_test:
        case sm_full:
            if( FAILED( hr = DoSaver() ) )
                DisplayErrorMsg( hr, 0 );
            break;

        default:
            break;
    }

    ShutdownSaver();

    for( DWORD iAdapter = 0; iAdapter < m_dwNumAdapters; iAdapter++ )
        SAFE_DELETE( m_Adapters[iAdapter] );
    SAFE_RELEASE( m_pD3D );

    CoUninitialize();
    return 0;
}




//-----------------------------------------------------------------------------
// Name: ParseCommandLine()
// Desc: Interpret command-line parameters passed to this app.
//-----------------------------------------------------------------------------
SaverMode CD3DScreensaver::ParseCommandLine( TCHAR* pstrCommandLine )
{
    m_hWndParent = NULL;

    // Skip the first part of the command line, which is the full path
    // to the exe.  If it contains spaces, it will be contained in quotes.
    if( *pstrCommandLine == TEXT('\"') )
    {
        pstrCommandLine++;
        while( *pstrCommandLine != TEXT('\0') && *pstrCommandLine != TEXT('\"') )
            pstrCommandLine++;
        if( *pstrCommandLine == TEXT('\"') )
            pstrCommandLine++;
    }
    else
    {
        while( *pstrCommandLine != TEXT('\0') && *pstrCommandLine != TEXT(' ') )
            pstrCommandLine++;
        if( *pstrCommandLine == TEXT(' ') )
            pstrCommandLine++;
    }

    // Skip along to the first option delimiter "/" or "-"
    while( *pstrCommandLine != TEXT('\0') && *pstrCommandLine != TEXT('/') && *pstrCommandLine != TEXT('-') )
        pstrCommandLine++;

    // If there wasn't one, then must be config mode
    if( *pstrCommandLine == TEXT('\0') )
        return sm_config;

    // Otherwise see what the option was
    switch( *(++pstrCommandLine) )
    {
        case 'c':
        case 'C':
            pstrCommandLine++;
            while( *pstrCommandLine && !iswdigit( *pstrCommandLine ) )
                pstrCommandLine++;
            if( iswdigit( *pstrCommandLine ) )
            {
                CHAR strCommandLine[2048];
                WideCharToMultiByte( CP_ACP, 0, pstrCommandLine, -1, strCommandLine,
                                     sizeof(strCommandLine) - 1, NULL, NULL );
                strCommandLine[sizeof(strCommandLine) - 1] = 0;
                m_hWndParent = (HWND)(_atoi64( strCommandLine ));
            }
            else
            {
                m_hWndParent = NULL;
            }
            return sm_config;

        case 't':
        case 'T':
            return sm_test;

        case 'p':
        case 'P':
            // Preview-mode, so option is followed by the parent HWND in decimal
            pstrCommandLine++;
            while( *pstrCommandLine && !iswdigit( *pstrCommandLine ) )
                pstrCommandLine++;
            if( iswdigit( *pstrCommandLine ) )
            {
                CHAR strCommandLine[2048];
                WideCharToMultiByte( CP_ACP, 0, pstrCommandLine, -1, strCommandLine,
                                     sizeof(strCommandLine) - 1, NULL, NULL );
                strCommandLine[sizeof(strCommandLine) - 1] = 0;
                m_hWndParent = (HWND)(_atoi64( strCommandLine ));
            }
            return sm_preview;

        default:
            // All other options => run the screensaver (typically this is "/s")
            return sm_full;
    }
}




//-----------------------------------------------------------------------------
// Name: CreateSaverWindow
// Desc: Register and create the appropriate window(s)
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::CreateSaverWindow()
{
    // Register an appropriate window class
    WNDCLASS    cls;
    cls.hCursor        = LoadCursor( NULL, IDC_ARROW );
    cls.hIcon          = LoadIcon( m_hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON) );
    cls.lpszMenuName   = NULL;
    cls.lpszClassName  = TEXT("D3DSaverWndClass");
    cls.hbrBackground  = (HBRUSH) GetStockObject( BLACK_BRUSH );
    cls.hInstance      = m_hInstance;
    cls.style          = CS_VREDRAW | CS_HREDRAW;
    cls.lpfnWndProc    = SaverProcStub;
    cls.cbWndExtra     = 0;
    cls.cbClsExtra     = 0;
    RegisterClass( &cls );

    // Create the window
    RECT rc;
    DWORD dwStyle;
    switch( m_SaverMode )
    {
        case sm_preview:
            GetClientRect( m_hWndParent, &rc );
            dwStyle = WS_VISIBLE | WS_CHILD;
            AdjustWindowRect( &rc, dwStyle, FALSE );
            m_hWnd = CreateWindow( TEXT("D3DSaverWndClass"), m_strWindowTitle, dwStyle,
                                   rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                                   m_hWndParent, NULL, m_hInstance, this );
            m_Monitors[0].hWnd = m_hWnd;
            GetClientRect( m_hWnd, &m_rcRenderTotal );
            GetClientRect( m_hWnd, &m_rcRenderCurDevice );
            break;

        case sm_test:
            rc.left = rc.top = 50;
            rc.right = rc.left + 600;
            rc.bottom = rc.top + 400;
            dwStyle = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
            AdjustWindowRect( &rc, dwStyle, FALSE );
            m_hWnd = CreateWindow( TEXT("D3DSaverWndClass"), m_strWindowTitle, dwStyle,
                                   rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                                   NULL, NULL, m_hInstance, this );
            m_Monitors[0].hWnd = m_hWnd;
            GetClientRect( m_hWnd, &m_rcRenderTotal );
            GetClientRect( m_hWnd, &m_rcRenderCurDevice );
            break;

        case sm_full:
            // Create windows for each monitor.  Note that m_hWnd is NULL when CreateWindowEx
            // is called for the first monitor, so that window has no parent.  Windows for
            // additional monitors are created as children of the window for the first monitor.
            dwStyle = WS_VISIBLE | WS_POPUP;
            m_hWnd = NULL;
            for( DWORD iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
            {
                MonitorInfo* pMonitorInfo;
                pMonitorInfo = &m_Monitors[iMonitor];
                if( pMonitorInfo->hMonitor == NULL )
                    continue;
                rc = pMonitorInfo->rcScreen;
                pMonitorInfo->hWnd = CreateWindowEx( WS_EX_TOPMOST | WS_EX_TOOLWINDOW, TEXT("D3DSaverWndClass"),
                    m_strWindowTitle, dwStyle, rc.left, rc.top, rc.right - rc.left,
                    rc.bottom - rc.top, m_hWnd, NULL, m_hInstance, this );
                if( pMonitorInfo->hWnd == NULL )
                    return E_FAIL;
                if( m_hWnd == NULL )
                    m_hWnd = pMonitorInfo->hWnd;
            }
            break;

        default:
            break;
    }

    if( m_hWnd == NULL )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DoSaver()
// Desc: Run the screensaver graphics - may be preview, test or full-on mode
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::DoSaver()
{
    HRESULT hr;

    // Initialize the application timer
    DXUtil_Timer( TIMER_START );

    if( !m_bErrorMode )
    {
        // Do graphical init stuff
        if( FAILED( hr = Initialize3DEnvironment() ) )
        {
            m_bErrorMode = true;
            m_hrError = hr;
        }

        // Initialize the app's custom scene stuff
        if( !m_bErrorMode )
        {
            if( FAILED( hr = OneTimeSceneInit() ) )
                return DisplayErrorMsg( hr, MSGERR_APPMUSTEXIT );
        }
    }

    // Flag as screensaver running if in full on mode
    if( m_SaverMode == sm_full )
    {
        BOOL bUnused;
        SystemParametersInfo( SPI_SCREENSAVERRUNNING, TRUE, &bUnused, 0 );
    }

    // Message pump
    BOOL bGotMsg;
    MSG msg;
    msg.message = WM_NULL;
    while( msg.message != WM_QUIT )
    {
        bGotMsg = PeekMessage( &msg, NULL, 0, 0, PM_REMOVE );
        if( bGotMsg )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            Sleep( 10 );
            if( m_bErrorMode )
            {
                Sleep( 6 );
                UpdateErrorBox();
            }
            else
            {
                Render3DEnvironment();
            }
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ShutdownSaver()
// Desc:
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::ShutdownSaver()
{
    // Unflag screensaver running if in full on mode
    if( m_SaverMode == sm_full )
    {
        BOOL bUnused;
        SystemParametersInfo( SPI_SCREENSAVERRUNNING, FALSE, &bUnused, 0 );
    }

    // Kill graphical stuff
    Cleanup3DEnvironment();

    // Let client app clean up its resources
    FinalCleanup();

    // Post message to drop out of message loop
    PostQuitMessage( 0 );
}




//-----------------------------------------------------------------------------
// Name: InterruptSaver()
// Desc: A message was received (mouse move, keydown, etc.) that may mean
//       the screen saver should shut down.
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::InterruptSaver()
{
    if( m_SaverMode == sm_test || m_SaverMode == sm_full )
        ShutdownSaver();
}




//-----------------------------------------------------------------------------
// Name: SaverProcStub()
// Desc: This function forwards all window messages to SaverProc, which has
//       access to the "this" pointer.
//-----------------------------------------------------------------------------
LRESULT CALLBACK CD3DScreensaver::SaverProcStub( HWND hWnd, UINT uMsg,
                                                 WPARAM wParam, LPARAM lParam )
{
    return s_pD3DScreensaver->SaverProc( hWnd, uMsg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: SaverProc()
// Desc: Handle window messages for main screensaver windows (one per screen).
//-----------------------------------------------------------------------------
LRESULT CD3DScreensaver::SaverProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
        case WM_USER:
            // All initialization messages have gone through.  Allow
            // 500ms of idle time, then proceed with initialization.
            SetTimer( hWnd, 1, 500, NULL );
            break;

        case WM_TIMER:
            // Initial idle time is done, proceed with initialization.
            m_bWaitForInputIdle = false;
            KillTimer( hWnd, 1 );
            break;

        case WM_DESTROY:
            if( m_SaverMode == sm_preview || m_SaverMode == sm_test )
                ShutdownSaver();
            break;

        case WM_SETCURSOR:
            if( m_SaverMode == sm_full )
            {
                // Hide cursor
                SetCursor( NULL );
                return TRUE;
            }
            break;

        case WM_PAINT:
        {
            // Show error message, if there is one
            PAINTSTRUCT ps;
            BeginPaint( hWnd, &ps );

            // In preview mode, just fill
            // the preview window with black.
            if( !m_bErrorMode && m_SaverMode == sm_preview )
            {
                RECT rc;
                GetClientRect( hWnd, &rc );
                HBRUSH hbrBlack = (HBRUSH)GetStockObject( BLACK_BRUSH );
                if( hbrBlack != NULL )
                    FillRect( ps.hdc, &rc, hbrBlack );
            }
            else
            {
                DoPaint( hWnd, ps.hdc );
            }

            EndPaint( hWnd, &ps );
            return 0;
        }

        case WM_ERASEBKGND:
            // Erase background only if the window is not assigned to a render unit
            {
                RenderUnit* pRenderUnit;
                D3DAdapterInfo* pD3DAdapterInfo;
                for( DWORD iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
                {
                    pRenderUnit = &m_RenderUnits[iRenderUnit];
                    pD3DAdapterInfo = m_Adapters[pRenderUnit->iAdapter];
                    if( pD3DAdapterInfo->hWndDevice == hWnd )
                        return TRUE; // don't erase this window
                }
            }
            break;

        case WM_MOUSEMOVE:
            if( m_SaverMode != sm_test )
            {
                static INT xPrev = -1;
                static INT yPrev = -1;
                INT xCur = GET_X_LPARAM( lParam );
                INT yCur = GET_Y_LPARAM( lParam );
                if( xCur != xPrev || yCur != yPrev )
                {
                    xPrev = xCur;
                    yPrev = yCur;
                    m_dwSaverMouseMoveCount++;
                    if( m_dwSaverMouseMoveCount > 5 )
                        InterruptSaver();
                }
            }
            break;

        case WM_KEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if( m_SaverMode != sm_test )
                InterruptSaver();
            break;

        case WM_ACTIVATEAPP:
            if( wParam == FALSE && m_SaverMode != sm_test )
                InterruptSaver();
            break;

        case WM_CLOSE:
            InterruptSaver();
            break;

        case WM_POWERBROADCAST:
            if( wParam == PBT_APMQUERYSUSPEND )
                return TRUE;
            if( wParam == PBT_APMSUSPEND )
                InterruptSaver();
            break;

        case WM_SYSCOMMAND:
            if( m_SaverMode == sm_full )
            {
                switch( wParam )
                {
                    case SC_NEXTWINDOW:
                    case SC_PREVWINDOW:
                    case SC_SCREENSAVE:
                        return FALSE;

                    case SC_CLOSE:
                    case SC_MONITORPOWER:
                        ShutdownSaver();
                        break;
                }
            }
            break;
    }

    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: Initialize3DEnvironment()
// Desc: Set up D3D device(s)
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::Initialize3DEnvironment()
{
    HRESULT hr;
    DWORD iAdapter;
    DWORD iMonitor;
    DWORD iRenderUnit;
    D3DAdapterInfo* pD3DAdapterInfo;
    MonitorInfo* pMonitorInfo;
    RenderUnit* pRenderUnit;
    MONITORINFO monitorInfo;

    if( m_SaverMode == sm_full )
    {
        // Fullscreen mode.  Create a RenderUnit for each monitor (unless
        // the user wants it black)
        m_bWindowed = false;

        if( m_bOneScreenOnly )
        {
            // Set things up to only create a RenderUnit on the best device
            for( iAdapter = 0; iAdapter < m_dwNumAdapters; iAdapter++ )
            {
                pD3DAdapterInfo = m_Adapters[iAdapter];
                pD3DAdapterInfo->bLeaveBlack = TRUE;
            }
            GetBestAdapter( &iAdapter );
            if( iAdapter == NO_ADAPTER )
            {
                m_bErrorMode = true;
                m_hrError = D3DAPPERR_NOCOMPATIBLEDEVICES;
            }
            else
            {
                pD3DAdapterInfo = m_Adapters[iAdapter];
                pD3DAdapterInfo->bLeaveBlack = FALSE;
            }
        }

        for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
        {
            pMonitorInfo = &m_Monitors[iMonitor];
            iAdapter = pMonitorInfo->iAdapter;
            if( iAdapter == NO_ADAPTER )
                continue;
            pD3DAdapterInfo = m_Adapters[iAdapter];
            if( !pD3DAdapterInfo->bLeaveBlack && pD3DAdapterInfo->dwNumDevices > 0 )
            {
                pD3DAdapterInfo->hWndDevice = pMonitorInfo->hWnd;
                pRenderUnit = &m_RenderUnits[m_dwNumRenderUnits++];
                ZeroMemory( pRenderUnit, sizeof(RenderUnit) );
                pRenderUnit->iAdapter = iAdapter;

                // No mode switch: the "fullscreen" saver is a windowed device
                // on a topmost popup that covers the monitor.
                if( FAILED( hr = CreateWindowedRenderUnit( pRenderUnit ) ) )
                {
                    // skip this render unit and leave screen blank
                    m_dwNumRenderUnits--;
                    m_bErrorMode = true;
                    m_hrError = D3DAPPERR_CREATEDEVICEFAILED;
                }
            }
        }
    }
    else
    {
        // Windowed mode, for test mode or preview window.  Just need one RenderUnit.
        m_bWindowed = true;

        GetClientRect( m_hWnd, &m_rcRenderTotal );
        GetClientRect( m_hWnd, &m_rcRenderCurDevice );

        GetBestAdapter( &iAdapter );
        if( iAdapter == NO_ADAPTER )
        {
            m_bErrorMode = true;
            m_hrError = D3DAPPERR_CREATEDEVICEFAILED;
        }
        else
        {
            pD3DAdapterInfo = m_Adapters[iAdapter];
            pD3DAdapterInfo->hWndDevice = m_hWnd;
        }
        if( !m_bErrorMode )
        {
            pRenderUnit = &m_RenderUnits[m_dwNumRenderUnits++];
            ZeroMemory( pRenderUnit, sizeof(RenderUnit) );
            pRenderUnit->iAdapter = iAdapter;
            if( FAILED( hr = CreateWindowedRenderUnit( pRenderUnit ) ) )
            {
                m_dwNumRenderUnits--;
                m_bErrorMode = true;
                if( m_SaverMode == sm_preview )
                    m_hrError = D3DAPPERR_NOPREVIEW;
                else
                    m_hrError = D3DAPPERR_CREATEDEVICEFAILED;
            }
        }
    }

    // (Re-)determine coordinates of all screens, and make sure windows
    // still cover each screen
    for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
    {
        pMonitorInfo = &m_Monitors[iMonitor];
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo( pMonitorInfo->hMonitor, &monitorInfo );
        pMonitorInfo->rcScreen = monitorInfo.rcMonitor;
        if( m_SaverMode == sm_full )
        {
            SetWindowPos( pMonitorInfo->hWnd, HWND_TOPMOST, monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_NOACTIVATE );
        }
    }

    // For fullscreen, determine bounds of the virtual screen containing all
    // screens that are rendering.  Don't just use SM_XVIRTUALSCREEN, because
    // we don't want to count screens that are just black
    if( m_SaverMode == sm_full )
    {
        for( iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
        {
            pRenderUnit = &m_RenderUnits[iRenderUnit];
            pMonitorInfo = &m_Monitors[pRenderUnit->iMonitor];
            UnionRect( &m_rcRenderTotal, &m_rcRenderTotal, &pMonitorInfo->rcScreen );
        }
    }

    if( !m_bErrorMode )
    {
        // Initialize D3D devices for all render units
        for( iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
        {
            pRenderUnit = &m_RenderUnits[iRenderUnit];
            SwitchToRenderUnit( iRenderUnit );
            if( FAILED( hr = InitDeviceObjects() ) )
            {
                m_bErrorMode = true;
                m_hrError = D3DAPPERR_INITDEVICEOBJECTSFAILED;
                return D3DAPPERR_INITDEVICEOBJECTSFAILED;
            }
            pRenderUnit->bDeviceObjectsInited = true;
            if( FAILED( hr = RestoreDeviceObjects() ) )
            {
                m_bErrorMode = true;
                m_hrError = D3DAPPERR_INITDEVICEOBJECTSFAILED;
                return D3DAPPERR_INITDEVICEOBJECTSFAILED;
            }
            pRenderUnit->bDeviceObjectsRestored = true;
        }
        UpdateDeviceStats();
    }

    // Make sure all those display changes don't count as user mouse moves
    m_dwSaverMouseMoveCount = 0;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetBestAdapter()
// Desc: To decide which adapter to use, loop through monitors until you find
//       one whose adapter has a compatible HAL.  If none, use the first
//       monitor that has an compatible SW device.
//-----------------------------------------------------------------------------
BOOL CD3DScreensaver::GetBestAdapter( DWORD* piAdapter )
{
    DWORD iAdapterBest = NO_ADAPTER;
    DWORD iAdapter;
    DWORD iMonitor;
    MonitorInfo* pMonitorInfo;
    D3DAdapterInfo* pD3DAdapterInfo;

    for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
    {
        pMonitorInfo = &m_Monitors[iMonitor];
        iAdapter = pMonitorInfo->iAdapter;
        if( iAdapter == NO_ADAPTER )
            continue;
        pD3DAdapterInfo = m_Adapters[iAdapter];
        if( pD3DAdapterInfo->bHasAppCompatHAL )
        {
            iAdapterBest = iAdapter;
            if( iAdapter == 0 )
                break;          // the primary adapter with a HAL: stop looking
        }
        else if( pD3DAdapterInfo->bHasAppCompatSW )
        {
            if( iAdapterBest == NO_ADAPTER )
                iAdapterBest = iAdapter;
        }
    }
    *piAdapter = iAdapterBest;

    return ( iAdapterBest != NO_ADAPTER );
}




//-----------------------------------------------------------------------------
// Name: CreateWindowedRenderUnit()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::CreateWindowedRenderUnit( RenderUnit* pRenderUnit )
{
    HRESULT hr;
    UINT iAdapter = pRenderUnit->iAdapter;
    D3DAdapterInfo* pD3DAdapterInfo = m_Adapters[iAdapter];
    DWORD iMonitor = pD3DAdapterInfo->iMonitor;
    D3DDeviceInfo* pD3DDeviceInfo;
    D3DWindowedModeInfo D3DWindowedModeInfo;

    // Find the best device for the adapter.  Use the HAL if it's there and
    // nobody said to disable it; otherwise SW, and REF as the last resort.
    D3DDEVTYPE curType = D3DDEVTYPE_FORCE_DWORD;
    pD3DAdapterInfo->dwCurrentDevice = 0xffff;
    for( DWORD iDevice = 0; iDevice < pD3DAdapterInfo->dwNumDevices; iDevice++ )
    {
        pD3DDeviceInfo = &pD3DAdapterInfo->devices[iDevice];
        if( pD3DDeviceInfo->DeviceType == D3DDEVTYPE_HAL && !pD3DAdapterInfo->bDisableHW &&
            pD3DDeviceInfo->bCanDoWindowed )
        {
            pD3DAdapterInfo->dwCurrentDevice = iDevice;
            break; // stop looking
        }
        else if( pD3DDeviceInfo->DeviceType == D3DDEVTYPE_SW && pD3DDeviceInfo->bCanDoWindowed )
        {
            pD3DAdapterInfo->dwCurrentDevice = iDevice;
            curType = D3DDEVTYPE_SW;
            // but keep looking
        }
        else if( pD3DDeviceInfo->DeviceType == D3DDEVTYPE_REF && m_bAllowRef && curType != D3DDEVTYPE_SW )
        {
            pD3DAdapterInfo->dwCurrentDevice = iDevice;
            curType = D3DDEVTYPE_REF;
            // but keep looking
        }
    }
    if( pD3DAdapterInfo->dwCurrentDevice == 0xffff )
        return D3DAPPERR_NOHARDWAREDEVICE;
    pD3DDeviceInfo = &pD3DAdapterInfo->devices[pD3DAdapterInfo->dwCurrentDevice];

    D3DWindowedModeInfo.DisplayFormat = pD3DAdapterInfo->d3ddmDesktop.Format;
    D3DWindowedModeInfo.BackBufferFormat = pD3DAdapterInfo->d3ddmDesktop.Format;
    if( FAILED( CheckWindowedFormat( iAdapter, &D3DWindowedModeInfo ) ) )
    {
        D3DWindowedModeInfo.BackBufferFormat = D3DFMT_A8R8G8B8;
        if( FAILED( CheckWindowedFormat( iAdapter, &D3DWindowedModeInfo ) ) )
        {
            D3DWindowedModeInfo.BackBufferFormat = D3DFMT_X8R8G8B8;
            if( FAILED( CheckWindowedFormat( iAdapter, &D3DWindowedModeInfo ) ) )
            {
                D3DWindowedModeInfo.BackBufferFormat = D3DFMT_A1R5G5B5;
                if( FAILED( CheckWindowedFormat( iAdapter, &D3DWindowedModeInfo ) ) )
                {
                    D3DWindowedModeInfo.BackBufferFormat = D3DFMT_R5G6B5;
                    if( FAILED( CheckWindowedFormat( iAdapter, &D3DWindowedModeInfo ) ) )
                    {
                        return E_FAIL;
                    }
                }
            }
        }
    }

    pRenderUnit->iMonitor = iMonitor;
    pRenderUnit->DeviceType = pD3DDeviceInfo->DeviceType;
    pRenderUnit->dwBehavior = D3DWindowedModeInfo.dwBehavior;
    pRenderUnit->d3dpp.BackBufferWidth = 0;
    pRenderUnit->d3dpp.BackBufferHeight = 0;
    pRenderUnit->d3dpp.Windowed = TRUE;
    pRenderUnit->d3dpp.FullScreen_RefreshRateInHz = 0;
    pRenderUnit->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    pRenderUnit->d3dpp.AutoDepthStencilFormat = D3DWindowedModeInfo.DepthStencilFormat;
    pRenderUnit->d3dpp.BackBufferCount = 1;
    pRenderUnit->d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pRenderUnit->d3dpp.BackBufferFormat = D3DWindowedModeInfo.BackBufferFormat;
    pRenderUnit->d3dpp.SwapEffect = m_SwapEffectWindowed;
    pRenderUnit->d3dpp.hDeviceWindow = pD3DAdapterInfo->hWndDevice;
    pRenderUnit->d3dpp.EnableAutoDepthStencil = m_bUseDepthBuffer;
    pRenderUnit->d3dpp.Flags = 0;

    // Create device
    hr = m_pD3D->CreateDevice( iAdapter, pRenderUnit->DeviceType, m_hWnd,
                               pRenderUnit->dwBehavior, &pRenderUnit->d3dpp, &pRenderUnit->pd3dDevice );
    if( FAILED( hr ) )
    {
        return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateDeviceStats()
// Desc: Store device description
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::UpdateDeviceStats()
{
    DWORD iRenderUnit;
    RenderUnit* pRenderUnit;
    for( iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
    {
        pRenderUnit = &m_RenderUnits[iRenderUnit];
        if( pRenderUnit->DeviceType == D3DDEVTYPE_REF )
            StringCchCopy( pRenderUnit->strDeviceStats, 90, TEXT("REF") );
        else if( pRenderUnit->DeviceType == D3DDEVTYPE_HAL )
            StringCchCopy( pRenderUnit->strDeviceStats, 90, TEXT("HAL") );
        else if( pRenderUnit->DeviceType == D3DDEVTYPE_SW )
            StringCchCopy( pRenderUnit->strDeviceStats, 90, TEXT("SW") );

        if( pRenderUnit->dwBehavior & D3DCREATE_HARDWARE_VERTEXPROCESSING &&
            pRenderUnit->dwBehavior & D3DCREATE_PUREDEVICE )
        {
            if( pRenderUnit->DeviceType == D3DDEVTYPE_HAL )
                StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (pure hw vp)") );
            else
                StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (simulated pure hw vp)") );
        }
        else if( pRenderUnit->dwBehavior & D3DCREATE_HARDWARE_VERTEXPROCESSING )
        {
            if( pRenderUnit->DeviceType == D3DDEVTYPE_HAL )
                StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (hw vp)") );
            else
                StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (simulated hw vp)") );
        }
        else if( pRenderUnit->dwBehavior & D3DCREATE_MIXED_VERTEXPROCESSING )
        {
            if( pRenderUnit->DeviceType == D3DDEVTYPE_HAL )
                StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (mixed vp)") );
            else
                StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (simulated mixed vp)") );
        }
        else if( pRenderUnit->dwBehavior & D3DCREATE_SOFTWARE_VERTEXPROCESSING )
        {
            StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(" (sw vp)") );
        }

        if( pRenderUnit->DeviceType == D3DDEVTYPE_HAL )
        {
            // Be sure not to overflow strDeviceStats when appending the adapter
            // description, since it can be long.  Note that the adapter description
            // is initially CHAR and must be converted to WCHAR.
            StringCchCat( pRenderUnit->strDeviceStats, 90, TEXT(": ") );
            TCHAR szDescription[300];
            MultiByteToWideChar( CP_ACP, 0,
                m_Adapters[pRenderUnit->iAdapter]->d3dAdapterIdentifier.Description, -1,
                szDescription, 299 );
            szDescription[299] = 0;
            StringCchCat( pRenderUnit->strDeviceStats, 90, szDescription );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: SwitchToRenderUnit()
// Desc: Updates internal variables and notifies client that the render unit
//       / D3D device has changed.
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::SwitchToRenderUnit( UINT iRenderUnit )
{
    RenderUnit* pRenderUnit = &m_RenderUnits[iRenderUnit];
    MonitorInfo* pMonitorInfo = &m_Monitors[pRenderUnit->iMonitor];

    m_pd3dDevice = pRenderUnit->pd3dDevice;
    if( !m_bWindowed )
        m_rcRenderCurDevice = pMonitorInfo->rcScreen;

    if( m_pd3dDevice != NULL && !m_bErrorMode )
    {
        // Store render target surface desc
        LPDIRECT3DSURFACE9 pBackBuffer;
        m_pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
        pBackBuffer->GetDesc( &m_d3dsdBackBuffer );
        pBackBuffer->Release();
    }

    StringCchCopy( m_strDeviceStats, 90, pRenderUnit->strDeviceStats );
    StringCchCopy( m_strFrameStats, 40, pRenderUnit->strFrameStats );

    // Notify the client that the device has changed
    SetDevice( iRenderUnit );
}




//-----------------------------------------------------------------------------
// Name: SortModesCallback()
// Desc: Callback function for sorting display modes (used by BuildDeviceList).
//-----------------------------------------------------------------------------
static int __cdecl SortModesCallback( const VOID* arg1, const VOID* arg2 )
{
    D3DDISPLAYMODE* p1 = (D3DDISPLAYMODE*)arg1;
    D3DDISPLAYMODE* p2 = (D3DDISPLAYMODE*)arg2;

    if( p1->Width  < p2->Width )    return -1;
    if( p1->Width  > p2->Width )    return +1;
    if( p1->Height < p2->Height )   return -1;
    if( p1->Height > p2->Height )   return +1;
    if( p1->Format > p2->Format )   return -1;
    if( p1->Format < p2->Format )   return +1;

    return 0;
}




//-----------------------------------------------------------------------------
// Name: BuildDeviceList()
// Desc: Builds a list of all available adapters, devices, and modes.
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::BuildDeviceList()
{
    DWORD dwNumDeviceTypes;
    const TCHAR* strDeviceDescs[] = { TEXT("HAL"), TEXT("SW"), TEXT("REF") };
    const D3DDEVTYPE DeviceTypes[] = { D3DDEVTYPE_HAL, D3DDEVTYPE_SW, D3DDEVTYPE_REF };
    if( m_bAllowRef )
        dwNumDeviceTypes = 3;
    else
        dwNumDeviceTypes = 2;

    // Loop through all the adapters on the system (usually, there's just one
    // unless more than one graphics card is present).
    for( UINT iAdapter = 0; iAdapter < m_pD3D->GetAdapterCount(); iAdapter++ )
    {
        // Fill in adapter info
        if( m_Adapters[m_dwNumAdapters] == NULL )
        {
            m_Adapters[m_dwNumAdapters] = new D3DAdapterInfo;
            if( m_Adapters[m_dwNumAdapters] == NULL )
                return E_OUTOFMEMORY;
            ZeroMemory( m_Adapters[m_dwNumAdapters], sizeof(D3DAdapterInfo) );
        }

        D3DAdapterInfo* pAdapter = m_Adapters[m_dwNumAdapters];
        m_pD3D->GetAdapterIdentifier( iAdapter, 0, &pAdapter->d3dAdapterIdentifier );
        m_pD3D->GetAdapterDisplayMode( iAdapter, &pAdapter->d3ddmDesktop );
        pAdapter->iMonitor        = NO_MONITOR;
        pAdapter->dwNumDevices    = 0;
        pAdapter->dwCurrentDevice = 0;
        pAdapter->bLeaveBlack     = FALSE;

        // Find the MonitorInfo that corresponds to this adapter.  If the monitor
        // is disabled, the adapter has a NULL HMONITOR and we cannot find a
        // MonitorInfo.  (And it doesn't make sense to save screens on that adapter.)
        HMONITOR hMonitor = m_pD3D->GetAdapterMonitor( iAdapter );
        if( hMonitor != NULL )
        {
            for( DWORD iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
            {
                MonitorInfo* pMonitorInfo;
                pMonitorInfo = &m_Monitors[iMonitor];
                if( pMonitorInfo->hMonitor == hMonitor )
                {
                    pAdapter->iMonitor = iMonitor;
                    pMonitorInfo->iAdapter = iAdapter;
                    break;
                }
            }
        }

        // Enumerate all display modes on this adapter
        D3DDISPLAYMODE modes[100];
        D3DFORMAT      formats[20];
        DWORD dwNumFormats      = 0;
        DWORD dwNumModes        = 0;
        DWORD dwNumAdapterModes = m_pD3D->GetAdapterModeCount( iAdapter, pAdapter->d3ddmDesktop.Format );
        DWORD m, f;

        // Add the adapter's current desktop format to the list of formats
        formats[dwNumFormats++] = pAdapter->d3ddmDesktop.Format;

        for( UINT iMode = 0; iMode < dwNumAdapterModes; iMode++ )
        {
            // Get the display mode attributes
            D3DDISPLAYMODE DisplayMode;
            m_pD3D->EnumAdapterModes( iAdapter, pAdapter->d3ddmDesktop.Format, iMode, &DisplayMode );

            // Filter out low-resolution modes
            if( DisplayMode.Width  < 640 || DisplayMode.Height < 400 )
                continue;

            // Check if the mode already exists (to filter out refresh rates)
            for( m = 0L; m < dwNumModes; m++ )
            {
                if( ( modes[m].Width  == DisplayMode.Width  ) &&
                    ( modes[m].Height == DisplayMode.Height ) &&
                    ( modes[m].Format == DisplayMode.Format ) )
                    break;
            }

            // If we found a new mode, add it to the list of modes
            if( m == dwNumModes )
            {
                modes[dwNumModes].Width       = DisplayMode.Width;
                modes[dwNumModes].Height      = DisplayMode.Height;
                modes[dwNumModes].Format      = DisplayMode.Format;
                modes[dwNumModes].RefreshRate = 0;
                if( dwNumModes < 99 )
                    dwNumModes++;

                // Check if the mode's format already exists
                for( f = 0; f < dwNumFormats; f++ )
                {
                    if( dwNumFormats >= 20 || DisplayMode.Format == formats[f] )
                        break;
                }

                // If the format is new, add it to the list
                if( f == dwNumFormats && dwNumFormats < 20 )
                    formats[dwNumFormats++] = DisplayMode.Format;
            }
        }

        if( dwNumModes > 100 )
            dwNumModes = 100;

        // Sort the list of display modes (by format, then width, then height)
        qsort( modes, dwNumModes, sizeof(D3DDISPLAYMODE), SortModesCallback );

        // Add devices to adapter
        for( UINT iDevice = 0; iDevice < dwNumDeviceTypes; iDevice++ )
        {
            // Fill in device info
            D3DDeviceInfo* pDevice;
            pDevice                  = &pAdapter->devices[pAdapter->dwNumDevices];
            pDevice->DeviceType      = DeviceTypes[iDevice];
            m_pD3D->GetDeviceCaps( iAdapter, DeviceTypes[iDevice], &pDevice->d3dCaps );
            pDevice->strDesc         = strDeviceDescs[iDevice];
            pDevice->dwNumModes      = 0;
            pDevice->dwCurrentMode   = 0;
            pDevice->bCanDoWindowed  = false;
            pDevice->bWindowed       = false;
            pDevice->MultiSampleType = D3DMULTISAMPLE_NONE;

            // Examine each format supported by the adapter to see if it will
            // work with this device and meets the needs of the application.
            bool      bFormatConfirmed[20];
            DWORD     dwBehavior[20];
            D3DFORMAT fmtDepthStencil[20];

            for( f = 0; f < dwNumFormats; f++ )
            {
                bFormatConfirmed[f] = false;
                fmtDepthStencil[f] = D3DFMT_UNKNOWN;

                // Skip formats that cannot be used as render targets on this device
                if( FAILED( m_pD3D->CheckDeviceType( iAdapter, pDevice->DeviceType,
                                                     formats[f], formats[f], FALSE ) ) )
                    continue;

                if( pDevice->DeviceType == D3DDEVTYPE_SW )
                {
                    // This system has a SW device
                    pAdapter->bHasSW = true;
                }

                if( pDevice->DeviceType == D3DDEVTYPE_HAL )
                {
                    // This system has a HAL device
                    pAdapter->bHasHAL = true;
                }

                // Confirm the device/format for HW vertex processing
                if( pDevice->d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT )
                {
                    if( pDevice->d3dCaps.DevCaps & D3DDEVCAPS_PUREDEVICE )
                    {
                        dwBehavior[f] = D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                        D3DCREATE_PUREDEVICE;

                        if( SUCCEEDED( ConfirmDevice( &pDevice->d3dCaps, dwBehavior[f],
                                                      formats[f] ) ) )
                            bFormatConfirmed[f] = true;
                    }

                    if( !bFormatConfirmed[f] )
                    {
                        dwBehavior[f] = D3DCREATE_HARDWARE_VERTEXPROCESSING;

                        if( SUCCEEDED( ConfirmDevice( &pDevice->d3dCaps, dwBehavior[f],
                                                      formats[f] ) ) )
                            bFormatConfirmed[f] = true;
                    }

                    if( !bFormatConfirmed[f] )
                    {
                        dwBehavior[f] = D3DCREATE_MIXED_VERTEXPROCESSING;

                        if( SUCCEEDED( ConfirmDevice( &pDevice->d3dCaps, dwBehavior[f],
                                                      formats[f] ) ) )
                            bFormatConfirmed[f] = true;
                    }
                }

                // Confirm the device/format for SW vertex processing
                if( !bFormatConfirmed[f] )
                {
                    dwBehavior[f] = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

                    if( SUCCEEDED( ConfirmDevice( &pDevice->d3dCaps, dwBehavior[f],
                                                  formats[f] ) ) )
                        bFormatConfirmed[f] = true;
                }

                if( !bFormatConfirmed[f] )
                    continue;

                if( m_bMultithreaded )
                {
                    dwBehavior[f] |= D3DCREATE_MULTITHREADED;
                }

                // Find a suitable depth/stencil buffer format for this device/format
                if( m_bUseDepthBuffer )
                {
                    if( !FindDepthStencilFormat( iAdapter, pDevice->DeviceType,
                        formats[f], &fmtDepthStencil[f] ) )
                    {
                        bFormatConfirmed[f] = false;
                    }
                }
            }

            // Add all enumerated display modes with confirmed formats to the
            // device's list of valid modes
            for( m = 0L; m < dwNumModes; m++ )
            {
                for( f = 0; f < dwNumFormats; f++ )
                {
                    if( modes[m].Format == formats[f] )
                    {
                        if( bFormatConfirmed[f] == true )
                        {
                            // Add this mode to the device's list of valid modes
                            pDevice->modes[pDevice->dwNumModes].Width      = modes[m].Width;
                            pDevice->modes[pDevice->dwNumModes].Height     = modes[m].Height;
                            pDevice->modes[pDevice->dwNumModes].Format     = modes[m].Format;
                            pDevice->modes[pDevice->dwNumModes].dwBehavior = dwBehavior[f];
                            pDevice->modes[pDevice->dwNumModes].DepthStencilFormat = fmtDepthStencil[f];
                            pDevice->dwNumModes++;
                        }
                    }
                }
            }

            // Select any 640x480 mode for default (but prefer a 16-bit mode)
            for( m = 0; m < pDevice->dwNumModes; m++ )
            {
                if( pDevice->modes[m].Width == 640 && pDevice->modes[m].Height == 480 )
                {
                    pDevice->dwCurrentMode = m;
                    if( pDevice->modes[m].Format == D3DFMT_R5G6B5 ||
                        pDevice->modes[m].Format == D3DFMT_X1R5G5B5 ||
                        pDevice->modes[m].Format == D3DFMT_A1R5G5B5 )
                    {
                        break;
                    }
                }
            }

            // Check if the device is compatible with the desktop display mode
            // (which was added initially as formats[0])
            if( bFormatConfirmed[0] )
            {
                pDevice->bCanDoWindowed = true;
                pDevice->bWindowed      = true;
            }

            // If valid modes were found, keep this device
            if( pDevice->dwNumModes > 0 )
            {
                pAdapter->dwNumDevices++;
                if( pDevice->DeviceType == D3DDEVTYPE_SW )
                    pAdapter->bHasAppCompatSW = true;
                else if( pDevice->DeviceType == D3DDEVTYPE_HAL )
                    pAdapter->bHasAppCompatHAL = true;
            }
        }

        // Count adapters even if no devices, so we can throw up blank windows on them
        m_dwNumAdapters++;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CheckWindowedFormat()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::CheckWindowedFormat( UINT iAdapter, D3DWindowedModeInfo* pD3DWindowedModeInfo )
{
    HRESULT hr;
    D3DAdapterInfo* pD3DAdapterInfo = m_Adapters[iAdapter];
    D3DDeviceInfo* pD3DDeviceInfo = &pD3DAdapterInfo->devices[pD3DAdapterInfo->dwCurrentDevice];
    bool bFormatConfirmed = false;

    if( FAILED( hr = m_pD3D->CheckDeviceType( iAdapter, pD3DDeviceInfo->DeviceType,
        pD3DAdapterInfo->d3ddmDesktop.Format, pD3DWindowedModeInfo->BackBufferFormat, TRUE ) ) )
    {
        return hr;
    }

    // Confirm the device/format for HW vertex processing
    if( pD3DDeviceInfo->d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT )
    {
        if( pD3DDeviceInfo->d3dCaps.DevCaps & D3DDEVCAPS_PUREDEVICE )
        {
            pD3DWindowedModeInfo->dwBehavior = D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                               D3DCREATE_PUREDEVICE;

            if( SUCCEEDED( ConfirmDevice( &pD3DDeviceInfo->d3dCaps, pD3DWindowedModeInfo->dwBehavior,
                                          pD3DWindowedModeInfo->BackBufferFormat ) ) )
                bFormatConfirmed = true;
        }

        if( !bFormatConfirmed )
        {
            pD3DWindowedModeInfo->dwBehavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;

            if( SUCCEEDED( ConfirmDevice( &pD3DDeviceInfo->d3dCaps, pD3DWindowedModeInfo->dwBehavior,
                                          pD3DWindowedModeInfo->BackBufferFormat ) ) )
                bFormatConfirmed = true;
        }

        if( !bFormatConfirmed )
        {
            pD3DWindowedModeInfo->dwBehavior = D3DCREATE_MIXED_VERTEXPROCESSING;

            if( SUCCEEDED( ConfirmDevice( &pD3DDeviceInfo->d3dCaps, pD3DWindowedModeInfo->dwBehavior,
                                          pD3DWindowedModeInfo->BackBufferFormat ) ) )
                bFormatConfirmed = true;
        }
    }

    // Confirm the device/format for SW vertex processing
    if( !bFormatConfirmed )
    {
        pD3DWindowedModeInfo->dwBehavior = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

        if( SUCCEEDED( ConfirmDevice( &pD3DDeviceInfo->d3dCaps, pD3DWindowedModeInfo->dwBehavior,
                                      pD3DWindowedModeInfo->BackBufferFormat ) ) )
            bFormatConfirmed = true;
    }

    if( !bFormatConfirmed )
        return E_FAIL;

    if( m_bMultithreaded )
    {
        pD3DWindowedModeInfo->dwBehavior |= D3DCREATE_MULTITHREADED;
    }

    // Find a suitable depth/stencil buffer format for this device/format
    if( m_bUseDepthBuffer )
    {
        if( !FindDepthStencilFormat( iAdapter, pD3DDeviceInfo->DeviceType,
            pD3DWindowedModeInfo->BackBufferFormat, &pD3DWindowedModeInfo->DepthStencilFormat ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FindDepthStencilFormat()
// Desc: Finds a depth/stencil format for the given device that is compatible
//       with the render target format and meets the needs of the app.
//-----------------------------------------------------------------------------
BOOL CD3DScreensaver::FindDepthStencilFormat( UINT iAdapter, D3DDEVTYPE DeviceType,
    D3DFORMAT TargetFormat, D3DFORMAT* pDepthStencilFormat )
{
    if( m_dwMinDepthBits <= 16 && m_dwMinStencilBits == 0 )
    {
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( iAdapter, DeviceType,
            TargetFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16 ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( iAdapter, DeviceType,
                TargetFormat, TargetFormat, D3DFMT_D16 ) ) )
            {
                *pDepthStencilFormat = D3DFMT_D16;
                return TRUE;
            }
        }
    }

    if( m_dwMinDepthBits <= 15 && m_dwMinStencilBits <= 1 )
    {
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( iAdapter, DeviceType,
            TargetFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D15S1 ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( iAdapter, DeviceType,
                TargetFormat, TargetFormat, D3DFMT_D15S1 ) ) )
            {
                *pDepthStencilFormat = D3DFMT_D15S1;
                return TRUE;
            }
        }
    }

    if( m_dwMinDepthBits <= 24 && m_dwMinStencilBits == 0 )
    {
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( iAdapter, DeviceType,
            TargetFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X8 ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( iAdapter, DeviceType,
                TargetFormat, TargetFormat, D3DFMT_D24X8 ) ) )
            {
                *pDepthStencilFormat = D3DFMT_D24X8;
                return TRUE;
            }
        }
    }

    if( m_dwMinDepthBits <= 24 && m_dwMinStencilBits <= 8 )
    {
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( iAdapter, DeviceType,
            TargetFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8 ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( iAdapter, DeviceType,
                TargetFormat, TargetFormat, D3DFMT_D24S8 ) ) )
            {
                *pDepthStencilFormat = D3DFMT_D24S8;
                return TRUE;
            }
        }
    }

    if( m_dwMinDepthBits <= 24 && m_dwMinStencilBits <= 4 )
    {
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( iAdapter, DeviceType,
            TargetFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X4S4 ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( iAdapter, DeviceType,
                TargetFormat, TargetFormat, D3DFMT_D24X4S4 ) ) )
            {
                *pDepthStencilFormat = D3DFMT_D24X4S4;
                return TRUE;
            }
        }
    }

    if( m_dwMinDepthBits <= 32 && m_dwMinStencilBits == 0 )
    {
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( iAdapter, DeviceType,
            TargetFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D32 ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( iAdapter, DeviceType,
                TargetFormat, TargetFormat, D3DFMT_D32 ) ) )
            {
                *pDepthStencilFormat = D3DFMT_D32;
                return TRUE;
            }
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: Cleanup3DEnvironment()
// Desc:
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::Cleanup3DEnvironment()
{
    RenderUnit* pRenderUnit;

    for( DWORD iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
    {
        pRenderUnit = &m_RenderUnits[iRenderUnit];
        SwitchToRenderUnit( iRenderUnit );
        if( pRenderUnit->bDeviceObjectsRestored )
        {
            InvalidateDeviceObjects();
            pRenderUnit->bDeviceObjectsRestored = false;
        }
        if( pRenderUnit->bDeviceObjectsInited )
        {
            DeleteDeviceObjects();
            pRenderUnit->bDeviceObjectsInited = false;
        }
        SAFE_RELEASE( m_pd3dDevice );
    }
    m_dwNumRenderUnits = 0;
    SAFE_RELEASE( m_pD3D );
}




//-----------------------------------------------------------------------------
// Name: Render3DEnvironment()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::Render3DEnvironment()
{
    HRESULT hr;
    RenderUnit* pRenderUnit;
    D3DAdapterInfo* pAdapterInfo;
    DWORD iRenderUnit;

    m_fTime        = DXUtil_Timer( TIMER_GETAPPTIME );
    m_fElapsedTime = DXUtil_Timer( TIMER_GETELAPSEDTIME );

    // Tell client to update the world
    FrameMove();
    UpdateFrameStats();

    for( iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
    {
        pRenderUnit = &m_RenderUnits[iRenderUnit];
        pAdapterInfo = m_Adapters[pRenderUnit->iAdapter];

        SwitchToRenderUnit( iRenderUnit );

        if( m_pd3dDevice == NULL )
            continue;

        // Test the cooperative level to see if it's okay to render
        if( FAILED( hr = m_pd3dDevice->TestCooperativeLevel() ) )
        {
            // If the device was lost, do not render until we get it back
            if( D3DERR_DEVICELOST == hr )
                return S_OK;

            // Check if the device needs to be reset.
            if( D3DERR_DEVICENOTRESET == hr )
            {
                // If we are windowed, read the desktop mode and use the same format for
                // the back buffer
                if( m_bWindowed )
                {
                    m_pD3D->GetAdapterDisplayMode( pRenderUnit->iAdapter, &pAdapterInfo->d3ddmDesktop );
                }

                if( pRenderUnit->bDeviceObjectsRestored )
                {
                    InvalidateDeviceObjects();
                    pRenderUnit->bDeviceObjectsRestored = false;
                }
                if( FAILED( hr = m_pd3dDevice->Reset( &pRenderUnit->d3dpp ) ) ||
                    FAILED( hr = RestoreDeviceObjects() ) )
                {
                    m_bErrorMode = true;
                    continue;
                }
                pRenderUnit->bDeviceObjectsRestored = true;
            }
        }

        // Tell client to render using the current device
        if( !m_bErrorMode )
            Render();
    }

    // No device at all (for example every render unit failed): show the error text
    if( m_pd3dDevice == NULL )
        m_bErrorMode = true;

    // Call Present() in a separate loop once all rendering is done
    // so multiple monitors are as closely synced visually as possible
    if( !m_bErrorMode )
    {
        for( iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
        {
            SwitchToRenderUnit( iRenderUnit );
            // Present the results of the rendering to the screen
            m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateErrorBox()
// Desc: Update the error message.  The text fades in, stays for a while,
//       fades out and then reappears at a random position (so that a static
//       error message cannot burn into the screen it is supposed to save).
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::UpdateErrorBox()
{
    MonitorInfo* pMonitorInfo;
    HWND hwnd;
    RECT rcBounds;
    static DWORD dwTimeLast = 0;
    static FLOAT fTime = 0.0f;       // seconds since the text was (re)positioned
    DWORD dwTimeNow;

    // Make sure all the RenderUnits / D3D devices have been torn down
    // so the error box is visible
    if( m_bErrorMode && m_dwNumRenderUnits > 0 )
    {
        Cleanup3DEnvironment();
    }

    // Update timing to determine how much to fade the error text
    if( dwTimeLast == 0 )
        dwTimeLast = timeGetTime();
    dwTimeNow = timeGetTime();
    fTime += (FLOAT)( dwTimeNow - dwTimeLast ) / 1000.0f;
    if( fTime < 0.0f )
        fTime = 0.0f;
    dwTimeLast = dwTimeNow;

    // Load error string if necessary
    if( m_szError[0] == TEXT('\0') )
    {
        GetTextForError( m_hrError, m_szError, sizeof(m_szError) / sizeof(TCHAR) );
    }

    for( DWORD iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
    {
        pMonitorInfo = &m_Monitors[iMonitor];
        hwnd = pMonitorInfo->hWnd;
        if( hwnd == NULL )
            continue;
        if( m_SaverMode == sm_full )
        {
            rcBounds = pMonitorInfo->rcScreen;
            ScreenToClient( hwnd, (POINT*)&rcBounds.left );
            ScreenToClient( hwnd, (POINT*)&rcBounds.right );
        }
        else
        {
            rcBounds = m_rcRenderTotal;
        }

        if( pMonitorInfo->widthError == 0.0f )
        {
            if( m_SaverMode == sm_preview )
            {
                pMonitorInfo->xError = 0.0f;
                pMonitorInfo->yError = 0.0f;
                pMonitorInfo->crError = RGB( 255, 255, 255 );
                pMonitorInfo->widthError = (float)( rcBounds.right - rcBounds.left );
                pMonitorInfo->heightError = (float)( rcBounds.bottom - rcBounds.top );
                InvalidateRect( hwnd, NULL, FALSE );    // Invalidate the hwnd so it gets drawn
                UpdateWindow( hwnd );
            }
            else
            {
                pMonitorInfo->widthError = ERRORBOX_WIDTH;
                pMonitorInfo->heightError = ERRORBOX_HEIGHT;
                pMonitorInfo->xError = ( (float)( rcBounds.right + rcBounds.left ) - pMonitorInfo->widthError ) * 0.5f;
                pMonitorInfo->yError = ( (float)( rcBounds.bottom + rcBounds.top ) - pMonitorInfo->heightError ) * 0.5f;
            }
        }
        else
        {
            if( m_SaverMode != sm_preview )
            {
                RECT rcOld;
                RECT rcNew;
                INT  nGray;

                // Fade the text in and out
                if( fTime < ERRORBOX_FADEIN )
                    nGray = (INT)( fTime * 255.0f );
                else if( fTime > ERRORBOX_HOLD )
                    nGray = 255 - (INT)( ( fTime - ERRORBOX_HOLD ) * 255.0f / ERRORBOX_FADEOUT );
                else
                    nGray = 255;
                pMonitorInfo->crError = (COLORREF)( nGray * 0x010101 );

                SetRect( &rcOld, (INT)pMonitorInfo->xError, (INT)pMonitorInfo->yError,
                    (INT)( pMonitorInfo->xError + pMonitorInfo->widthError ),
                    (INT)( pMonitorInfo->yError + pMonitorInfo->heightError ) );

                // Once it has faded out, move the text somewhere else
                if( fTime > ERRORBOX_PERIOD )
                {
                    pMonitorInfo->xError = ( (float)rcBounds.right - pMonitorInfo->widthError ) *
                        ( (float)rand() / (float)( RAND_MAX + 1 ) ) + (float)rcBounds.left;
                    pMonitorInfo->yError = ( (float)rcBounds.bottom - pMonitorInfo->heightError ) *
                        ( (float)rand() / (float)( RAND_MAX + 1 ) ) + (float)rcBounds.top;
                    fTime = 0.0f;
                    pMonitorInfo->bEraseError = true;
                }

                SetRect( &rcNew, (INT)pMonitorInfo->xError, (INT)pMonitorInfo->yError,
                    (INT)( pMonitorInfo->xError + pMonitorInfo->widthError ),
                    (INT)( pMonitorInfo->yError + pMonitorInfo->heightError ) );

                InvalidateRect( hwnd, &rcOld, FALSE );    // Invalidate old rect so it gets erased
                InvalidateRect( hwnd, &rcNew, FALSE );    // Invalidate new rect so it gets drawn
                UpdateWindow( hwnd );
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: GetTextForError()
// Desc: Translate an HRESULT error code into a string that can be displayed
//       to explain the error.  A class derived from CD3DScreensaver can
//       provide its own version of this function that provides app-specific
//       error translation instead of or in addition to calling this function.
//       This function returns TRUE if a specific error was translated, or
//       FALSE if no specific translation for the HRESULT was found (though
//       it still puts a generic string into pszError).
//-----------------------------------------------------------------------------
BOOL CD3DScreensaver::GetTextForError( HRESULT hr, TCHAR* pszError,
                                       DWORD dwNumChars )
{
    const DWORD dwErrorMap[][2] =
    {
    //  HRESULT, stringID
        { D3DAPPERR_CREATEDEVICEFAILED,      IDS_ERR_CREATEDEVICEFAILED },
        { D3DAPPERR_HALNOTCOMPATIBLE,        IDS_ERR_HALNOTCOMPATIBLE },
        { D3DAPPERR_INITDEVICEOBJECTSFAILED, IDS_ERR_HALNOTCOMPATIBLE },
        { D3DAPPERR_MEDIANOTFOUND,           IDS_ERR_MEDIANOTFOUND },
        { D3DAPPERR_NOCOMPATIBLEDEVICES,     IDS_ERR_NOCOMPATIBLEDEVICES },
        { D3DAPPERR_NODIRECT3D,              IDS_ERR_NODIRECT3D },
        { D3DAPPERR_NOHALTHISMODE,           IDS_ERR_NOHALTHISMODE },
        { D3DAPPERR_NOHARDWAREDEVICE,        IDS_ERR_NOHARDWAREDEVICE },
        { D3DAPPERR_NOPREVIEW,               IDS_ERR_NOPREVIEW },
        { D3DAPPERR_NOWINDOWEDHAL,           IDS_ERR_NOWINDOWEDHAL },
        { D3DAPPERR_RESIZEFAILED,            IDS_ERR_RESIZEFAILED },
        { (DWORD)D3DERR_OUTOFVIDEOMEMORY,    IDS_ERR_OUTOFVIDEOMEMORY },
        { (DWORD)E_FAIL,                     IDS_ERR_GENERIC },
        { (DWORD)E_OUTOFMEMORY,              IDS_ERR_OUTOFMEMORY },
    };
    const DWORD dwErrorMapSize = sizeof(dwErrorMap) / sizeof(DWORD[2]);

    DWORD iError;
    DWORD resid = 0;

    for( iError = 0; iError < dwErrorMapSize; iError++ )
    {
        if( hr == (HRESULT)dwErrorMap[iError][0] )
        {
            resid = dwErrorMap[iError][1];
            break;
        }
    }
    if( resid == 0 )
    {
        resid = IDS_ERR_GENERIC;
    }

    if( LoadString( NULL, resid, pszError, dwNumChars ) == 0 )
    {
        StringCchCopy( pszError, dwNumChars, TEXT("Unspecified error encountered.") );
        return FALSE;
    }

    return ( resid != IDS_ERR_GENERIC );
}




//-----------------------------------------------------------------------------
// Name: UpdateFrameStats()
// Desc: Keep track of the frame count
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::UpdateFrameStats()
{
    UINT iRenderUnit;
    RenderUnit* pRenderUnit;
    UINT iAdapter;
    static FLOAT fLastTime = 0.0f;
    static DWORD dwFrames  = 0L;
    FLOAT fTime = DXUtil_Timer( TIMER_GETABSOLUTETIME );

    ++dwFrames;

    // Update the scene stats once per second
    if( fTime - fLastTime > 1.0f )
    {
        m_fFPS    = dwFrames / ( fTime - fLastTime );
        fLastTime = fTime;
        dwFrames  = 0L;

        for( iRenderUnit = 0; iRenderUnit < m_dwNumRenderUnits; iRenderUnit++ )
        {
            pRenderUnit = &m_RenderUnits[iRenderUnit];
            iAdapter = pRenderUnit->iAdapter;

            // Get adapter's current mode so we can report
            // bit depth (back buffer depth may be unknown)
            D3DDISPLAYMODE mode;
            m_pD3D->GetAdapterDisplayMode( iAdapter, &mode );

            StringCchPrintf( pRenderUnit->strFrameStats, 40, TEXT("%.02f fps (%dx%dx%d)"), m_fFPS,
                             mode.Width, mode.Height,
                             mode.Format == D3DFMT_X8R8G8B8 ? 32 : 16 );
            if( m_bUseDepthBuffer )
            {
                D3DAdapterInfo* pAdapterInfo = m_Adapters[iAdapter];
                D3DDeviceInfo*  pDeviceInfo  = &pAdapterInfo->devices[pAdapterInfo->dwCurrentDevice];
                D3DModeInfo*    pModeInfo    = &pDeviceInfo->modes[pDeviceInfo->dwCurrentMode];

                switch( pModeInfo->DepthStencilFormat )
                {
                case D3DFMT_D16:
                    StringCchCat( pRenderUnit->strFrameStats, 40, TEXT(" (D16)") );
                    break;
                case D3DFMT_D15S1:
                    StringCchCat( pRenderUnit->strFrameStats, 40, TEXT(" (D15S1)") );
                    break;
                case D3DFMT_D24X8:
                    StringCchCat( pRenderUnit->strFrameStats, 40, TEXT(" (D24X8)") );
                    break;
                case D3DFMT_D24S8:
                    StringCchCat( pRenderUnit->strFrameStats, 40, TEXT(" (D24S8)") );
                    break;
                case D3DFMT_D24X4S4:
                    StringCchCat( pRenderUnit->strFrameStats, 40, TEXT(" (D24X4S4)") );
                    break;
                case D3DFMT_D32:
                    StringCchCat( pRenderUnit->strFrameStats, 40, TEXT(" (D32)") );
                    break;
                default:
                    break;
                }
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: DoPaint()
// Desc: Paints the error text (and blackness everywhere else) on a saver window
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::DoPaint( HWND hwnd, HDC hdc )
{
    HMONITOR hMonitor = MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST );
    MonitorInfo* pMonitorInfo = NULL;
    DWORD iMonitor;
    for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
    {
        pMonitorInfo = &m_Monitors[iMonitor];
        if( pMonitorInfo->hMonitor == hMonitor )
            break;
    }

    if( iMonitor == m_dwNumMonitors )
        return;

    HBRUSH hbrBlack = (HBRUSH)GetStockObject( BLACK_BRUSH );

    // Draw the error message
    RECT rc;
    SetRect( &rc, (INT)pMonitorInfo->xError, (INT)pMonitorInfo->yError,
        (INT)( pMonitorInfo->xError + pMonitorInfo->widthError ),
        (INT)( pMonitorInfo->yError + pMonitorInfo->heightError ) );

    if( pMonitorInfo->bEraseError && m_SaverMode != sm_preview )
    {
        FillRect( hdc, &rc, hbrBlack );
        pMonitorInfo->bEraseError = false;
    }

    INT nPointSize = ( m_SaverMode == sm_preview ) ? 9 : 16;
    HFONT hFont = CreateFont( -MulDiv( nPointSize, GetDeviceCaps( hdc, LOGPIXELSY ), 72 ),
                              0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                              DEFAULT_PITCH, TEXT("Segoe UI") );
    HGDIOBJ hFontOld = SelectObject( hdc, hFont );

    RECT rc2;
    int height;
    rc2 = rc;
    height = DrawText( hdc, m_szError, -1, &rc, DT_WORDBREAK | DT_CENTER | DT_CALCRECT );
    rc = rc2;

    rc2.top = ( rc.bottom + rc.top - height ) / 2;

    SetBkColor( hdc, RGB( 0, 0, 0 ) );
    SetTextColor( hdc, pMonitorInfo->crError );
    DrawText( hdc, m_szError, -1, &rc2, DT_WORDBREAK | DT_CENTER );

    // Erase everywhere except the error message box
    ExcludeClipRect( hdc, rc.left, rc.top, rc.right, rc.bottom );
    rc = pMonitorInfo->rcScreen;
    ScreenToClient( hwnd, (POINT*)&rc.left );
    ScreenToClient( hwnd, (POINT*)&rc.right );
    if( hbrBlack != NULL )
        FillRect( hdc, &rc, hbrBlack );

    if( hFontOld != NULL )
    {
        SelectObject( hdc, hFontOld );
        DeleteObject( hFontOld );       // (sic)
    }
    if( hFont != NULL )
        DeleteObject( hFont );
    if( hbrBlack != NULL )
        DeleteObject( hbrBlack );
}




//-----------------------------------------------------------------------------
// Name: DisplayErrorMsg()
// Desc: Displays error messages in a message box
//-----------------------------------------------------------------------------
HRESULT CD3DScreensaver::DisplayErrorMsg( HRESULT hr, DWORD dwType )
{
    TCHAR strMsg[512];

    (void)dwType;
    GetTextForError( hr, strMsg, 512 );

    MessageBox( m_hWnd, strMsg, m_strWindowTitle, MB_ICONERROR | MB_OK );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: ReadScreenSettings()
// Desc: Read the registry settings that affect how the screens are set up and
//       used.
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::ReadScreenSettings( HKEY hkeyParent )
{
    TCHAR strKey[100];
    DWORD iMonitor;
    MonitorInfo* pMonitorInfo;
    DWORD iAdapter;
    D3DAdapterInfo* pD3DAdapterInfo;
    HKEY hkey;
    DWORD dwLength;
    DWORD dwLength2;
    GUID guidAdapterID;
    GUID guidZero;
    ZeroMemory( &guidAdapterID, sizeof(GUID) );
    ZeroMemory( &guidZero, sizeof(GUID) );

    dwLength = sizeof(DWORD);
    RegQueryValueEx( hkeyParent, TEXT("AllScreensSame"), NULL, NULL,
        (BYTE*)&m_bAllScreensSame, &dwLength );
    for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
    {
        pMonitorInfo = &m_Monitors[iMonitor];
        iAdapter = pMonitorInfo->iAdapter;
        if( iAdapter == NO_ADAPTER )
            continue;
        pD3DAdapterInfo = m_Adapters[iAdapter];
        StringCchPrintf( strKey, 100, TEXT("Screen %d"), iMonitor + 1 );
        if( ERROR_SUCCESS == RegCreateKeyEx( hkeyParent, strKey,
            0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hkey, NULL ) )
        {
            dwLength2 = sizeof(GUID);
            RegQueryValueEx( hkey, TEXT("Adapter ID"), NULL, NULL,
                (BYTE*)&guidAdapterID, &dwLength2 );

            dwLength = sizeof(DWORD);
            RegQueryValueEx( hkey, TEXT("Leave Black"), NULL, NULL,
                (BYTE*)&pD3DAdapterInfo->bLeaveBlack, &dwLength );

            if( guidAdapterID == pD3DAdapterInfo->d3dAdapterIdentifier.DeviceIdentifier ||
                guidAdapterID == guidZero )
            {
                dwLength = sizeof(DWORD);
                RegQueryValueEx( hkey, TEXT("Disable Hardware"), NULL, NULL,
                    (BYTE*)&pD3DAdapterInfo->bDisableHW, &dwLength );
                dwLength = sizeof(DWORD);
                RegQueryValueEx( hkey, TEXT("Width"), NULL, NULL,
                    (BYTE*)&pD3DAdapterInfo->dwUserPrefWidth, &dwLength );
                dwLength = sizeof(DWORD);
                RegQueryValueEx( hkey, TEXT("Height"), NULL, NULL,
                    (BYTE*)&pD3DAdapterInfo->dwUserPrefHeight, &dwLength );
                dwLength = sizeof(DWORD);
                RegQueryValueEx( hkey, TEXT("Format"), NULL, NULL,
                    (BYTE*)&pD3DAdapterInfo->d3dfmtUserPrefFormat, &dwLength );
            }
            RegCloseKey( hkey );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: WriteScreenSettings()
// Desc: Write the registry settings that affect how the screens are set up
//       and used.
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::WriteScreenSettings( HKEY hkeyParent )
{
    TCHAR strKey[100];
    DWORD iMonitor;
    MonitorInfo* pMonitorInfo;
    DWORD iAdapter;
    D3DAdapterInfo* pD3DAdapterInfo;
    HKEY hkey;

    RegSetValueEx( hkeyParent, TEXT("AllScreensSame"), 0, REG_DWORD,
        (BYTE*)&m_bAllScreensSame, sizeof(DWORD) );
    for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
    {
        pMonitorInfo = &m_Monitors[iMonitor];
        iAdapter = pMonitorInfo->iAdapter;
        if( iAdapter == NO_ADAPTER )
            continue;
        pD3DAdapterInfo = m_Adapters[iAdapter];
        StringCchPrintf( strKey, 100, TEXT("Screen %d"), iMonitor + 1 );
        if( ERROR_SUCCESS == RegCreateKeyEx( hkeyParent, strKey,
            0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hkey, NULL ) )
        {
            RegSetValueEx( hkey, TEXT("Leave Black"), 0, REG_DWORD,
                (BYTE*)&pD3DAdapterInfo->bLeaveBlack, sizeof(DWORD) );
            RegSetValueEx( hkey, TEXT("Disable Hardware"), 0, REG_DWORD,
                (BYTE*)&pD3DAdapterInfo->bDisableHW, sizeof(DWORD) );
            RegSetValueEx( hkey, TEXT("Width"), 0, REG_DWORD,
                (BYTE*)&pD3DAdapterInfo->dwUserPrefWidth, sizeof(DWORD) );
            RegSetValueEx( hkey, TEXT("Height"), 0, REG_DWORD,
                (BYTE*)&pD3DAdapterInfo->dwUserPrefHeight, sizeof(DWORD) );
            RegSetValueEx( hkey, TEXT("Format"), 0, REG_DWORD,
                (BYTE*)&pD3DAdapterInfo->d3dfmtUserPrefFormat, sizeof(DWORD) );
            RegSetValueEx( hkey, TEXT("Adapter ID"), 0, REG_BINARY,
                (BYTE*)&pD3DAdapterInfo->d3dAdapterIdentifier.DeviceIdentifier, sizeof(GUID) );
            RegCloseKey( hkey );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: DoScreenSettingsDialog()
// Desc:
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::DoScreenSettingsDialog( HWND hwndParent )
{
    LPCTSTR pstrTemplate;

    if( m_dwNumAdapters > 1 && !m_bOneScreenOnly )
        pstrTemplate = MAKEINTRESOURCE( IDD_MULTIMONITORSETTINGS );
    else
        pstrTemplate = MAKEINTRESOURCE( IDD_SINGLEMONITORSETTINGS );

    DialogBox( m_hInstance, pstrTemplate, hwndParent, ScreenSettingsDlgProcStub );
}




//-----------------------------------------------------------------------------
// Name: ScreenSettingsDlgProcStub()
// Desc:
//-----------------------------------------------------------------------------
INT_PTR CALLBACK CD3DScreensaver::ScreenSettingsDlgProcStub( HWND hWnd, UINT uMsg,
                                                             WPARAM wParam, LPARAM lParam )
{
    return s_pD3DScreensaver->ScreenSettingsDlgProc( hWnd, uMsg, wParam, lParam );
}




// We need to store a copy of the original screen settings so that the user
// can modify those settings in the dialog, then hit Cancel and have the
// original settings restored.
static D3DAdapterInfo* s_AdaptersSave[MAX_DISPLAYS];
static BOOL s_bAllScreensSameSave;

//-----------------------------------------------------------------------------
// Name: ScreenSettingsDlgProc()
// Desc:
//-----------------------------------------------------------------------------
INT_PTR CD3DScreensaver::ScreenSettingsDlgProc( HWND hWnd, UINT uMsg, WPARAM wParam,
                                                LPARAM lParam )
{
    HWND hwndTabs = GetDlgItem( hWnd, IDC_MONITORSTAB );
    HWND hwndModeList = GetDlgItem( hWnd, IDC_MODESCOMBO );
    DWORD iMonitor;
    MonitorInfo* pMonitorInfo;
    DWORD iAdapter;
    D3DAdapterInfo* pD3DAdapterInfo;
    D3DDeviceInfo* pD3DDeviceInfo;
    D3DModeInfo* pD3DModeInfo;

    switch( uMsg )
    {
    case WM_INITDIALOG:
        {
            INT i = 0;
            TC_ITEM tie;
            TCHAR szFmt[100];
            TCHAR sz[100];

            GetWindowText( GetDlgItem( hWnd, IDC_TABNAMEFMT ), szFmt, 100 );

            tie.mask = TCIF_TEXT | TCIF_IMAGE;
            tie.iImage = -1;
            for( iMonitor = 0; iMonitor < m_dwNumMonitors; iMonitor++ )
            {
                StringCchPrintf( sz, 100, szFmt, iMonitor + 1 );
                tie.pszText = sz;
                TabCtrl_InsertItem( hwndTabs, i++, &tie );
            }
            for( iAdapter = 0; iAdapter < m_dwNumAdapters; iAdapter++ )
            {
                s_AdaptersSave[iAdapter] = new D3DAdapterInfo;
                if( s_AdaptersSave[iAdapter] != NULL )
                    *s_AdaptersSave[iAdapter] = *m_Adapters[iAdapter];
            }
            s_bAllScreensSameSave = m_bAllScreensSame;
            SetupAdapterPage( hWnd );
            CheckDlgButton( hWnd, IDC_SAME, ( m_bAllScreensSame ? BST_CHECKED : BST_UNCHECKED ) );
        }
        return TRUE;

    case WM_NOTIFY:
        {
            NMHDR* pnmh = (LPNMHDR)lParam;
            UINT code = pnmh->code;
            if( code == TCN_SELCHANGE )
            {
                SetupAdapterPage( hWnd );
            }
        }
        return TRUE;

    case WM_COMMAND:
        switch( LOWORD( wParam ) )
        {
        case IDC_MODESCOMBO:
            if( HIWORD( wParam ) == CBN_SELCHANGE )
            {
                DWORD iSel;
                DWORD iMode;

                if( m_bOneScreenOnly )
                {
                    GetBestAdapter( &iAdapter );
                    if( iAdapter != NO_ADAPTER )
                        iMonitor = m_Adapters[iAdapter]->iMonitor;
                    else
                        iMonitor = TabCtrl_GetCurSel( hwndTabs );
                }
                else
                {
                    iMonitor = TabCtrl_GetCurSel( hwndTabs );
                }
                pMonitorInfo = &m_Monitors[iMonitor];
                iAdapter = pMonitorInfo->iAdapter;
                pD3DAdapterInfo = m_Adapters[iAdapter];
                iSel = ComboBox_GetCurSel( hwndModeList );
                if( iSel == 0 )
                {
                    // "Automatic"
                    pD3DAdapterInfo->dwUserPrefWidth = 0;
                    pD3DAdapterInfo->dwUserPrefHeight = 0;
                    pD3DAdapterInfo->d3dfmtUserPrefFormat = D3DFMT_UNKNOWN;
                }
                else
                {
                    pD3DDeviceInfo = &pD3DAdapterInfo->devices[pD3DAdapterInfo->dwCurrentDevice];
                    iMode = (DWORD)ComboBox_GetItemData( hwndModeList, iSel );
                    pD3DModeInfo = &pD3DDeviceInfo->modes[iMode];
                    pD3DAdapterInfo->dwUserPrefWidth = pD3DModeInfo->Width;
                    pD3DAdapterInfo->dwUserPrefHeight = pD3DModeInfo->Height;
                    pD3DAdapterInfo->d3dfmtUserPrefFormat = pD3DModeInfo->Format;
                }
            }
            break;

        case IDC_DISABLEHW:
            if( m_bOneScreenOnly )
            {
                GetBestAdapter( &iAdapter );
                if( iAdapter != NO_ADAPTER )
                    iMonitor = m_Adapters[iAdapter]->iMonitor;
                else
                    iMonitor = TabCtrl_GetCurSel( hwndTabs );
            }
            else
            {
                iMonitor = TabCtrl_GetCurSel( hwndTabs );
            }
            pMonitorInfo = &m_Monitors[iMonitor];
            iAdapter = pMonitorInfo->iAdapter;
            pD3DAdapterInfo = m_Adapters[iAdapter];
            if( IsDlgButtonChecked( hWnd, IDC_DISABLEHW ) == BST_CHECKED )
                pD3DAdapterInfo->bDisableHW = TRUE;
            else
                pD3DAdapterInfo->bDisableHW = FALSE;
            SetupAdapterPage( hWnd );
            break;

        case IDC_MOREINFO:
            {
                if( m_bOneScreenOnly )
                {
                    GetBestAdapter( &iAdapter );
                    if( iAdapter != NO_ADAPTER )
                        iMonitor = m_Adapters[iAdapter]->iMonitor;
                    else
                        iMonitor = TabCtrl_GetCurSel( hwndTabs );
                }
                else
                {
                    iMonitor = TabCtrl_GetCurSel( hwndTabs );
                }
                pMonitorInfo = &m_Monitors[iMonitor];

                // Accelerated / Unaccelerated settings
                bool bHasHAL = false;
                bool bHasAppCompatHAL = false;
                bool bDisabledHAL = false;
                bool bHasSW = false;
                bool bHasAppCompatSW = false;
                UINT resid;

                if( pMonitorInfo->hMonitor != NULL &&
                    ( pD3DAdapterInfo = m_Adapters[pMonitorInfo->iAdapter] ) != NULL )
                {
                    bHasHAL = pD3DAdapterInfo->bHasHAL;
                    bHasAppCompatHAL = pD3DAdapterInfo->bHasAppCompatHAL;
                    bDisabledHAL = ( pD3DAdapterInfo->bDisableHW != FALSE );
                    bHasSW = pD3DAdapterInfo->bHasSW;
                    bHasAppCompatSW = pD3DAdapterInfo->bHasAppCompatSW;
                }

                if( bHasHAL && !bDisabledHAL && bHasAppCompatHAL )
                {
                    // Good HAL
                    resid = IDS_INFO_GOODHAL;
                }
                else if( bHasHAL && bDisabledHAL )
                {
                    // Disabled HAL
                    if( bHasSW && bHasAppCompatSW )
                        resid = IDS_INFO_DISABLEDHAL_GOODSW;
                    else if( bHasSW )
                        resid = IDS_INFO_DISABLEDHAL_BADSW;
                    else
                        resid = IDS_INFO_DISABLEDHAL_NOSW;
                }
                else if( bHasHAL && !bHasAppCompatHAL )
                {
                    // Bad HAL
                    if( bHasSW && bHasAppCompatSW )
                        resid = IDS_INFO_BADHAL_GOODSW;
                    else if( bHasSW )
                        resid = IDS_INFO_BADHAL_BADSW;
                    else
                        resid = IDS_INFO_BADHAL_NOSW;
                }
                else
                {
                    // No HAL
                    if( bHasSW && bHasAppCompatSW )
                        resid = IDS_INFO_NOHAL_GOODSW;
                    else if( bHasSW )
                        resid = IDS_INFO_NOHAL_BADSW;
                    else
                        resid = IDS_INFO_NOHAL_NOSW;
                }

                TCHAR szMoreInfo[500];
                LoadString( NULL, resid, szMoreInfo, 500 );
                MessageBox( hWnd, szMoreInfo, pMonitorInfo->strDeviceName, MB_OK | MB_ICONINFORMATION );
            }
            break;

        case IDC_RENDER:
        case IDC_LEAVEBLACK:
            if( m_bOneScreenOnly )
            {
                GetBestAdapter( &iAdapter );
                if( iAdapter != NO_ADAPTER )
                    iMonitor = m_Adapters[iAdapter]->iMonitor;
                else
                    iMonitor = TabCtrl_GetCurSel( hwndTabs );
            }
            else
            {
                iMonitor = TabCtrl_GetCurSel( hwndTabs );
            }
            pMonitorInfo = &m_Monitors[iMonitor];
            iAdapter = pMonitorInfo->iAdapter;
            pD3DAdapterInfo = m_Adapters[iAdapter];
            if( IsDlgButtonChecked( hWnd, IDC_LEAVEBLACK ) == BST_CHECKED )
            {
                pD3DAdapterInfo->bLeaveBlack = TRUE;
                EnableWindow( GetDlgItem( hWnd, IDC_MODESCOMBO ), FALSE );
                EnableWindow( GetDlgItem( hWnd, IDC_MODESSTATIC ), FALSE );
                EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODEBOX ), FALSE );
                EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODENOTE ), FALSE );
            }
            else
            {
                pD3DAdapterInfo->bLeaveBlack = FALSE;
                EnableWindow( GetDlgItem( hWnd, IDC_MODESCOMBO ), TRUE );
                EnableWindow( GetDlgItem( hWnd, IDC_MODESSTATIC ), TRUE );
                EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODEBOX ), TRUE );
                EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODENOTE ), TRUE );
            }
            break;

        case IDC_SAME:
            m_bAllScreensSame = ( IsDlgButtonChecked( hWnd, IDC_SAME ) == BST_CHECKED );
            break;

        case IDOK:
            for( iAdapter = 0; iAdapter < m_dwNumAdapters; iAdapter++ )
            {
                SAFE_DELETE( s_AdaptersSave[iAdapter] );
            }
            EndDialog( hWnd, IDOK );
            break;

        case IDCANCEL:
            // Restore member values to original state
            for( iAdapter = 0; iAdapter < m_dwNumAdapters; iAdapter++ )
            {
                if( s_AdaptersSave[iAdapter] != NULL )
                    *m_Adapters[iAdapter] = *s_AdaptersSave[iAdapter];
                SAFE_DELETE( s_AdaptersSave[iAdapter] );
            }
            m_bAllScreensSame = s_bAllScreensSameSave;
            EndDialog( hWnd, IDCANCEL );
            break;
        }
        return TRUE;

    default:
        return FALSE;
    }
}




//-----------------------------------------------------------------------------
// Name: SetupAdapterPage()
// Desc: Set up the controls for a given page in the Screen Settings dialog.
//-----------------------------------------------------------------------------
VOID CD3DScreensaver::SetupAdapterPage( HWND hWnd )
{
    HWND hwndTabs = GetDlgItem( hWnd, IDC_MONITORSTAB );
    HWND hwndModeList = GetDlgItem( hWnd, IDC_MODESCOMBO );
    UINT iPage = TabCtrl_GetCurFocus( hwndTabs );
    HWND hwndDesc = GetDlgItem( hWnd, IDC_ADAPTERNAME );
    MonitorInfo* pMonitorInfo;
    D3DAdapterInfo* pD3DAdapterInfo;
    D3DDeviceInfo* pD3DDeviceInfo;
    D3DModeInfo* pD3DModeInfo;

    if( m_bOneScreenOnly )
    {
        DWORD iAdapter;
        GetBestAdapter( &iAdapter );
        if( iAdapter != NO_ADAPTER )
        {
            pD3DAdapterInfo = m_Adapters[iAdapter];
            iPage = pD3DAdapterInfo->iMonitor;
        }
    }

    pMonitorInfo = &m_Monitors[iPage];

    SetWindowText( hwndDesc, pMonitorInfo->strDeviceName );

    if( pMonitorInfo->iAdapter == NO_ADAPTER )
        pD3DAdapterInfo = NULL;
    else
        pD3DAdapterInfo = m_Adapters[pMonitorInfo->iAdapter];

    // Accelerated / Unaccelerated settings
    bool bHasHAL = false;
    bool bHasAppCompatHAL = false;
    bool bDisabledHAL = false;
    bool bHasSW = false;
    bool bHasAppCompatSW = false;

    if( pD3DAdapterInfo != NULL )
    {
        bHasHAL = pD3DAdapterInfo->bHasHAL;
        bHasAppCompatHAL = pD3DAdapterInfo->bHasAppCompatHAL;
        bDisabledHAL = ( pD3DAdapterInfo->bDisableHW != FALSE );
        bHasSW = pD3DAdapterInfo->bHasSW;
        bHasAppCompatSW = pD3DAdapterInfo->bHasAppCompatSW;
    }

    TCHAR szStatus[200];
    if( bHasHAL && !bDisabledHAL && bHasAppCompatHAL )
    {
        LoadString( NULL, IDS_RENDERING_HAL, szStatus, 200 );
    }
    else if( bHasSW && bHasAppCompatSW )
    {
        LoadString( NULL, IDS_RENDERING_SW, szStatus, 200 );
    }
    else
    {
        LoadString( NULL, IDS_RENDERING_NONE, szStatus, 200 );
    }
    SetWindowText( GetDlgItem( hWnd, IDC_RENDERING ), szStatus );

    if( bHasHAL && bHasAppCompatHAL )
    {
        EnableWindow( GetDlgItem( hWnd, IDC_DISABLEHW ), TRUE );
        CheckDlgButton( hWnd, IDC_DISABLEHW,
                        pD3DAdapterInfo->bDisableHW ? BST_CHECKED : BST_UNCHECKED );
    }
    else
    {
        EnableWindow( GetDlgItem( hWnd, IDC_DISABLEHW ), FALSE );
        CheckDlgButton( hWnd, IDC_DISABLEHW, BST_UNCHECKED );
    }

    if( bHasAppCompatHAL || bHasAppCompatSW )
    {
        if( pD3DAdapterInfo->bLeaveBlack )
            CheckRadioButton( hWnd, IDC_RENDER, IDC_LEAVEBLACK, IDC_LEAVEBLACK );
        else
            CheckRadioButton( hWnd, IDC_RENDER, IDC_LEAVEBLACK, IDC_RENDER );
        EnableWindow( GetDlgItem( hWnd, IDC_LEAVEBLACK ), TRUE );
        EnableWindow( GetDlgItem( hWnd, IDC_RENDER ), TRUE );
        EnableWindow( GetDlgItem( hWnd, IDC_SCREENUSAGEBOX ), TRUE );
    }
    else
    {
        CheckRadioButton( hWnd, IDC_RENDER, IDC_LEAVEBLACK, IDC_LEAVEBLACK );
        EnableWindow( GetDlgItem( hWnd, IDC_LEAVEBLACK ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDC_RENDER ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDC_SCREENUSAGEBOX ), FALSE );
    }

    if( IsDlgButtonChecked( hWnd, IDC_LEAVEBLACK ) == BST_CHECKED )
    {
        EnableWindow( GetDlgItem( hWnd, IDC_MODESCOMBO ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDC_MODESSTATIC ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODEBOX ), FALSE );
        EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODENOTE ), FALSE );
    }
    else
    {
        EnableWindow( GetDlgItem( hWnd, IDC_MODESCOMBO ), TRUE );
        EnableWindow( GetDlgItem( hWnd, IDC_MODESSTATIC ), TRUE );
        EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODEBOX ), TRUE );
        EnableWindow( GetDlgItem( hWnd, IDC_DISPLAYMODENOTE ), TRUE );
    }

    // Fill in modes list
    ComboBox_ResetContent( hwndModeList );
    if( pD3DAdapterInfo == NULL )
        return;
    TCHAR strAutomatic[100];
    GetWindowText( GetDlgItem( hWnd, IDC_AUTOMATIC ), strAutomatic, 100 );
    ComboBox_AddString( hwndModeList, strAutomatic );
    ComboBox_SetItemData( hwndModeList, 0, -1 );
    pD3DDeviceInfo = &pD3DAdapterInfo->devices[pD3DAdapterInfo->dwCurrentDevice];
    DWORD iSelInitial = 0;
    TCHAR strModeFmt[100];

    GetWindowText( GetDlgItem( hWnd, IDC_MODEFMT ), strModeFmt, 100 );
    for( DWORD iMode = 0; iMode < pD3DDeviceInfo->dwNumModes; iMode++ )
    {
        DWORD dwBitDepth;
        TCHAR strMode[80];
        DWORD dwItem;

        pD3DModeInfo = &pD3DDeviceInfo->modes[iMode];
        dwBitDepth = 16;
        if( pD3DModeInfo->Format == D3DFMT_X8R8G8B8 ||
            pD3DModeInfo->Format == D3DFMT_A8R8G8B8 ||
            pD3DModeInfo->Format == D3DFMT_R8G8B8 )
        {
            dwBitDepth = 32;
        }

        StringCchPrintf( strMode, 80, strModeFmt, pD3DModeInfo->Width,
                         pD3DModeInfo->Height, dwBitDepth );
        dwItem = ComboBox_AddString( hwndModeList, strMode );
        ComboBox_SetItemData( hwndModeList, dwItem, iMode );

        if( pD3DModeInfo->Width == pD3DAdapterInfo->dwUserPrefWidth &&
            pD3DModeInfo->Height == pD3DAdapterInfo->dwUserPrefHeight &&
            pD3DModeInfo->Format == pD3DAdapterInfo->d3dfmtUserPrefFormat )
        {
            iSelInitial = dwItem;
        }
    }
    ComboBox_SetCurSel( hwndModeList, iSelInitial );
}
