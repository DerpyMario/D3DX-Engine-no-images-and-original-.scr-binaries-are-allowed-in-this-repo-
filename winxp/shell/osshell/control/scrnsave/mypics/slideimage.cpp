//-----------------------------------------------------------------------------
// File: slideimage.cpp
//
// Desc: The pictures of the My Pictures Slideshow screen saver: a picture
//       file scaled to the screen (with its name on it, if wanted), or a
//       message in white on black
//-----------------------------------------------------------------------------
#include <windows.h>
#include "slideimage.h"
#include "gdiphelp.h"




//-----------------------------------------------------------------------------
// Name: CSlideImage::~CSlideImage()
// Desc:
//-----------------------------------------------------------------------------
CSlideImage::~CSlideImage()
{
    if( m_hBitmap != NULL )
        DeleteObject( m_hBitmap );
    m_hBitmap = NULL;

    if( m_hPalette != NULL )
        DeleteObject( m_hPalette );
    m_hPalette = NULL;
}




//-----------------------------------------------------------------------------
// Name: CSlideImage::GetSize()
// Desc:
//-----------------------------------------------------------------------------
SIZE CSlideImage::GetSize() const
{
    SIZE size = { 0, 0 };
    BITMAP bm;

    if( m_hBitmap != NULL && GetObjectW( m_hBitmap, sizeof(BITMAP), &bm ) != 0 )
    {
        size.cx = bm.bmWidth;
        size.cy = bm.bmHeight;
    }
    return size;
}




//-----------------------------------------------------------------------------
// Name: CSlideImage::CreatePaletteFor()
// Desc: Only for palette displays: the colour table of a bitmap of up to 256
//       colours, else the halftone palette
//-----------------------------------------------------------------------------
HPALETTE CSlideImage::CreatePaletteFor( CScopedDC& dc, HBITMAP hBitmap )
{
    if( !(GetDeviceCaps( dc, RASTERCAPS ) & RC_PALETTE) )
        return NULL;

    if( hBitmap == NULL )
        return NULL;

    DIBSECTION ds;
    ZeroMemory( &ds, sizeof(ds) );
    GetObjectW( hBitmap, sizeof(DIBSECTION), &ds );

    if( ds.dsBmih.biBitCount != 32 )
    {
        int nColors = 1 << ds.dsBmih.biBitCount;
        if( nColors <= 256 )
        {
            HPALETTE hPalette = NULL;
            RGBQUAD* prgb = (RGBQUAD*)LocalAlloc( LPTR, nColors * sizeof(RGBQUAD) );
            if( prgb != NULL )
            {
                CScopedDC dcMem;
                if( dcMem.CreateCompatibleDC( dc ) )
                {
                    SelectObject( dcMem, hBitmap );
                    GetDIBColorTable( dcMem, 0, nColors, prgb );

                    LOGPALETTE* plp = (LOGPALETTE*)LocalAlloc( LPTR, sizeof(LOGPALETTE) +
                                                               (nColors - 1) * sizeof(PALETTEENTRY) );
                    if( plp != NULL )
                    {
                        plp->palVersion = 0x300;
                        plp->palNumEntries = (WORD)nColors;
                        for( int i = 0; i < nColors; i++ )
                        {
                            plp->palPalEntry[i].peRed = prgb[i].rgbRed;
                            plp->palPalEntry[i].peGreen = prgb[i].rgbGreen;
                            plp->palPalEntry[i].peBlue = prgb[i].rgbBlue;
                            plp->palPalEntry[i].peFlags = 0;
                        }
                        hPalette = CreatePalette( plp );
                        LocalFree( plp );
                    }
                }
                LocalFree( prgb );
            }
            return hPalette;
        }
    }

    return CreateHalftonePalette( dc );
}




