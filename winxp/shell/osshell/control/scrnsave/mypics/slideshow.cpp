//-----------------------------------------------------------------------------
// File: slideshow.cpp
//
// Desc: The slide show of the My Pictures Slideshow screen saver
//-----------------------------------------------------------------------------
#include <windows.h>
#include "slideshow.h"
#include "findimages.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Name: CImageList::Add()
// Desc: (always true)
//-----------------------------------------------------------------------------
bool CImageList::Add( LPCWSTR pszFile )
{
    if( pszFile != NULL )
    {
        CStr strFile( pszFile );
        m_Files.Append( strFile );
    }
    return true;
}




//-----------------------------------------------------------------------------
// Name: CImageList::Shuffle()
// Desc:
//-----------------------------------------------------------------------------
void CImageList::Shuffle()
{
    for( int i = 0; i < m_Files.Size(); i++ )
    {
        int j = i;
        if( m_Files.Size() - i != 0 )
            j = NextRandom( &m_nSeed ) % (m_Files.Size() - i) + i;

        CStr strFile( m_Files[i] );
        m_Files[i] = m_Files[j];
        m_Files[j] = strFile;
    }
}




//-----------------------------------------------------------------------------
// Name: CImageList::GetNext()
// Desc: The next picture, from the first one again after the last
//-----------------------------------------------------------------------------
bool CImageList::GetNext( CStr& strFile )
{
    strFile = L"";

    if( m_Files.Size() == 0 )
        return false;

    if( m_nCurrent >= m_Files.Size() )
        m_nCurrent = 0;

    strFile = m_Files[m_nCurrent];
    m_nCurrent++;

    return strFile.Length() != 0;
}




