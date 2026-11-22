//-----------------------------------------------------------------------------
// File: transitions.cpp
//
// Desc: The transitions between the pictures of the My Pictures Slideshow
//       screen saver
//-----------------------------------------------------------------------------
#include <windows.h>
#include <stdlib.h>
#include "transitions.h"




//-----------------------------------------------------------------------------
// Name: CTransition::CTransition()
// Desc: Puts the picture at a random place on its screen
//-----------------------------------------------------------------------------
CTransition::CTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen,
                          const RECT* prcWindow )
{
    m_pImage = pImage;
    m_bFirstPaint = true;
    m_dwStartTime = 0;
    m_bUnused = false;
    m_nSeed = GetTickCount();
    m_rcWindow = *prcWindow;
    m_rcScreen = *prcScreen;
    m_dwDuration = 0;

    if( m_pImage != NULL )
    {
        SIZE size = m_pImage->GetSize();

        int x = (prcScreen->right - prcScreen->left) - size.cx;
        if( x != 0 )
            x = NextRandom( &m_nSeed ) % x;

        int y = (prcScreen->bottom - prcScreen->top) - size.cy;
        if( y != 0 )
            y = NextRandom( &m_nSeed ) % y;

        m_rcImage.left = prcScreen->left + x;
        m_rcImage.top = prcScreen->top + y;
        m_rcImage.right = m_rcImage.left + size.cx;
        m_rcImage.bottom = m_rcImage.top + size.cy;
    }
}

CTransition::~CTransition()
{
    delete m_pImage;
    m_pImage = NULL;
}




//-----------------------------------------------------------------------------
// Name: CTransition::IsValid()
// Desc:
//-----------------------------------------------------------------------------
bool CTransition::IsValid()
{
    return m_pImage != NULL && m_pImage->GetBitmap() != NULL;
}




//-----------------------------------------------------------------------------
// Name: CTransition::Paint()
// Desc: The next frame.  The first one clears the window.  Returns true when
//       there is nothing more to draw.
//-----------------------------------------------------------------------------
bool CTransition::Paint( CScopedDC& dc )
{
    if( m_bFirstPaint )
    {
        m_dwStartTime = GetTickCount();
        FillRect( dc, &m_rcWindow, (HBRUSH)GetStockObject( BLACK_BRUSH ) );
    }

    if( !m_bFirstPaint && !IsAnimating() )
        return true;

    if( m_pImage != NULL && dc.IsValid() )
    {
        SelectAndRealizePalette( dc, m_pImage->GetPalette(), FALSE );

        CScopedDC dcImage;
        if( dcImage.CreateCompatibleDC( dc ) )
        {
            SelectAndRealizePalette( dc, m_pImage->GetPalette(), FALSE );
            DrawFrame( dc, dcImage );
        }
    }

    if( m_bFirstPaint )
        m_bFirstPaint = false;

    return false;
}




//-----------------------------------------------------------------------------
// Name: CTransition::Repaint()
// Desc:
//-----------------------------------------------------------------------------
void CTransition::Repaint( CScopedDC& dc )
{
    if( dc.IsValid() && m_pImage != NULL )
    {
        SelectAndRealizePalette( dc, m_pImage->GetPalette(), FALSE );

        CScopedDC dcImage;
        if( dcImage.CreateCompatibleDC( dc ) )
        {
            SelectAndRealizePalette( dc, m_pImage->GetPalette(), FALSE );
            DrawComplete( dc, dcImage );
        }
    }
}

void CTransition::DrawComplete( CScopedDC& dc, CScopedDC& dcImage )
{
    SelectObject( dcImage, m_pImage->GetBitmap() );
    BitBlt( dc, m_rcImage.left, m_rcImage.top, m_rcImage.right - m_rcImage.left,
            m_rcImage.bottom - m_rcImage.top, dcImage, 0, 0, SRCCOPY );
}




//-----------------------------------------------------------------------------
// Name: CCutTransition::DrawFrame()
// Desc:
//-----------------------------------------------------------------------------
void CCutTransition::DrawFrame( CScopedDC& dc, CScopedDC& dcImage )
{
    SelectObject( dcImage, m_pImage->GetBitmap() );
    SIZE size = m_pImage->GetSize();
    BitBlt( dc, m_rcImage.left, m_rcImage.top, size.cx, size.cy, dcImage, 0, 0, SRCCOPY );
}




