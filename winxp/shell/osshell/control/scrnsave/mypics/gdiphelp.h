//-----------------------------------------------------------------------------
// File: gdiphelp.h
//
// Desc: GDI+ for the My Pictures Slideshow screen saver: starting and
//       stopping it, the list of image file types it decodes, and loading a
//       picture into a bitmap of the size it is shown at.  (0x28 bytes)
//-----------------------------------------------------------------------------
#ifndef _MYPICS_GDIPHELP_H
#define _MYPICS_GDIPHELP_H

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "util.h"

HRESULT GpStatusToHResult( Gdiplus::Status status );
SIZE    ScaleToFit( int cxMax, int cyMax, int cx, int cy );

class CGdiplusHelper
{
public:
    CGdiplusHelper();
    ~CGdiplusHelper();

    void    GetImageExtensions( CStr& strExtensions ) const;
    HRESULT LoadScaledBitmap( HBITMAP* phBitmap, LPCWSTR pszFile, UINT cxMax, UINT cyMax,
                              bool bStretch );

private:
    HRESULT LoadCodecs();
    void    FreeCodecs();

    ULONG_PTR               m_nToken;           // +0x00
    Gdiplus::ImageCodecInfo* m_pEncoders;       // +0x08
    UINT                    m_nEncoders;        // +0x10
    Gdiplus::ImageCodecInfo* m_pDecoders;       // +0x18
    UINT                    m_nDecoders;        // +0x20
};

#endif // _MYPICS_GDIPHELP_H
