//-----------------------------------------------------------------------------
// File: transitions.h
//
// Desc: The ways a picture of the slide show appears: at once, sliding in
//       from one of the eight directions, in random blocks, fading in from
//       black, or uncovered from both sides towards the middle.
//
//       A transition owns its picture.  It is placed at a random position
//       on its screen (rcScreen, in window coordinates); the first frame
//       clears the whole window (rcWindow).
//-----------------------------------------------------------------------------
#ifndef _MYPICS_TRANSITIONS_H
#define _MYPICS_TRANSITIONS_H

#include "util.h"
#include "slideimage.h"


//-----------------------------------------------------------------------------
// Name: class CTransition
// Desc: Shows the picture at once.  (0x50 bytes)
//-----------------------------------------------------------------------------
class CTransition
{
public:
    CTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen, const RECT* prcWindow );
    virtual ~CTransition();

    bool Paint( CScopedDC& dc );            // returns true once the picture is complete
    void Repaint( CScopedDC& dc );          // all of it again (WM_PAINT)

    virtual void DrawComplete( CScopedDC& dc, CScopedDC& dcImage );
    virtual bool IsAnimating()              { return false; }
    virtual bool IsValid();
    virtual void DrawFrame( CScopedDC& dc, CScopedDC& dcImage ) {}

protected:
    CSlideImage*    m_pImage;               // +0x08
    DWORD           m_dwStartTime;          // +0x10
    bool            m_bFirstPaint;          // +0x14
    bool            m_bUnused;              // +0x15 (cleared, never read)
    UINT            m_nSeed;                // +0x18
    RECT            m_rcWindow;             // +0x1c
    RECT            m_rcScreen;             // +0x2c
    RECT            m_rcImage;              // +0x3c
    DWORD           m_dwDuration;           // +0x4c

    DWORD ElapsedTime() const
    {
        DWORD dwElapsed = GetTickCount() - m_dwStartTime;
        return (dwElapsed > m_dwDuration) ? m_dwDuration : dwElapsed;
    }
};


//-----------------------------------------------------------------------------
// Name: class CCutTransition
// Desc: No transition: the picture at once.  (0x50 bytes)
//-----------------------------------------------------------------------------
class CCutTransition : public CTransition
{
public:
    CCutTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen, const RECT* prcWindow )
        : CTransition( pImage, dc, prcScreen, prcWindow ) {}

    virtual void DrawFrame( CScopedDC& dc, CScopedDC& dcImage );
};


//-----------------------------------------------------------------------------
// Name: class CSlideTransition
// Desc: The picture slides in from outside the window, in 5 s.  (0x70 bytes)
//-----------------------------------------------------------------------------
class CSlideTransition : public CTransition
{
public:
    CSlideTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen, const RECT* prcWindow );

    virtual bool IsAnimating();
    virtual void DrawFrame( CScopedDC& dc, CScopedDC& dcImage );

private:
    RECT    m_rcStart;                      // +0x50
    RECT    m_rcLast;                       // +0x60
};


//-----------------------------------------------------------------------------
// Name: class CBlockTransition
// Desc: The picture appears in 10x10 blocks in random order, in 2 s.
//       (0x68 bytes)
//-----------------------------------------------------------------------------
class CBlockTransition : public CTransition
{
public:
    CBlockTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen, const RECT* prcWindow );
    virtual ~CBlockTransition();

    virtual bool IsAnimating();
    virtual bool IsValid();
    virtual void DrawFrame( CScopedDC& dc, CScopedDC& dcImage );

private:
    int*    m_pnOrder;                      // +0x50 the blocks, shuffled
    int     m_nBlockSize;                   // +0x58
    int     m_nBlocksDrawn;                 // +0x5c
    int     m_nColumns;                     // +0x60
    int     m_nRows;                        // +0x64
};


//-----------------------------------------------------------------------------
// Name: class CFadeTransition
// Desc: The picture fades in from black, in 6 s.  (0xf0 bytes)
//-----------------------------------------------------------------------------
class CFadeTransition : public CTransition
{
public:
    CFadeTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen, const RECT* prcWindow );
    virtual ~CFadeTransition();

    virtual bool IsAnimating();
    virtual bool IsValid();
    virtual void DrawFrame( CScopedDC& dc, CScopedDC& dcImage );

private:
    BLENDFUNCTION   m_bf;                   // +0x50
    HBITMAP         m_hbmWork;              // +0x58 black, then the blend
    CScopedDC       m_dcWork;               // +0x60
};


//-----------------------------------------------------------------------------
// Name: class CCurtainTransition
// Desc: The picture is uncovered from its left and right edges towards the
//       middle, in 3 s.  (0x58 bytes)
//-----------------------------------------------------------------------------
class CCurtainTransition : public CTransition
{
public:
    CCurtainTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen, const RECT* prcWindow );

    virtual bool IsAnimating();
    virtual void DrawFrame( CScopedDC& dc, CScopedDC& dcImage );

private:
    int     m_nWidthDrawn;                  // +0x50 on each side
    int     m_nHalfWidth;                   // +0x54
};

#endif // _MYPICS_TRANSITIONS_H