//-----------------------------------------------------------------------------
// Name: CSlideTransition::CSlideTransition()
// Desc: Starts just outside the window, in one of eight directions: from
//       the corners, or straight from the sides (in line with the picture)
//-----------------------------------------------------------------------------
CSlideTransition::CSlideTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen,
                                    const RECT* prcWindow )
    : CTransition( pImage, dc, prcScreen, prcWindow )
{
    m_dwDuration = 5000;
    SetRectEmpty( &m_rcLast );

    SIZE size = m_pImage->GetSize();
    UINT nSeed = GetTickCount();

    switch( NextRandom( &nSeed ) & 7 )
    {
    case 0:     // top left
        SetRect( &m_rcStart, prcWindow->left - size.cx, prcWindow->top - size.cy,
                 prcWindow->left, prcWindow->top );
        break;
    case 1:     // top
        SetRect( &m_rcStart, m_rcImage.left, prcWindow->top - size.cy,
                 m_rcImage.right, prcWindow->top );
        break;
    case 2:     // top right
        SetRect( &m_rcStart, prcWindow->right, prcWindow->top - size.cy,
                 prcWindow->right + size.cx, prcWindow->top );
        break;
    case 3:     // right
        SetRect( &m_rcStart, prcWindow->right, m_rcImage.top,
                 prcWindow->right + size.cx, m_rcImage.bottom );
        break;
    case 4:     // bottom right
        SetRect( &m_rcStart, prcWindow->right, prcWindow->bottom,
                 prcWindow->right + size.cx, prcWindow->bottom + size.cy );
        break;
    case 5:     // bottom
        SetRect( &m_rcStart, m_rcImage.left, prcWindow->bottom,
                 m_rcImage.right, prcWindow->bottom + size.cy );
        break;
    case 6:     // bottom left
        SetRect( &m_rcStart, prcWindow->left - size.cx, prcWindow->bottom,
                 prcWindow->left, prcWindow->bottom + size.cy );
        break;
    case 7:     // left
        SetRect( &m_rcStart, prcWindow->left - size.cx, m_rcImage.top,
                 prcWindow->left, m_rcImage.bottom );
        break;
    }
}

bool CSlideTransition::IsAnimating()
{
    return m_rcLast.left != m_rcImage.left || m_rcLast.top != m_rcImage.top ||
           m_rcLast.right != m_rcImage.right || m_rcLast.bottom != m_rcImage.bottom;
}

void CSlideTransition::DrawFrame( CScopedDC& dc, CScopedDC& dcImage )
{
    SelectObject( dcImage, m_pImage->GetBitmap() );
    SIZE size = m_pImage->GetSize();

    DWORD dwElapsed = ElapsedTime();

    int cxTotal = m_rcImage.left - m_rcStart.left;
    int cyTotal = m_rcImage.top - m_rcStart.top;
    int dx = MulDiv( cxTotal, dwElapsed, m_dwDuration );
    int dy = MulDiv( cyTotal, dwElapsed, m_dwDuration );
    if( abs( dx ) > abs( cxTotal ) )
        dx = cxTotal;
    if( abs( dy ) > abs( cyTotal ) )
        dy = cyTotal;

    RECT rc = m_rcStart;
    NormalizeRect( &rc );
    OffsetRect( &rc, dx, dy );

    // black where the picture was, and the picture where it is now
    FillUncovered( dc, &m_rcLast, &rc, (HBRUSH)GetStockObject( BLACK_BRUSH ) );
    BitBlt( dc, rc.left, rc.top, size.cx, size.cy, dcImage, 0, 0, SRCCOPY );

    m_rcLast = rc;
}




//-----------------------------------------------------------------------------
// Name: CBlockTransition::CBlockTransition()
// Desc: Shuffles the blocks
//-----------------------------------------------------------------------------
CBlockTransition::CBlockTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen,
                                    const RECT* prcWindow )
    : CTransition( pImage, dc, prcScreen, prcWindow )
{
    m_nBlockSize = 10;
    m_dwDuration = 2000;
    m_pnOrder = NULL;
    m_nBlocksDrawn = 0;

    SIZE size = m_pImage->GetSize();
    m_nColumns = RoundUp( size.cx, m_nBlockSize ) / m_nBlockSize;
    m_nRows = RoundUp( size.cy, m_nBlockSize ) / m_nBlockSize;

    int nBlocks = m_nColumns * m_nRows;
    m_pnOrder = (nBlocks != 0) ? new int[nBlocks] : NULL;
    if( m_pnOrder != NULL )
    {
        for( int i = 0; i < nBlocks; i++ )
            m_pnOrder[i] = i;

        for( int i = 0; i < nBlocks; i++ )
        {
            int j = i;
            if( nBlocks - i != 0 )
                j = NextRandom( &m_nSeed ) % (nBlocks - i) + i;

            int n = m_pnOrder[i];
            m_pnOrder[i] = m_pnOrder[j];
            m_pnOrder[j] = n;
        }
    }
}

CBlockTransition::~CBlockTransition()
{
    delete[] m_pnOrder;
    m_pnOrder = NULL;
}

bool CBlockTransition::IsAnimating()
{
    return m_nBlocksDrawn < m_nColumns * m_nRows;
}

bool CBlockTransition::IsValid()
{
    return m_pImage != NULL && m_pImage->GetBitmap() != NULL && m_pnOrder != NULL;
}

