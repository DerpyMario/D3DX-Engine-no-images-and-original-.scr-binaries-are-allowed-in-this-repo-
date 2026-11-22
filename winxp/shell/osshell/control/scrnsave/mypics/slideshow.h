//-----------------------------------------------------------------------------
// File: slideshow.h
//
// Desc: The slide show of the My Pictures Slideshow screen saver: the list
//       of pictures found so far, the screens (monitors) of the window, the
//       settings, and the transition that shows the current picture.
//-----------------------------------------------------------------------------
#ifndef _MYPICS_SLIDESHOW_H
#define _MYPICS_SLIDESHOW_H

#include "util.h"
#include "settings.h"
#include "gdiphelp.h"
#include "slideimage.h"
#include "transitions.h"


//-----------------------------------------------------------------------------
// Name: class CImageList
// Desc: The picture files, in random order.  (0x30 bytes)
//-----------------------------------------------------------------------------
class CImageList
{
public:
    CImageList() : m_Files(10), m_nSeed(GetTickCount()), m_nCurrent(0) {}
    virtual ~CImageList() {}

    bool    Add( LPCWSTR pszFile );
    void    Shuffle();
    void    Restart()                       { m_nCurrent = 0; Shuffle(); }
    bool    GetNext( CStr& strFile );
    bool    GetPrevious( CStr& strFile );
    int     Count() const                   { return m_Files.Size(); }

private:
    CDynamicArray<CStr> m_Files;            // +0x08
    UINT                m_nSeed;            // +0x28
    int                 m_nCurrent;         // +0x2c the next picture
};


//-----------------------------------------------------------------------------
// Name: class CSlideshow
// Desc: (0x368 bytes)
//-----------------------------------------------------------------------------
class CSlideshow
{
public:
    CSlideshow( HINSTANCE hInstance, const CStr& strRegistryKey );
    ~CSlideshow();

    bool    IsValid() const                 { return true; }
    void    SetWindow( HWND hWnd );
    HANDLE  StartSearch( HWND hWnd, UINT uMsg, HANDLE hStopEvent );

    CImageList& Images()                    { return m_Images; }
    bool    ChangeImage( bool bForward, bool bImmediate );
    bool    Paint( CScopedDC& dc );
    void    Repaint( CScopedDC& dc );

    UINT    GetChangeInterval() const       { return m_Settings.GetChangeInterval(); }
    UINT    GetPaintInterval() const        { return m_Settings.GetPaintInterval(); }
    bool    GetAllowKeyboardControl() const { return m_Settings.GetAllowKeyboardControl(); }

private:
    RECT        RandomScreen();
    CTransition* CreateTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen,
                                   const RECT* prcWindow );
    static BOOL CALLBACK MonitorEnumProc( HMONITOR hMonitor, HDC hdc, LPRECT prc, LPARAM lParam );

    CImageList          m_Images;           // +0x000
    CTransition*        m_pTransition;      // +0x030
    HINSTANCE           m_hInstance;        // +0x038
    RECT                m_rcWindow;         // +0x040 client area
    CSlideshowSettings  m_Settings;         // +0x050
    CDynamicArray<RECT> m_Monitors;         // +0x300 screen coordinates
    CDynamicArray<RECT> m_Screens;          // +0x320 their parts of the window
    CGdiplusHelper      m_Gdiplus;          // +0x340
};

#endif // _MYPICS_SLIDESHOW_H
