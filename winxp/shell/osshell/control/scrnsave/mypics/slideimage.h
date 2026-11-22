//-----------------------------------------------------------------------------
// File: slideimage.h
//
// Desc: A picture of the slide show, as a bitmap of the size it is shown at
//       (with the palette to show it with on palette displays).  (0x18 bytes)
//-----------------------------------------------------------------------------
#ifndef _MYPICS_SLIDEIMAGE_H
#define _MYPICS_SLIDEIMAGE_H

#include "util.h"

class CSlideImage
{
public:
    CSlideImage() : m_hBitmap(NULL), m_hPalette(NULL) {}
    virtual ~CSlideImage();

    bool        Load( CScopedDC& dc, LPCWSTR pszFile, const RECT* prcScreen, int nMaxPercent,
                      bool bStretch, bool bShowFileName );
    bool        CreateFromText( LPCWSTR pszText, const RECT* prcScreen, int nMaxPercent );

    HBITMAP     GetBitmap() const           { return m_hBitmap; }
    HPALETTE    GetPalette() const          { return m_hPalette; }
    SIZE        GetSize() const;

private:
    HPALETTE    CreatePaletteFor( CScopedDC& dc, HBITMAP hBitmap );

    HBITMAP     m_hBitmap;                  // +0x08
    HPALETTE    m_hPalette;                 // +0x10
};

#endif // _MYPICS_SLIDEIMAGE_H