//-----------------------------------------------------------------------------
// Name: CSlideImage::Load()
// Desc: The picture, scaled to fit nMaxPercent of the screen, with its file
//       name in the top left corner on a half-transparent white label
//-----------------------------------------------------------------------------
bool CSlideImage::Load( CScopedDC& dc, LPCWSTR pszFile, const RECT* prcScreen, int nMaxPercent,
                        bool bStretch, bool bShowFileName )
{
    // (The old bitmap handle is not cleared: without a file name the
    // deleted handle stays.  Each picture gets a new CSlideImage, so this
    // never matters.)
    if( m_hBitmap != NULL )
        DeleteObject( m_hBitmap );
    if( m_hPalette != NULL )
        DeleteObject( m_hPalette );
    m_hPalette = NULL;

    if( pszFile != NULL && lstrlenW( pszFile ) != 0 )
    {
        CGdiplusHelper gdiplus;

        int cxMax = (prcScreen->right - prcScreen->left) * nMaxPercent / 100;
        int cyMax = (prcScreen->bottom - prcScreen->top) * nMaxPercent / 100;

        HRESULT hr = gdiplus.LoadScaledBitmap( &m_hBitmap, pszFile, cxMax, cyMax, bStretch );
        if( SUCCEEDED( hr ) && m_hBitmap != NULL )
        {
            int cx = 0, cy = 0;
            BITMAP bm;
            ZeroMemory( &bm, sizeof(bm) );
            if( GetObjectW( m_hBitmap, sizeof(BITMAP), &bm ) != 0 )
            {
                cx = bm.bmWidth;
                cy = bm.bmHeight;
            }

            m_hPalette = CreatePaletteFor( dc, m_hBitmap );

            if( bShowFileName && *pszFile != L'\0' )
            {
                CScopedDC dcImage;
                if( dcImage.CreateCompatibleDC( dc ) )
                {
                    SelectAndRealizePalette( dcImage, m_hPalette, FALSE );
                    SelectObject( dcImage, m_hBitmap );
                    SetBkMode( dcImage, TRANSPARENT );

                    CScopedDC dcLabel;
                    if( dcLabel.CreateCompatibleDC( dc ) )
                    {
                        SelectAndRealizePalette( dcLabel, m_hPalette, FALSE );
                        SelectObject( dcLabel, GetStockObject( DEFAULT_GUI_FONT ) );
                        SetBkMode( dcLabel, TRANSPARENT );

                        // the size of the label
                        RECT rc = { 0, 0, cx, cy };
                        InflateRect( &rc, -2, -2 );
                        DrawTextW( dcLabel, pszFile, lstrlenW( pszFile ), &rc,
                                   DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS );
                        InflateRect( &rc, 2, 2 );
                        if( rc.right > cx )
                            rc.right = cx;
                        if( rc.bottom > cy )
                            rc.bottom = cy;

                        BITMAPINFO bmi;
                        ZeroMemory( &bmi, sizeof(bmi) );
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = rc.right - rc.left;
                        bmi.bmiHeader.biHeight = rc.bottom - rc.top;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 24;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        void* pvBits;
                        HBITMAP hbmLabel = CreateDIBSection( dc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0 );
                        if( hbmLabel != NULL )
                        {
                            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 0x80, 0 };
                            HGDIOBJ hbmOld = SelectObject( dcLabel, hbmLabel );

                            // white, with the picture half showing through
                            FillRect( dcLabel, &rc, (HBRUSH)GetStockObject( WHITE_BRUSH ) );
                            AlphaBlend( dcLabel, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                                        dcImage, rc.left, rc.top, rc.right - rc.left,
                                        rc.bottom - rc.top, bf );

                            InflateRect( &rc, -2, -2 );
                            DrawTextW( dcLabel, pszFile, lstrlenW( pszFile ), &rc,
                                       DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS );
                            InflateRect( &rc, 2, 2 );

                            BitBlt( dcImage, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                                    dcLabel, 0, 0, SRCCOPY );

                            DeleteObject( SelectObject( dcLabel, hbmOld ) );
                        }
                    }
                }
            }
        }
    }

    return m_hBitmap != NULL;
}




//-----------------------------------------------------------------------------
// Name: CSlideImage::CreateFromText()
// Desc: White text on black, word-wrapped to nMaxPercent of the screen
//       (DT_RTLREADING is the flag the original passes)
//-----------------------------------------------------------------------------
bool CSlideImage::CreateFromText( LPCWSTR pszText, const RECT* prcScreen, int nMaxPercent )
{
    if( m_hBitmap != NULL )
        DeleteObject( m_hBitmap );
    m_hBitmap = NULL;
    if( m_hPalette != NULL )
        DeleteObject( m_hPalette );
    m_hPalette = NULL;

    HDC hdcScreen = GetDC( NULL );
    if( hdcScreen != NULL )
    {
        RECT rc;
        rc.left = 0;
        rc.top = 0;
        rc.right = (prcScreen->right - prcScreen->left) * nMaxPercent / 100;
        rc.bottom = (prcScreen->bottom - prcScreen->top) * nMaxPercent / 100;

        CScopedDC dc;
        if( dc.CreateCompatibleDC( hdcScreen ) )
        {
            SelectObject( dc, GetStockObject( DEFAULT_GUI_FONT ) );
            DrawTextW( dc, pszText, lstrlenW( pszText ), &rc,
                       DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_RTLREADING );

            m_hBitmap = CreateCompatibleBitmap( hdcScreen, rc.right, rc.bottom );
            if( m_hBitmap != NULL )
            {
                SetBkColor( dc, RGB(0, 0, 0) );
                SetTextColor( dc, RGB(255, 255, 255) );
                SelectObject( dc, m_hBitmap );
                DrawTextW( dc, pszText, lstrlenW( pszText ), &rc,
                           DT_WORDBREAK | DT_NOPREFIX | DT_RTLREADING );
            }
        }
        ReleaseDC( NULL, hdcScreen );
    }

    return m_hBitmap != NULL;
}
