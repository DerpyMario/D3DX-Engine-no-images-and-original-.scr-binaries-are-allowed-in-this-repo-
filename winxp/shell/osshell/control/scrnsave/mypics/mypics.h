//-----------------------------------------------------------------------------
// File: mypics.h
//
// Desc: The screen saver window of the My Pictures Slideshow: its timers,
//       the search thread, and the keyboard.  (0x178 bytes)
//
//       Three timers: the start timer gives the search 5 s to find
//       pictures (the show starts earlier once a random 1 to 20 pictures are
//       found); the change timer shows the next picture; the paint timer
//       draws the transition, until the picture is complete.
//-----------------------------------------------------------------------------
#ifndef _MYPICS_MYPICS_H
#define _MYPICS_MYPICS_H

#include "util.h"
#include "slideshow.h"

class CMyPicturesSaver
{
public:
    CMyPicturesSaver( HWND hWnd, UINT uFoundFileMsg, UINT nPaintTimer, UINT nChangeTimer,
                      UINT nStartTimer, LPCWSTR pszRegistryKey, HINSTANCE hInstance );
    ~CMyPicturesSaver();

    void    Initialize();
    void    OnTimer( UINT_PTR nTimer );
    void    OnPaint();
    bool    OnKey( UINT uMsg, WPARAM wParam );
    void    OnFileFound( CStr* pstrFile );

private:
    CSlideshow*     m_pSlideshow;           // +0x000
    HINSTANCE       m_hInstance;            // +0x008
    CStr            m_strRegistryKey;       // +0x010
    HWND            m_hWnd;                 // +0x128
    UINT            m_nPaintTimer;          // +0x130
    UINT            m_nChangeTimer;         // +0x134
    UINT            m_nStartTimer;          // +0x138
    UINT            m_nStartTimeout;        // +0x13c ms
    UINT            m_uFoundFileMsg;        // +0x140
    bool            m_bPaused;              // +0x144
    CWindowTimer    m_Timer;                // +0x148 the paint or the change timer
    HANDLE          m_hSearchThread;        // +0x158
    HANDLE          m_hStopEvent;           // +0x160
    bool            m_bStarted;             // +0x168 a picture was shown
    int             m_nStartAfter;          // +0x16c pictures to find before showing
    int             m_nShuffleEvery;        // +0x170 pictures found
    UINT            m_nSeed;                // +0x174
};

#endif // _MYPICS_MYPICS_H