void CBlockTransition::DrawFrame( CScopedDC& dc, CScopedDC& dcImage )
{
    if( m_pnOrder == NULL )
        return;

    SelectObject( dcImage, m_pImage->GetBitmap() );

    DWORD dwElapsed = ElapsedTime();
    int nBlocks = (int)((UINT)(m_nColumns * m_nRows) * dwElapsed / m_dwDuration);

    for( int i = m_nBlocksDrawn; i < nBlocks && i < m_nColumns * m_nRows; i++ )
    {
        int x = m_nBlockSize * (m_pnOrder[i] % m_nColumns);
        int y = m_nBlockSize * (m_pnOrder[i] / m_nColumns);
        BitBlt( dc, m_rcImage.left + x, m_rcImage.top + y, m_nBlockSize, m_nBlockSize,
                dcImage, x, y, SRCCOPY );
    }

    m_nBlocksDrawn = nBlocks;
}




//-----------------------------------------------------------------------------
// Name: CFadeTransition::CFadeTransition()
// Desc:
//-----------------------------------------------------------------------------
CFadeTransition::CFadeTransition( CSlideImage* pImage, CScopedDC& dc, const RECT* prcScreen,
                                  const RECT* prcWindow )
    : CTransition( pImage, dc, prcScreen, prcWindow ), m_hbmWork(NULL)
{
    m_dwDuration = 6000;

    if( m_pImage != NULL )
    {
        m_bf.BlendOp = AC_SRC_OVER;
        m_bf.BlendFlags = 0;
        m_bf.SourceConstantAlpha = 0;
        m_bf.AlphaFormat = 0;

        SIZE size = m_pImage->GetSize();
        m_hbmWork = CreateCompatibleBitmap( dc, size.cx, size.cy );
        m_dcWork.CreateCompatibleDC( dc );
    }
}

CFadeTransition::~CFadeTransition()
{
    if( m_hbmWork != NULL )
        DeleteObject( m_hbmWork );
    m_dcWork.Release();
}

bool CFadeTransition::IsAnimating()
{
    return m_bf.SourceConstantAlpha < 255;
}

bool CFadeTransition::IsValid()
{
    return m_pImage != NULL && m_pImage->GetBitmap() != NULL && m_hbmWork != NULL &&
           m_dcWork.IsValid();
}

void CFadeTransition::DrawFrame( CScopedDC& dc, CScopedDC& dcImage )
{
    SelectAndRealizePalette( m_dcWork, m_pImage->GetPalette(), FALSE );
    SelectObject( dcImage, m_pImage->GetBitmap() );
    HGDIOBJ hbmOld = SelectObject( m_dcWork, m_hbmWork );

    UINT nAlpha = ElapsedTime() * 255 / m_dwDuration;
    m_bf.SourceConstantAlpha = (BYTE)((nAlpha > 255) ? 255 : nAlpha);

    // the picture, that much opaque, over black
    SIZE size = m_pImage->GetSize();
    RECT rc = { 0, 0, size.cx, size.cy };
    FillRect( m_dcWork, &rc, (HBRUSH)GetStockObject( BLACK_BRUSH ) );

    int cx = m_rcImage.right - m_rcImage.left;
    int cy = m_rcImage.bottom - m_rcImage.top;
    AlphaBlend( m_dcWork, 0, 0, cx, cy, dcImage, 0, 0, cx, cy, m_bf );
    BitBlt( dc, m_rcImage.left, m_rcImage.top, cx, cy, m_dcWork, 0, 0, SRCCOPY );

    SelectObject( m_dcWork, hbmOld );
}




//-----------------------------------------------------------------------------
// Name: CCurtainTransition::CCurtainTransition()
// Desc:
//-----------------------------------------------------------------------------
CCurtainTransition::CCurtainTransition( CSlideImage* pImage, CScopedDC& dc,
                                        const RECT* prcScreen, const RECT* prcWindow )
    : CTransition( pImage, dc, prcScreen, prcWindow )
{
    m_nWidthDrawn = 0;
    m_dwDuration = 3000;

    SIZE size = m_pImage->GetSize();
    m_nHalfWidth = RoundUp( size.cx, 2 ) / 2;
}

bool CCurtainTransition::IsAnimating()
{
    return m_nWidthDrawn < m_nHalfWidth;
}

void CCurtainTransition::DrawFrame( CScopedDC& dc, CScopedDC& dcImage )
{
    m_nWidthDrawn = (int)((LONGLONG)m_nHalfWidth * (int)ElapsedTime() / (int)m_dwDuration);

    SelectObject( dcImage, m_pImage->GetBitmap() );

    int cy = m_rcImage.bottom - m_rcImage.top;
    BitBlt( dc, m_rcImage.left, m_rcImage.top, m_nWidthDrawn, cy, dcImage, 0, 0, SRCCOPY );

    int x = m_rcImage.right - m_nWidthDrawn;
    BitBlt( dc, x, m_rcImage.top, m_nWidthDrawn, cy, dcImage, x - m_rcImage.left, 0, SRCCOPY );
}
