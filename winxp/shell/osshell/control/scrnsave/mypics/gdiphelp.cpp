//-----------------------------------------------------------------------------
// File: gdiphelp.cpp
//
// Desc: GDI+ for the My Pictures Slideshow screen saver
//-----------------------------------------------------------------------------
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "gdiphelp.h"




//-----------------------------------------------------------------------------
// Name: GpStatusToHResult()
// Desc:
//-----------------------------------------------------------------------------
HRESULT GpStatusToHResult( Gdiplus::Status status )
{
    switch( status )
    {
    case Gdiplus::Ok:                   return S_OK;
    case Gdiplus::InvalidParameter:     return E_INVALIDARG;
    case Gdiplus::OutOfMemory:          return E_OUTOFMEMORY;
    case Gdiplus::ObjectBusy:           return E_PENDING;
    case Gdiplus::InsufficientBuffer:   return HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
    case Gdiplus::NotImplemented:       return E_NOTIMPL;
    case Gdiplus::Win32Error:           return HRESULT_FROM_WIN32( GetLastError() );
    case Gdiplus::Aborted:              return E_ABORT;
    case Gdiplus::FileNotFound:         return HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
    case Gdiplus::AccessDenied:         return E_ACCESSDENIED;
    case Gdiplus::UnknownImageFormat:   return HRESULT_FROM_WIN32( ERROR_INVALID_PIXEL_FORMAT );
    default:                            return E_FAIL;
    }
}




//-----------------------------------------------------------------------------
// Name: ScaleToFit()
// Desc: The size of cx x cy scaled to fit cxMax x cyMax
//-----------------------------------------------------------------------------
SIZE ScaleToFit( int cxMax, int cyMax, int cx, int cy )
{
    SIZE size = { 0, cyMax };

    if( cx != 0 && cy != 0 )
    {
        if( cxMax * cy < cyMax * cx )
        {
            size.cx = cxMax;
            size.cy = (LONG)((LONGLONG)cxMax * cy / cx);
            return size;
        }
        size.cx = (LONG)((LONGLONG)cyMax * cx / cy);
    }
    return size;
}




//-----------------------------------------------------------------------------
// Name: CGdiplusHelper::CGdiplusHelper()
// Desc: Starts GDI+ and gets its codecs
//-----------------------------------------------------------------------------
CGdiplusHelper::CGdiplusHelper()
{
    Gdiplus::GdiplusStartupInput input;     // version 1, no callback

    Gdiplus::GdiplusStartup( &m_nToken, &input, NULL );

    m_pEncoders = NULL;
    m_nEncoders = 0;
    m_pDecoders = NULL;
    m_nDecoders = 0;

    LoadCodecs();
}

CGdiplusHelper::~CGdiplusHelper()
{
    FreeCodecs();
    Gdiplus::GdiplusShutdown( m_nToken );
}




//-----------------------------------------------------------------------------
// Name: CGdiplusHelper::LoadCodecs()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CGdiplusHelper::LoadCodecs()
{
    UINT cbSize = 0;

    HRESULT hr = GpStatusToHResult( Gdiplus::GetImageEncodersSize( &m_nEncoders, &cbSize ) );
    if( SUCCEEDED( hr ) && cbSize != 0 )
    {
        m_pEncoders = (Gdiplus::ImageCodecInfo*)LocalAlloc( LPTR, cbSize );
        if( m_pEncoders == NULL )
            hr = HRESULT_FROM_WIN32( GetLastError() );
        else
            hr = GpStatusToHResult( Gdiplus::GetImageEncoders( m_nEncoders, cbSize, m_pEncoders ) );
    }

    if( SUCCEEDED( hr ) )
    {
        hr = GpStatusToHResult( Gdiplus::GetImageDecodersSize( &m_nDecoders, &cbSize ) );
        if( SUCCEEDED( hr ) && cbSize != 0 )
        {
            m_pDecoders = (Gdiplus::ImageCodecInfo*)LocalAlloc( LPTR, cbSize );
            if( m_pDecoders == NULL )
                hr = HRESULT_FROM_WIN32( GetLastError() );
            else
                hr = GpStatusToHResult( Gdiplus::GetImageDecoders( m_nDecoders, cbSize, m_pDecoders ) );
        }
    }

    if( FAILED( hr ) )
        FreeCodecs();

    return hr;
}

