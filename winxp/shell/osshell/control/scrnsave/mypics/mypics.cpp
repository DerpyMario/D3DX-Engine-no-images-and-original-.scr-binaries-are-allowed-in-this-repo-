//-----------------------------------------------------------------------------
// File: mypics.cpp
//
// Desc: The screen saver window of the My Pictures Slideshow
//-----------------------------------------------------------------------------
#include <windows.h>
#include "mypics.h"




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::CMyPicturesSaver()
// Desc:
//-----------------------------------------------------------------------------
CMyPicturesSaver::CMyPicturesSaver( HWND hWnd, UINT uFoundFileMsg, UINT nPaintTimer,
                                    UINT nChangeTimer, UINT nStartTimer, LPCWSTR pszRegistryKey,
                                    HINSTANCE hInstance )
    : m_pSlideshow(NULL), m_hInstance(hInstance), m_strRegistryKey(pszRegistryKey)
{
    m_nChangeTimer = nChangeTimer;
    m_hWnd = hWnd;
    m_nPaintTimer = nPaintTimer;
    m_nStartTimeout = 5000;
    m_nStartTimer = nStartTimer;
    m_uFoundFileMsg = uFoundFileMsg;
    m_bPaused = false;
    m_hSearchThread = NULL;
    m_hStopEvent = NULL;
    m_bStarted = false;
    m_nStartAfter = 0;
    m_nShuffleEvery = 50;

    m_nSeed = GetTickCount();
    m_nStartAfter = NextRandom( &m_nSeed ) % 20;
}




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::~CMyPicturesSaver()
// Desc: Stops the search and waits for its thread
//-----------------------------------------------------------------------------
CMyPicturesSaver::~CMyPicturesSaver()
{
    delete m_pSlideshow;
    m_pSlideshow = NULL;

    if( m_hStopEvent != NULL )
    {
        SetEvent( m_hStopEvent );
        CloseHandle( m_hStopEvent );
        m_hStopEvent = NULL;
    }

    if( m_hSearchThread != NULL )
    {
        WaitForSingleObject( m_hSearchThread, INFINITE );
        CloseHandle( m_hSearchThread );
        m_hSearchThread = NULL;
    }

    m_Timer.Kill();
}




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::Initialize()
// Desc: Creates the slide show and starts the search
//-----------------------------------------------------------------------------
void CMyPicturesSaver::Initialize()
{
    HCURSOR hOldCursor = SetCursor( LoadCursorW( NULL, IDC_WAIT ) );

    HDC hdc = GetDC( m_hWnd );
    if( hdc != NULL )
    {
        m_pSlideshow = new CSlideshow( m_hInstance, m_strRegistryKey );
        if( m_pSlideshow != NULL )
        {
            m_pSlideshow->SetWindow( m_hWnd );
            if( m_pSlideshow->IsValid() )
            {
                m_hStopEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
                if( m_hStopEvent != NULL )
                    m_hSearchThread = m_pSlideshow->StartSearch( m_hWnd, m_uFoundFileMsg, m_hStopEvent );

                SetTimer( m_hWnd, m_nStartTimer, m_nStartTimeout, NULL );

                ReleaseDC( m_hWnd, hdc );
                SetCursor( hOldCursor );
                return;
            }

            delete m_pSlideshow;
            m_pSlideshow = NULL;
        }
        ReleaseDC( m_hWnd, hdc );
    }

    SetCursor( hOldCursor );
}




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::OnTimer()
// Desc:
//-----------------------------------------------------------------------------
void CMyPicturesSaver::OnTimer( UINT_PTR nTimer )
{
    if( nTimer == m_nPaintTimer )
    {
        // the next frame of the transition; at its end, wait for the next
        // picture
        if( m_pSlideshow != NULL )
        {
            CScopedDC dc;
            if( dc.GetDC( m_hWnd ) && m_pSlideshow->Paint( dc ) )
                m_Timer.Set( m_hWnd, m_nChangeTimer, m_pSlideshow->GetChangeInterval() );
        }
    }
    else if( nTimer == m_nChangeTimer )
    {
        m_Timer.Kill();
        if( !m_bPaused && m_pSlideshow != NULL && m_pSlideshow->ChangeImage( true, false ) )
        {
            m_Timer.Set( m_hWnd, m_nPaintTimer, m_pSlideshow->GetPaintInterval() );
            if( !m_bStarted )
                m_bStarted = true;
        }
    }
    else if( nTimer == m_nStartTimer )
    {
        // whatever was found so far, or the message that there is nothing
        KillTimer( m_hWnd, m_nStartTimer );
        if( !m_bStarted )
        {
            m_pSlideshow->Images().Restart();
            SendMessageW( m_hWnd, WM_TIMER, m_nChangeTimer, 0 );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::OnPaint()
// Desc:
//-----------------------------------------------------------------------------
void CMyPicturesSaver::OnPaint()
{
    if( m_pSlideshow != NULL )
    {
        CScopedDC dc;
        if( dc.BeginPaint( m_hWnd ) )
            m_pSlideshow->Repaint( dc );
    }
}




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::OnKey()
// Desc: Left: the previous picture; right: the next one (both at once,
//       without a transition); down: pause / go on.  Returns true for these
//       keys, which then do not end the screen saver.
//-----------------------------------------------------------------------------
bool CMyPicturesSaver::OnKey( UINT uMsg, WPARAM wParam )
{
    bool bForward, bImmediate;

    if( m_pSlideshow == NULL || !m_pSlideshow->GetAllowKeyboardControl() || uMsg != WM_KEYDOWN )
        return false;

    switch( wParam )
    {
    case VK_LEFT:
        bForward = false;
        bImmediate = true;
        break;

    case VK_RIGHT:
        bForward = true;
        bImmediate = true;
        break;

    case VK_DOWN:
        m_bPaused = !m_bPaused;
        if( m_bPaused )
            return true;
        bForward = true;
        bImmediate = false;
        break;

    default:
        return false;
    }

    if( m_pSlideshow->ChangeImage( bForward, bImmediate ) )
        m_Timer.Set( m_hWnd, m_nPaintTimer, m_pSlideshow->GetPaintInterval() );

    return true;
}




//-----------------------------------------------------------------------------
// Name: CMyPicturesSaver::OnFileFound()
// Desc: A picture the search found (pstrFile, which is deleted here), or
//       the end of the search (NULL)
//-----------------------------------------------------------------------------
void CMyPicturesSaver::OnFileFound( CStr* pstrFile )
{
    if( m_pSlideshow == NULL )
        return;

    if( pstrFile == NULL )
    {
        m_pSlideshow->Images().Restart();
        if( !m_bStarted )
            SendMessageW( m_hWnd, WM_TIMER, m_nChangeTimer, 0 );
        return;
    }

    CStr strFile( *pstrFile );
    if( !m_pSlideshow->Images().Add( strFile ) )
        SetEvent( m_hStopEvent );

    // enough to start with
    if( !m_bStarted )
    {
        int nCount = m_pSlideshow->Images().Count();
        if( nCount != 0 && m_nStartAfter + 1 == nCount )
        {
            m_pSlideshow->Images().Restart();
            SendMessageW( m_hWnd, WM_TIMER, m_nChangeTimer, 0 );
        }
    }

    // mix the new pictures in now and then
    int nCount = m_pSlideshow->Images().Count();
    if( nCount != 0 && nCount % m_nShuffleEvery == 0 )
        m_pSlideshow->Images().Restart();

    delete pstrFile;
}