//-----------------------------------------------------------------------------
// Name: CImageList::GetPrevious()
// Desc: The picture before the one shown (m_nCurrent is the next one)
//-----------------------------------------------------------------------------
bool CImageList::GetPrevious( CStr& strFile )
{
    strFile = L"";

    int nCount = m_Files.Size();
    if( nCount == 0 )
        return false;

    if( nCount < 2 )
    {
        m_nCurrent = 0;
        strFile = m_Files[0];
        return strFile.Length() != 0;
    }

    m_nCurrent--;
    if( m_nCurrent < 0 )
        m_nCurrent = nCount - 1;

    int i = m_nCurrent - 1;
    if( i < 0 )
        i = nCount - 1;

    strFile = m_Files[i];
    return strFile.Length() != 0;
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::CSlideshow()
// Desc: Reads the settings and collects the monitors.  (m_rcWindow is set by
//       SetWindow; the object comes zeroed from LocalAlloc.)
//-----------------------------------------------------------------------------
CSlideshow::CSlideshow( HINSTANCE hInstance, const CStr& strRegistryKey )
    : m_pTransition(NULL), m_hInstance(hInstance),
      m_Settings(HKEY_CURRENT_USER, strRegistryKey), m_Monitors(10), m_Screens(10)
{
    EnumDisplayMonitors( NULL, NULL, MonitorEnumProc, (LPARAM)this );
}

CSlideshow::~CSlideshow()
{
    delete m_pTransition;
    m_pTransition = NULL;
}

BOOL CALLBACK CSlideshow::MonitorEnumProc( HMONITOR hMonitor, HDC hdc, LPRECT prc, LPARAM lParam )
{
    CSlideshow* pThis = (CSlideshow*)lParam;

    if( pThis != NULL && hMonitor != NULL )
    {
        MONITORINFOEXW mi;
        ZeroMemory( &mi, sizeof(mi) );
        mi.cbSize = sizeof(mi);
        if( GetMonitorInfoW( hMonitor, &mi ) )
            pThis->m_Monitors.Append( mi.rcMonitor );
    }
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::SetWindow()
// Desc: The parts of the window on each monitor
//-----------------------------------------------------------------------------
void CSlideshow::SetWindow( HWND hWnd )
{
    GetClientRect( hWnd, &m_rcWindow );

    m_Screens.Destroy();
    for( int i = 0; i < m_Monitors.Size(); i++ )
    {
        RECT rcMonitor = m_Monitors[i];
        RECT rcScreen;

        MapWindowPoints( NULL, hWnd, (LPPOINT)&rcMonitor, 2 );
        if( IntersectRect( &rcScreen, &m_rcWindow, &rcMonitor ) )
            m_Screens.Append( rcScreen );
    }
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::StartSearch()
// Desc: Starts the thread that looks for the pictures; returns its handle
//-----------------------------------------------------------------------------
HANDLE CSlideshow::StartSearch( HWND hWnd, UINT uMsg, HANDLE hStopEvent )
{
    HANDLE hThread = NULL;

    CStr strMasks( L"" );
    m_Gdiplus.GetImageExtensions( strMasks );

    CImageFinder* pFinder = new CImageFinder( m_Settings.GetImageDirectory(), strMasks, hWnd, uMsg,
                                              hStopEvent, m_Settings.GetMaxFailedFiles(),
                                              m_Settings.GetMaxSuccessfulFiles(),
                                              m_Settings.GetMaxDirectories() );
    if( pFinder != NULL )
    {
        DWORD dwThreadId;
        hThread = CreateThread( NULL, 0, CImageFinder::ThreadProc, pFinder, 0, &dwThreadId );
        if( hThread == NULL )
            delete pFinder;
        else
            SetThreadPriority( hThread, THREAD_PRIORITY_LOWEST );
    }

    return hThread;
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::RandomScreen()
// Desc: One of the screens, at random
//-----------------------------------------------------------------------------
RECT CSlideshow::RandomScreen()
{
    int i = 0;
    int nScreens = m_Screens.Size();
    UINT nSeed = GetTickCount();

    if( nScreens != 0 )
        i = NextRandom( &nSeed ) % nScreens;

    return m_Screens[i];
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::CreateTransition()
// Desc: One of the five transitions, at random.  (A transition that is not
//       valid is deleted, and with it the picture, which the caller then
//       deletes again: the original does this.)
//-----------------------------------------------------------------------------
CTransition* CSlideshow::CreateTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen,
                                           const RECT* prcWindow )
{
    CTransition* pTransition = NULL;
    UINT nSeed = GetTickCount();

    switch( NextRandom( &nSeed ) % 5 )
    {
    case 0:
        pTransition = new CCutTransition( pImage, dc, prcScreen, prcWindow );
        break;
    case 1:
        pTransition = new CSlideTransition( pImage, dc, prcScreen, prcWindow );
        break;
    case 2:
        pTransition = new CBlockTransition( pImage, dc, prcScreen, prcWindow );
        break;
    case 3:
        pTransition = new CFadeTransition( pImage, dc, prcScreen, prcWindow );
        break;
    case 4:
        pTransition = new CCurtainTransition( pImage, dc, prcScreen, prcWindow );
        break;
    default:
        return NULL;
    }

    if( pTransition != NULL )
    {
        if( pTransition->IsValid() )
            return pTransition;
        delete pTransition;
    }
    return NULL;
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::ChangeImage()
// Desc: The next (or previous) picture that can be loaded, on a screen
//       picked at random, with a transition (unless they are turned off or
//       bImmediate).  Without any pictures, a message says so.
//-----------------------------------------------------------------------------
bool CSlideshow::ChangeImage( bool bForward, bool bImmediate )
{
    CStr strFile( L"" );

    delete m_pTransition;
    m_pTransition = NULL;

    if( m_Screens.Size() == 0 )
        return false;

    if( m_Images.Count() == 0 )
    {
        CSlideImage* pImage = new CSlideImage;
        if( pImage != NULL )
        {
            CScopedDC dc;
            if( dc.GetDC( NULL ) )
            {
                RECT rcScreen = RandomScreen();
                CStr strDirectory = m_Settings.GetImageDirectory();
                CStr strMessage;
                strMessage.Format( IDS_NO_PICTURES, g_hInstance, strDirectory.String() );

                if( !pImage->CreateFromText( strMessage, &rcScreen, m_Settings.GetMaxScreenPercent() ) )
                {
                    delete pImage;
                }
                else
                {
                    m_pTransition = new CCutTransition( pImage, dc, &rcScreen, &m_rcWindow );
                    if( m_pTransition == NULL )
                        delete pImage;
                }
            }
        }
    }
    else
    {
        for( int i = 0; m_pTransition == NULL && i < m_Images.Count(); i++ )
        {
            bool bFound = bForward ? m_Images.GetNext( strFile ) : m_Images.GetPrevious( strFile );
            if( !bFound )
                continue;

            CScopedDC dc;
            if( !dc.GetDC( NULL ) )
                continue;

            CSlideImage* pImage = new CSlideImage;
            if( pImage == NULL )
                continue;

            RECT rcScreen = RandomScreen();
            if( !pImage->Load( dc, strFile, &rcScreen, m_Settings.GetMaxScreenPercent(),
                               m_Settings.GetAllowStretching(), m_Settings.GetDisplayFilename() ) )
            {
                delete pImage;
                continue;
            }

            if( !m_Settings.GetDisableTransitions() && !bImmediate )
                m_pTransition = CreateTransition( pImage, dc, &rcScreen, &m_rcWindow );
            else
                m_pTransition = new CCutTransition( pImage, dc, &rcScreen, &m_rcWindow );

            if( m_pTransition == NULL )
                delete pImage;
        }
    }

    return m_pTransition != NULL;
}




//-----------------------------------------------------------------------------
// Name: CSlideshow::Paint(), Repaint()
// Desc: The next frame of the transition (true: the picture is complete);
//       the whole picture again
//-----------------------------------------------------------------------------
bool CSlideshow::Paint( CScopedDC& dc )
{
    if( m_pTransition != NULL && dc.IsValid() )
        return m_pTransition->Paint( dc );
    return false;
}

void CSlideshow::Repaint( CScopedDC& dc )
{
    if( m_pTransition != NULL && dc.IsValid() )
        m_pTransition->Repaint( dc );
}