void CGdiplusHelper::FreeCodecs()
{
    if( m_pEncoders != NULL )
    {
        LocalFree( m_pEncoders );
        m_pEncoders = NULL;
    }
    m_nEncoders = 0;

    if( m_pDecoders != NULL )
    {
        LocalFree( m_pDecoders );
        m_pDecoders = NULL;
    }
    m_nDecoders = 0;
}




//-----------------------------------------------------------------------------
// Name: CGdiplusHelper::GetImageExtensions()
// Desc: The file name patterns of all decoders ("*.BMP;*.DIB;...;*.PNG")
//-----------------------------------------------------------------------------
static void AppendExtensions( CStr& strExtensions, const Gdiplus::ImageCodecInfo* pCodecs,
                              UINT nCodecs )
{
    for( UINT i = 0; i < nCodecs; i++ )
    {
        if( strExtensions.Length() != 0 )
            strExtensions += CStr( L";" );
        strExtensions += CStr( pCodecs[i].FilenameExtension );
    }
}

void CGdiplusHelper::GetImageExtensions( CStr& strExtensions ) const
{
    AppendExtensions( strExtensions, m_pDecoders, m_nDecoders );
}




//-----------------------------------------------------------------------------
// Name: CGdiplusHelper::LoadScaledBitmap()
// Desc: A picture file as a bitmap: scaled down to fit cxMax x cyMax, or up
//       as well with bStretch
//-----------------------------------------------------------------------------
HRESULT CGdiplusHelper::LoadScaledBitmap( HBITMAP* phBitmap, LPCWSTR pszFile, UINT cxMax,
                                          UINT cyMax, bool bStretch )
{
    if( pszFile == NULL || lstrlenW( pszFile ) == 0 )
        return E_FAIL;

    Gdiplus::Bitmap image( CStr( pszFile ).String() );
    HRESULT hr = GpStatusToHResult( image.GetLastStatus() );
    if( FAILED( hr ) )
        return hr;

    UINT cxImage = image.GetWidth();
    UINT cyImage = image.GetHeight();
    if( cxImage == 0 || cyImage == 0 )
        return hr;

    UINT cx = cxImage;
    UINT cy = cyImage;
    if( cxMax != 0 && cyMax != 0 && (cxMax < cxImage || cyMax < cyImage || bStretch) )
    {
        SIZE size = ScaleToFit( cxMax, cyMax, cxImage, cyImage );
        cx = size.cx;
        cy = size.cy;
    }

    if( cx != 0 && cy != 0 )
    {
        Gdiplus::Bitmap bitmap( cx, cy, PixelFormat32bppARGB );
        hr = GpStatusToHResult( bitmap.GetLastStatus() );
        if( SUCCEEDED( hr ) )
        {
            Gdiplus::Graphics* pGraphics = Gdiplus::Graphics::FromImage( &bitmap );
            if( pGraphics != NULL )
            {
                hr = GpStatusToHResult( pGraphics->GetLastStatus() );
                if( SUCCEEDED( hr ) )
                {
                    hr = GpStatusToHResult( pGraphics->DrawImage( &image, 0, 0, (INT)cx, (INT)cy ) );
                    if( SUCCEEDED( hr ) )
                    {
                        hr = GpStatusToHResult( bitmap.GetHBITMAP( Gdiplus::Color( 0xff000000 ),
                                                                   phBitmap ) );
                        if( SUCCEEDED( hr ) && *phBitmap == NULL )
                            hr = E_FAIL;
                    }
                }
                delete pGraphics;
            }
        }
    }

    return hr;
}
