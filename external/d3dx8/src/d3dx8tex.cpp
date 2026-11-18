//-----------------------------------------------------------------------------
// File: d3dx8tex.cpp
//
// Desc: Texture loading functions of the D3DX 8 subset used by the Direct3D 8
//       screen savers (clean-room implementation, see ../README.md).
//
//       BMP, JPEG, PNG and GIF pictures are decoded with GDI+ (gdiplus.dll,
//       part of Windows since Windows XP), TGA and DDS files here.  Pictures
//       are scaled to a power of two (when the device needs it), converted
//       to a format the device supports, and get a full chain of mip levels
//       (box filter) unless asked otherwise.
//-----------------------------------------------------------------------------
#include <windows.h>
#include <objbase.h>
#include <new>
#include <vector>
#include <string.h>
#include "d3dx8.h"

namespace
{

//-----------------------------------------------------------------------------
// GDI+ (flat API, loaded on first use)
//-----------------------------------------------------------------------------
struct GdiplusStartupInput
{
    UINT32 GdiplusVersion;
    void*  DebugEventCallback;
    BOOL   SuppressBackgroundThread;
    BOOL   SuppressExternalCodecs;
};

struct GpBitmapData
{
    UINT      Width;
    UINT      Height;
    INT       Stride;
    INT       PixelFormat;
    void*     Scan0;
    UINT_PTR  Reserved;
};

struct GpRect { INT X, Y, Width, Height; };

#define PixelFormat32bppARGB    0x0026200A
#define ImageLockModeRead       1

typedef int (WINAPI *PFNGDIPLUSSTARTUP)( ULONG_PTR*, const GdiplusStartupInput*, void* );
typedef int (WINAPI *PFNGDIPCREATEBITMAPFROMSTREAM)( IStream*, void** );
typedef int (WINAPI *PFNGDIPGETIMAGEWIDTH)( void*, UINT* );
typedef int (WINAPI *PFNGDIPGETIMAGEHEIGHT)( void*, UINT* );
typedef int (WINAPI *PFNGDIPBITMAPLOCKBITS)( void*, const GpRect*, UINT, INT, GpBitmapData* );
typedef int (WINAPI *PFNGDIPBITMAPUNLOCKBITS)( void*, GpBitmapData* );
typedef int (WINAPI *PFNGDIPDISPOSEIMAGE)( void* );

static HMODULE                          s_hGdiplus;
static ULONG_PTR                        s_gdiplusToken;
static PFNGDIPCREATEBITMAPFROMSTREAM    s_pfnCreateBitmapFromStream;
static PFNGDIPGETIMAGEWIDTH             s_pfnGetImageWidth;
static PFNGDIPGETIMAGEHEIGHT            s_pfnGetImageHeight;
static PFNGDIPBITMAPLOCKBITS            s_pfnBitmapLockBits;
static PFNGDIPBITMAPUNLOCKBITS          s_pfnBitmapUnlockBits;
static PFNGDIPDISPOSEIMAGE              s_pfnDisposeImage;

static BOOL InitGdiplus()
{
    if( s_pfnCreateBitmapFromStream != NULL )
        return TRUE;

    s_hGdiplus = LoadLibraryW( L"gdiplus.dll" );
    if( s_hGdiplus == NULL )
        return FALSE;

    PFNGDIPLUSSTARTUP pfnStartup = (PFNGDIPLUSSTARTUP)(void*)GetProcAddress( s_hGdiplus, "GdiplusStartup" );
    s_pfnGetImageWidth = (PFNGDIPGETIMAGEWIDTH)(void*)GetProcAddress( s_hGdiplus, "GdipGetImageWidth" );
    s_pfnGetImageHeight = (PFNGDIPGETIMAGEHEIGHT)(void*)GetProcAddress( s_hGdiplus, "GdipGetImageHeight" );
    s_pfnBitmapLockBits = (PFNGDIPBITMAPLOCKBITS)(void*)GetProcAddress( s_hGdiplus, "GdipBitmapLockBits" );
    s_pfnBitmapUnlockBits = (PFNGDIPBITMAPUNLOCKBITS)(void*)GetProcAddress( s_hGdiplus, "GdipBitmapUnlockBits" );
    s_pfnDisposeImage = (PFNGDIPDISPOSEIMAGE)(void*)GetProcAddress( s_hGdiplus, "GdipDisposeImage" );
    PFNGDIPCREATEBITMAPFROMSTREAM pfnCreate =
        (PFNGDIPCREATEBITMAPFROMSTREAM)(void*)GetProcAddress( s_hGdiplus, "GdipCreateBitmapFromStream" );
    if( pfnStartup == NULL || pfnCreate == NULL || s_pfnGetImageWidth == NULL ||
        s_pfnGetImageHeight == NULL || s_pfnBitmapLockBits == NULL || s_pfnBitmapUnlockBits == NULL ||
        s_pfnDisposeImage == NULL )
        return FALSE;

    GdiplusStartupInput input = { 1, NULL, FALSE, FALSE };
    if( pfnStartup( &s_gdiplusToken, &input, NULL ) != 0 )
        return FALSE;

    s_pfnCreateBitmapFromStream = pfnCreate;
    return TRUE;
}


//-----------------------------------------------------------------------------
// Decoded pictures: 32-bit ARGB, top-down
//-----------------------------------------------------------------------------
struct IMAGE
{
    UINT                width;
    UINT                height;
    std::vector<DWORD>  pixels;
    BOOL                bAlpha;     // has transparency
    D3DFORMAT           format;     // format of the file
    D3DXIMAGE_FILEFORMAT fileFormat;
};

static HRESULT DecodeWithGdiplus( LPCVOID pData, UINT cbData, IMAGE* pImage )
{
    if( !InitGdiplus() )
        return E_FAIL;

    HGLOBAL hMem = GlobalAlloc( GMEM_MOVEABLE, cbData );
    if( hMem == NULL )
        return E_OUTOFMEMORY;
    memcpy( GlobalLock( hMem ), pData, cbData );
    GlobalUnlock( hMem );

    IStream* pStream = NULL;
    if( FAILED( CreateStreamOnHGlobal( hMem, TRUE, &pStream ) ) )
    {
        GlobalFree( hMem );
        return E_FAIL;
    }

    HRESULT hr = D3DXERR_INVALIDDATA;
    void* pBitmap = NULL;
    if( s_pfnCreateBitmapFromStream( pStream, &pBitmap ) == 0 && pBitmap != NULL )
    {
        UINT w = 0, h = 0;
        s_pfnGetImageWidth( pBitmap, &w );
        s_pfnGetImageHeight( pBitmap, &h );

        GpRect rc = { 0, 0, (INT)w, (INT)h };
        GpBitmapData bd;
        if( w > 0 && h > 0 && s_pfnBitmapLockBits( pBitmap, &rc, ImageLockModeRead, PixelFormat32bppARGB, &bd ) == 0 )
        {
            pImage->width = w;
            pImage->height = h;
            pImage->pixels.resize( (size_t)w * h );
            pImage->bAlpha = FALSE;
            for( UINT y = 0; y < h; y++ )
            {
                const DWORD* pRow = (const DWORD*)((const BYTE*)bd.Scan0 + (INT_PTR)y * bd.Stride);
                for( UINT x = 0; x < w; x++ )
                {
                    pImage->pixels[(size_t)y * w + x] = pRow[x];
                    if( (pRow[x] >> 24) != 0xff )
                        pImage->bAlpha = TRUE;
                }
            }
            s_pfnBitmapUnlockBits( pBitmap, &bd );
            hr = S_OK;
        }
        s_pfnDisposeImage( pBitmap );
    }

    pStream->Release();
    return hr;
}

// Uncompressed and RLE TGA (8-bit gray, 16, 24 and 32 bits)
static HRESULT DecodeTga( const BYTE* p, UINT cb, IMAGE* pImage )
{
    if( cb < 18 )
        return D3DXERR_INVALIDDATA;

    UINT idLength = p[0];
    UINT colorMapType = p[1];
    UINT imageType = p[2];
    UINT width = p[12] | (p[13] << 8);
    UINT height = p[14] | (p[15] << 8);
    UINT bpp = p[16];
    UINT desc = p[17];

    BOOL bRle = (imageType & 8) != 0;
    UINT type = imageType & 7;
    if( colorMapType != 0 || (type != 2 && type != 3) || width == 0 || height == 0 ||
        (bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) )
        return D3DXERR_INVALIDDATA;

    const BYTE* pSrc = p + 18 + idLength;
    const BYTE* pEnd = p + cb;
    UINT bytes = bpp / 8;
    std::vector<DWORD> pix( (size_t)width * height );

    UINT i = 0;
    while( i < width * height )
    {
        UINT count = 1;
        BOOL bRun = FALSE;
        if( bRle )
        {
            if( pSrc >= pEnd )
                return D3DXERR_INVALIDDATA;
            count = (*pSrc & 0x7f) + 1;
            bRun = (*pSrc & 0x80) != 0;
            pSrc++;
        }
        else
        {
            count = width * height;
        }

        for( UINT k = 0; k < count && i < width * height; k++ )
        {
            if( pSrc + bytes > pEnd )
                return D3DXERR_INVALIDDATA;
            DWORD c;
            switch( bpp )
            {
                case 8:  c = 0xff000000 | (pSrc[0] << 16) | (pSrc[0] << 8) | pSrc[0]; break;
                case 16:
                {
                    WORD w = pSrc[0] | (pSrc[1] << 8);
                    DWORD r = (w >> 10) & 31, g = (w >> 5) & 31, b = w & 31;
                    c = ((w & 0x8000) ? 0xff000000 : 0) | ((r * 255 / 31) << 16) | ((g * 255 / 31) << 8) | (b * 255 / 31);
                    if( (desc & 0x0f) == 0 )
                        c |= 0xff000000;
                    break;
                }
                case 24: c = 0xff000000 | (pSrc[2] << 16) | (pSrc[1] << 8) | pSrc[0]; break;
                default: c = ((DWORD)pSrc[3] << 24) | (pSrc[2] << 16) | (pSrc[1] << 8) | pSrc[0]; break;
            }
            pix[i++] = c;
            if( !bRun || k + 1 == count )
                pSrc += bytes;
        }
    }

    // Bottom-up unless bit 5 of the descriptor says top-down
    pImage->width = width;
    pImage->height = height;
    pImage->pixels.resize( (size_t)width * height );
    pImage->bAlpha = FALSE;
    for( UINT y = 0; y < height; y++ )
    {
        UINT ySrc = (desc & 0x20) ? y : height - 1 - y;
        for( UINT x = 0; x < width; x++ )
        {
            UINT xSrc = (desc & 0x10) ? width - 1 - x : x;
            DWORD c = pix[(size_t)ySrc * width + xSrc];
            pImage->pixels[(size_t)y * width + x] = c;
            if( (c >> 24) != 0xff )
                pImage->bAlpha = TRUE;
        }
    }
    return S_OK;
}


//-----------------------------------------------------------------------------
// DDS
//-----------------------------------------------------------------------------
#define DDS_MAGIC           0x20534444  // "DDS "
#define DDPF_ALPHAPIXELS    0x00000001
#define DDPF_FOURCC         0x00000004
#define DDPF_RGB            0x00000040
#define DDPF_LUMINANCE      0x00020000

struct DDSPIXELFORMAT
{
    DWORD dwSize, dwFlags, dwFourCC, dwRGBBitCount;
    DWORD dwRBitMask, dwGBitMask, dwBBitMask, dwABitMask;
};

struct DDSHEADER
{
    DWORD dwSize, dwFlags, dwHeight, dwWidth, dwPitchOrLinearSize, dwDepth, dwMipMapCount;
    DWORD dwReserved1[11];
    DDSPIXELFORMAT ddspf;
    DWORD dwCaps, dwCaps2, dwCaps3, dwCaps4, dwReserved2;
};

static DWORD Expand( DWORD v, DWORD mask )
{
    if( mask == 0 )
        return 0xff;
    int shift = 0;
    while( !(mask & 1) )
    {
        mask >>= 1;
        v >>= 1;
        shift++;
    }
    v &= mask;
    return (v * 255 + mask / 2) / mask;
}

// Decodes one 4x4 color block of DXT1..5
static void DecodeColorBlock( const BYTE* p, DWORD out[16], BOOL bDxt1 )
{
    WORD c0 = p[0] | (p[1] << 8), c1 = p[2] | (p[3] << 8);
    DWORD col[4];
    DWORD r0 = (c0 >> 11) * 255 / 31, g0 = ((c0 >> 5) & 63) * 255 / 63, b0 = (c0 & 31) * 255 / 31;
    DWORD r1 = (c1 >> 11) * 255 / 31, g1 = ((c1 >> 5) & 63) * 255 / 63, b1 = (c1 & 31) * 255 / 31;

    col[0] = 0xff000000 | (r0 << 16) | (g0 << 8) | b0;
    col[1] = 0xff000000 | (r1 << 16) | (g1 << 8) | b1;
    if( !bDxt1 || c0 > c1 )
    {
        col[2] = 0xff000000 | (((2 * r0 + r1) / 3) << 16) | (((2 * g0 + g1) / 3) << 8) | ((2 * b0 + b1) / 3);
        col[3] = 0xff000000 | (((r0 + 2 * r1) / 3) << 16) | (((g0 + 2 * g1) / 3) << 8) | ((b0 + 2 * b1) / 3);
    }
    else
    {
        col[2] = 0xff000000 | (((r0 + r1) / 2) << 16) | (((g0 + g1) / 2) << 8) | ((b0 + b1) / 2);
        col[3] = 0x00000000;
    }

    DWORD bits = p[4] | (p[5] << 8) | (p[6] << 16) | ((DWORD)p[7] << 24);
    for( int i = 0; i < 16; i++ )
        out[i] = col[(bits >> (i * 2)) & 3];
}

static HRESULT DecodeDds( const BYTE* p, UINT cb, IMAGE* pImage )
{
    if( cb < 4 + sizeof(DDSHEADER) || *(const DWORD*)p != DDS_MAGIC )
        return D3DXERR_INVALIDDATA;

    const DDSHEADER* ph = (const DDSHEADER*)(p + 4);
    const BYTE* pData = p + 4 + sizeof(DDSHEADER);
    const BYTE* pEnd = p + cb;
    UINT w = ph->dwWidth, h = ph->dwHeight;
    if( w == 0 || h == 0 )
        return D3DXERR_INVALIDDATA;

    pImage->width = w;
    pImage->height = h;
    pImage->pixels.resize( (size_t)w * h );
    pImage->bAlpha = FALSE;

    const DDSPIXELFORMAT& pf = ph->ddspf;
    if( pf.dwFlags & DDPF_FOURCC )
    {
        int kind;
        if( pf.dwFourCC == MAKEFOURCC('D','X','T','1') )      { kind = 1; pImage->format = D3DFMT_DXT1; }
        else if( pf.dwFourCC == MAKEFOURCC('D','X','T','2') ) { kind = 3; pImage->format = D3DFMT_DXT2; }
        else if( pf.dwFourCC == MAKEFOURCC('D','X','T','3') ) { kind = 3; pImage->format = D3DFMT_DXT3; }
        else if( pf.dwFourCC == MAKEFOURCC('D','X','T','4') ) { kind = 5; pImage->format = D3DFMT_DXT4; }
        else if( pf.dwFourCC == MAKEFOURCC('D','X','T','5') ) { kind = 5; pImage->format = D3DFMT_DXT5; }
        else return D3DXERR_INVALIDDATA;

        UINT blockSize = (kind == 1) ? 8 : 16;
        UINT bw = (w + 3) / 4, bh = (h + 3) / 4;
        if( pData + (size_t)bw * bh * blockSize > pEnd )
            return D3DXERR_INVALIDDATA;

        for( UINT by = 0; by < bh; by++ )
        {
            for( UINT bx = 0; bx < bw; bx++ )
            {
                const BYTE* pb = pData + ((size_t)by * bw + bx) * blockSize;
                DWORD texel[16];
                DecodeColorBlock( pb + ((kind == 1) ? 0 : 8), texel, kind == 1 );

                if( kind == 3 )
                {
                    for( int i = 0; i < 16; i++ )
                    {
                        DWORD a = (pb[i / 2] >> ((i & 1) * 4)) & 15;
                        texel[i] = (texel[i] & 0x00ffffff) | ((a * 17) << 24);
                    }
                }
                else if( kind == 5 )
                {
                    DWORD a[8];
                    a[0] = pb[0];
                    a[1] = pb[1];
                    if( a[0] > a[1] )
                    {
                        for( int i = 1; i < 7; i++ )
                            a[i + 1] = ((7 - i) * a[0] + i * a[1]) / 7;
                    }
                    else
                    {
                        for( int i = 1; i < 5; i++ )
                            a[i + 1] = ((5 - i) * a[0] + i * a[1]) / 5;
                        a[6] = 0;
                        a[7] = 255;
                    }
                    ULONGLONG bits = 0;
                    for( int i = 0; i < 6; i++ )
                        bits |= (ULONGLONG)pb[2 + i] << (8 * i);
                    for( int i = 0; i < 16; i++ )
                        texel[i] = (texel[i] & 0x00ffffff) | (a[(bits >> (3 * i)) & 7] << 24);
                }

                for( int i = 0; i < 16; i++ )
                {
                    UINT x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                    if( x < w && y < h )
                    {
                        pImage->pixels[(size_t)y * w + x] = texel[i];
                        if( (texel[i] >> 24) != 0xff )
                            pImage->bAlpha = TRUE;
                    }
                }
            }
        }
        return S_OK;
    }

    if( !(pf.dwFlags & (DDPF_RGB | DDPF_LUMINANCE)) || pf.dwRGBBitCount == 0 || pf.dwRGBBitCount > 32 ||
        (pf.dwRGBBitCount % 8) != 0 )
        return D3DXERR_INVALIDDATA;

    UINT bytes = pf.dwRGBBitCount / 8;
    UINT pitch = w * bytes;
    if( pData + (size_t)pitch * h > pEnd )
        return D3DXERR_INVALIDDATA;

    pImage->format = (pf.dwFlags & DDPF_ALPHAPIXELS) ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    for( UINT y = 0; y < h; y++ )
    {
        for( UINT x = 0; x < w; x++ )
        {
            const BYTE* pp = pData + (size_t)y * pitch + x * bytes;
            DWORD v = 0;
            for( UINT k = 0; k < bytes; k++ )
                v |= (DWORD)pp[k] << (8 * k);
            DWORD r, g, b, a;
            if( pf.dwFlags & DDPF_LUMINANCE )
                r = g = b = Expand( v, pf.dwRBitMask );
            else
            {
                r = Expand( v, pf.dwRBitMask );
                g = Expand( v, pf.dwGBitMask );
                b = Expand( v, pf.dwBBitMask );
            }
            a = (pf.dwFlags & DDPF_ALPHAPIXELS) ? Expand( v, pf.dwABitMask ) : 0xff;
            pImage->pixels[(size_t)y * w + x] = (a << 24) | (r << 16) | (g << 8) | b;
            if( a != 0xff )
                pImage->bAlpha = TRUE;
        }
    }
    return S_OK;
}


static HRESULT DecodeImage( LPCVOID pData, UINT cbData, IMAGE* pImage )
{
    const BYTE* p = (const BYTE*)pData;

    if( pData == NULL || cbData == 0 )
        return D3DERR_INVALIDCALL;

    pImage->format = D3DFMT_UNKNOWN;

    if( cbData >= 4 && *(const DWORD*)p == DDS_MAGIC )
    {
        pImage->fileFormat = D3DXIFF_DDS;
        return DecodeDds( p, cbData, pImage );
    }

    if( cbData >= 2 && p[0] == 'B' && p[1] == 'M' )
        pImage->fileFormat = D3DXIFF_BMP;
    else if( cbData >= 3 && p[0] == 0xff && p[1] == 0xd8 )
        pImage->fileFormat = D3DXIFF_JPG;
    else if( cbData >= 8 && p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G' )
        pImage->fileFormat = D3DXIFF_PNG;
    else if( cbData >= 3 && p[0] == 'G' && p[1] == 'I' && p[2] == 'F' )
        pImage->fileFormat = D3DXIFF_BMP;   // (no GIF in D3DXIMAGE_FILEFORMAT)
    else
    {
        // Anything else is taken for a TGA (no signature)
        pImage->fileFormat = D3DXIFF_TGA;
        HRESULT hr = DecodeTga( p, cbData, pImage );
        if( SUCCEEDED( hr ) )
            pImage->format = pImage->bAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
        return hr;
    }

    HRESULT hr = DecodeWithGdiplus( pData, cbData, pImage );
    if( SUCCEEDED( hr ) )
        pImage->format = pImage->bAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    return hr;
}


//-----------------------------------------------------------------------------
// Scaling and mip levels
//-----------------------------------------------------------------------------
static UINT NextPow2( UINT n )
{
    UINT p = 1;
    while( p < n )
        p <<= 1;
    return p;
}

// Resamples src (sw x sh) to dst (dw x dh): box filter when shrinking,
// bilinear when enlarging; filter D3DX_FILTER_POINT / NONE pick texels
static void Resample( const DWORD* src, UINT sw, UINT sh, DWORD* dst, UINT dw, UINT dh, DWORD dwFilter )
{
    DWORD f = dwFilter & 0xffff;

    for( UINT y = 0; y < dh; y++ )
    {
        for( UINT x = 0; x < dw; x++ )
        {
            if( f == D3DX_FILTER_POINT || f == D3DX_FILTER_NONE || (sw == dw && sh == dh) )
            {
                UINT sx = (UINT)((ULONGLONG)x * sw / dw);
                UINT sy = (UINT)((ULONGLONG)y * sh / dh);
                if( f == D3DX_FILTER_NONE )
                {
                    // no scaling: the picture goes into the top left corner
                    sx = x;
                    sy = y;
                    if( sx >= sw || sy >= sh )
                    {
                        dst[(size_t)y * dw + x] = 0;
                        continue;
                    }
                }
                dst[(size_t)y * dw + x] = src[(size_t)sy * sw + sx];
                continue;
            }

            double fx0 = (double)x * sw / dw, fx1 = (double)(x + 1) * sw / dw;
            double fy0 = (double)y * sh / dh, fy1 = (double)(y + 1) * sh / dh;
            double acc[4] = { 0, 0, 0, 0 };
            double wsum = 0;

            if( fx1 - fx0 >= 1.0 || fy1 - fy0 >= 1.0 )
            {
                // shrinking: average the covered texels
                for( UINT sy = (UINT)fy0; sy < sh && sy < fy1; sy++ )
                {
                    double wy = (sy + 1 < fy1 ? sy + 1 : fy1) - (sy > fy0 ? sy : fy0);
                    for( UINT sx = (UINT)fx0; sx < sw && sx < fx1; sx++ )
                    {
                        double wx = (sx + 1 < fx1 ? sx + 1 : fx1) - (sx > fx0 ? sx : fx0);
                        DWORD c = src[(size_t)sy * sw + sx];
                        double wgt = wx * wy;
                        for( int k = 0; k < 4; k++ )
                            acc[k] += wgt * ((c >> (k * 8)) & 0xff);
                        wsum += wgt;
                    }
                }
            }
            else
            {
                // enlarging: bilinear
                double cx = (x + 0.5) * sw / dw - 0.5, cy = (y + 0.5) * sh / dh - 0.5;
                int ix = (int)floor( cx ), iy = (int)floor( cy );
                double tx = cx - ix, ty = cy - iy;
                for( int j = 0; j < 2; j++ )
                {
                    for( int i = 0; i < 2; i++ )
                    {
                        int sx = ix + i, sy = iy + j;
                        if( sx < 0 ) sx = 0;
                        if( sy < 0 ) sy = 0;
                        if( sx >= (int)sw ) sx = sw - 1;
                        if( sy >= (int)sh ) sy = sh - 1;
                        double wgt = (i ? tx : 1 - tx) * (j ? ty : 1 - ty);
                        DWORD c = src[(size_t)sy * sw + sx];
                        for( int k = 0; k < 4; k++ )
                            acc[k] += wgt * ((c >> (k * 8)) & 0xff);
                        wsum += wgt;
                    }
                }
            }

            DWORD out = 0;
            for( int k = 0; k < 4; k++ )
            {
                int v = wsum > 0 ? (int)(acc[k] / wsum + 0.5) : 0;
                if( v > 255 ) v = 255;
                out |= (DWORD)v << (k * 8);
            }
            dst[(size_t)y * dw + x] = out;
        }
    }
}


//-----------------------------------------------------------------------------
// Writing pixels in a surface format
//-----------------------------------------------------------------------------
static UINT FormatBytes( D3DFORMAT fmt )
{
    switch( fmt )
    {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:   return 4;
        case D3DFMT_R8G8B8:     return 3;
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_X4R4G4B4:   return 2;
        default:                return 0;
    }
}

static void WritePixels( const DWORD* src, UINT w, UINT h, D3DFORMAT fmt, BYTE* pBits, INT pitch )
{
    for( UINT y = 0; y < h; y++ )
    {
        BYTE* pRow = pBits + (INT_PTR)y * pitch;
        for( UINT x = 0; x < w; x++ )
        {
            DWORD c = src[(size_t)y * w + x];
            DWORD a = c >> 24, r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
            switch( fmt )
            {
                case D3DFMT_A8R8G8B8: ((DWORD*)pRow)[x] = c; break;
                case D3DFMT_X8R8G8B8: ((DWORD*)pRow)[x] = c | 0xff000000; break;
                case D3DFMT_R8G8B8:   pRow[x * 3] = (BYTE)b; pRow[x * 3 + 1] = (BYTE)g; pRow[x * 3 + 2] = (BYTE)r; break;
                case D3DFMT_R5G6B5:   ((WORD*)pRow)[x] = (WORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); break;
                case D3DFMT_X1R5G5B5: ((WORD*)pRow)[x] = (WORD)(0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)); break;
                case D3DFMT_A1R5G5B5: ((WORD*)pRow)[x] = (WORD)(((a >= 128) ? 0x8000 : 0) | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)); break;
                case D3DFMT_A4R4G4B4: ((WORD*)pRow)[x] = (WORD)(((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)); break;
                case D3DFMT_X4R4G4B4: ((WORD*)pRow)[x] = (WORD)(0xf000 | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)); break;
                default: break;
            }
        }
    }
}

// The first format in the list the device can use as a texture
static D3DFORMAT ChooseFormat( LPDIRECT3DDEVICE8 pDevice, D3DFORMAT fmtWanted, BOOL bAlpha )
{
    LPDIRECT3D8 pD3D = NULL;
    D3DDEVICE_CREATION_PARAMETERS cp;
    D3DDISPLAYMODE mode;
    D3DFORMAT result = D3DFMT_UNKNOWN;

    static const D3DFORMAT s_alpha[] = { D3DFMT_A8R8G8B8, D3DFMT_A4R4G4B4, D3DFMT_A1R5G5B5,
                                         D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5 };
    static const D3DFORMAT s_opaque[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_R5G6B5,
                                          D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A4R4G4B4 };

    if( FAILED( pDevice->GetDirect3D( &pD3D ) ) )
        return D3DFMT_A8R8G8B8;
    pDevice->GetCreationParameters( &cp );
    pDevice->GetDisplayMode( &mode );

    if( FormatBytes( fmtWanted ) != 0 &&
        SUCCEEDED( pD3D->CheckDeviceFormat( cp.AdapterOrdinal, cp.DeviceType, mode.Format, 0,
                                            D3DRTYPE_TEXTURE, fmtWanted ) ) )
    {
        result = fmtWanted;
    }
    else
    {
        const D3DFORMAT* list = bAlpha ? s_alpha : s_opaque;
        for( int i = 0; i < 6 && result == D3DFMT_UNKNOWN; i++ )
        {
            if( SUCCEEDED( pD3D->CheckDeviceFormat( cp.AdapterOrdinal, cp.DeviceType, mode.Format, 0,
                                                    D3DRTYPE_TEXTURE, list[i] ) ) )
                result = list[i];
        }
    }

    pD3D->Release();
    return result != D3DFMT_UNKNOWN ? result : D3DFMT_A8R8G8B8;
}


//-----------------------------------------------------------------------------
// The texture
//-----------------------------------------------------------------------------
static HRESULT CreateTextureFromImage( LPDIRECT3DDEVICE8 pDevice, IMAGE* pImage, UINT Width, UINT Height,
                                       UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                       DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey,
                                       D3DXIMAGE_INFO* pSrcInfo, LPDIRECT3DTEXTURE8* ppTexture )
{
    HRESULT hr;
    D3DCAPS8 caps;

    if( pSrcInfo )
    {
        pSrcInfo->Width = pImage->width;
        pSrcInfo->Height = pImage->height;
        pSrcInfo->Depth = 1;
        pSrcInfo->MipLevels = 1;
        pSrcInfo->Format = pImage->format;
        pSrcInfo->ResourceType = D3DRTYPE_TEXTURE;
        pSrcInfo->ImageFileFormat = pImage->fileFormat;
    }

    // Color key: those pixels become transparent black
    if( ColorKey != 0 )
    {
        for( size_t i = 0; i < pImage->pixels.size(); i++ )
        {
            if( pImage->pixels[i] == ColorKey )
            {
                pImage->pixels[i] = 0;
                pImage->bAlpha = TRUE;
            }
        }
    }

    pDevice->GetDeviceCaps( &caps );

    // Size
    if( Width == 0 || Width == D3DX_DEFAULT )
        Width = pImage->width;
    if( Height == 0 || Height == D3DX_DEFAULT )
        Height = pImage->height;
    if( caps.TextureCaps & D3DPTEXTURECAPS_POW2 )
    {
        Width = NextPow2( Width );
        Height = NextPow2( Height );
    }
    if( caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY )
        Width = Height = (Width > Height) ? Width : Height;
    while( caps.MaxTextureWidth && Width > caps.MaxTextureWidth )
        Width >>= 1;
    while( caps.MaxTextureHeight && Height > caps.MaxTextureHeight )
        Height >>= 1;
    if( Width == 0 ) Width = 1;
    if( Height == 0 ) Height = 1;

    // Mip levels
    UINT maxLevels = 1;
    for( UINT w = Width, h = Height; w > 1 || h > 1; w >>= 1, h >>= 1 )
        maxLevels++;
    if( !(caps.TextureCaps & D3DPTEXTURECAPS_MIPMAP) )
        maxLevels = 1;
    if( MipLevels == 0 || MipLevels == D3DX_DEFAULT || MipLevels > maxLevels )
        MipLevels = maxLevels;

    // Format
    if( Format == D3DFMT_UNKNOWN || Format == (D3DFORMAT)D3DX_DEFAULT )
        Format = pImage->format;
    Format = ChooseFormat( pDevice, Format, pImage->bAlpha );

    // A texture in the default pool cannot be locked: fill a system memory
    // copy and update it
    LPDIRECT3DTEXTURE8 pTexture = NULL;
    LPDIRECT3DTEXTURE8 pStaging = NULL;
    if( FAILED( hr = pDevice->CreateTexture( Width, Height, MipLevels, Usage, Format, Pool, &pTexture ) ) )
        return hr;
    if( Pool == D3DPOOL_DEFAULT )
    {
        if( FAILED( hr = pDevice->CreateTexture( Width, Height, MipLevels, 0, Format, D3DPOOL_SYSTEMMEM, &pStaging ) ) )
        {
            pTexture->Release();
            return hr;
        }
    }
    LPDIRECT3DTEXTURE8 pFill = pStaging ? pStaging : pTexture;

    // Level 0, then each level from the previous one
    std::vector<DWORD> level( (size_t)Width * Height );
    Resample( &pImage->pixels[0], pImage->width, pImage->height, &level[0], Width, Height,
              Filter == D3DX_DEFAULT ? (D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER) : Filter );

    UINT w = Width, h = Height;
    for( UINT iLevel = 0; iLevel < pFill->GetLevelCount(); iLevel++ )
    {
        D3DLOCKED_RECT lr;
        if( iLevel > 0 )
        {
            UINT nw = w > 1 ? w / 2 : 1;
            UINT nh = h > 1 ? h / 2 : 1;
            std::vector<DWORD> next( (size_t)nw * nh );
            Resample( &level[0], w, h, &next[0], nw, nh,
                      MipFilter == D3DX_DEFAULT ? D3DX_FILTER_BOX : (MipFilter == D3DX_FILTER_NONE ? D3DX_FILTER_BOX : MipFilter) );
            level.swap( next );
            w = nw;
            h = nh;
        }
        if( SUCCEEDED( hr = pFill->LockRect( iLevel, &lr, NULL, 0 ) ) )
        {
            WritePixels( &level[0], w, h, Format, (BYTE*)lr.pBits, lr.Pitch );
            pFill->UnlockRect( iLevel );
        }
        if( FAILED( hr ) )
            break;
    }

    if( SUCCEEDED( hr ) && pStaging )
        hr = pDevice->UpdateTexture( pStaging, pTexture );
    if( pStaging )
        pStaging->Release();

    if( FAILED( hr ) )
    {
        pTexture->Release();
        return hr;
    }

    *ppTexture = pTexture;
    return S_OK;
}

static HRESULT ReadFileData( LPCWSTR pSrcFile, std::vector<BYTE>& data )
{
    HANDLE hFile = CreateFileW( pSrcFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
        return D3DXERR_INVALIDDATA;

    DWORD cb = GetFileSize( hFile, NULL );
    HRESULT hr = D3DXERR_INVALIDDATA;
    if( cb != INVALID_FILE_SIZE && cb > 0 )
    {
        data.resize( cb );
        DWORD cbRead = 0;
        if( ReadFile( hFile, &data[0], cb, &cbRead, NULL ) && cbRead == cb )
            hr = S_OK;
    }
    CloseHandle( hFile );
    return hr;
}

// The data of a resource of type RT_RCDATA, or a bitmap (RT_BITMAP: a DIB,
// returned as a .bmp file)
static HRESULT ReadResourceData( HMODULE hModule, LPCWSTR pSrcResource, std::vector<BYTE>& data )
{
    HRSRC hRsrc = FindResourceW( hModule, pSrcResource, (LPCWSTR)RT_RCDATA );
    BOOL bBitmap = FALSE;
    if( hRsrc == NULL )
    {
        hRsrc = FindResourceW( hModule, pSrcResource, (LPCWSTR)RT_BITMAP );
        bBitmap = TRUE;
    }
    if( hRsrc == NULL )
        return D3DXERR_INVALIDDATA;

    DWORD cb = SizeofResource( hModule, hRsrc );
    HGLOBAL hGlobal = LoadResource( hModule, hRsrc );
    const BYTE* p = hGlobal ? (const BYTE*)LockResource( hGlobal ) : NULL;
    if( p == NULL || cb == 0 )
        return D3DXERR_INVALIDDATA;

    if( !bBitmap )
    {
        data.assign( p, p + cb );
        return S_OK;
    }

    // Prepend a BITMAPFILEHEADER to the DIB
    const BITMAPINFOHEADER* pbih = (const BITMAPINFOHEADER*)p;
    DWORD cbColors = 0;
    if( pbih->biSize >= sizeof(BITMAPINFOHEADER) )
    {
        DWORD nColors = pbih->biClrUsed;
        if( nColors == 0 && pbih->biBitCount <= 8 )
            nColors = 1u << pbih->biBitCount;
        cbColors = nColors * sizeof(RGBQUAD);
        if( pbih->biCompression == BI_BITFIELDS && pbih->biSize == sizeof(BITMAPINFOHEADER) )
            cbColors += 3 * sizeof(DWORD);
    }
    BITMAPFILEHEADER bfh;
    bfh.bfType = 0x4d42;    // "BM"
    bfh.bfSize = sizeof(bfh) + cb;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = sizeof(bfh) + pbih->biSize + cbColors;
    data.resize( sizeof(bfh) + cb );
    memcpy( &data[0], &bfh, sizeof(bfh) );
    memcpy( &data[sizeof(bfh)], p, cb );
    return S_OK;
}

static std::vector<WCHAR> ToWide( LPCSTR psz )
{
    std::vector<WCHAR> w;
    int cch = MultiByteToWideChar( CP_ACP, 0, psz, -1, NULL, 0 );
    if( cch > 0 )
    {
        w.resize( cch );
        MultiByteToWideChar( CP_ACP, 0, psz, -1, &w[0], cch );
    }
    else
    {
        w.push_back( 0 );
    }
    return w;
}

} // namespace


//===========================================================================
// API
//===========================================================================
HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx( LPDIRECT3DDEVICE8 pDevice, LPCVOID pSrcData,
    UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture )
{
    HRESULT hr;
    IMAGE image;

    if( pDevice == NULL || ppTexture == NULL )
        return D3DERR_INVALIDCALL;

    if( FAILED( hr = DecodeImage( pSrcData, SrcDataSize, &image ) ) )
        return hr;

    return CreateTextureFromImage( pDevice, &image, Width, Height, MipLevels, Usage, Format, Pool,
                                   Filter, MipFilter, ColorKey, pSrcInfo, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromFileInMemory( LPDIRECT3DDEVICE8 pDevice, LPCVOID pSrcData,
    UINT SrcDataSize, LPDIRECT3DTEXTURE8* ppTexture )
{
    return D3DXCreateTextureFromFileInMemoryEx( pDevice, pSrcData, SrcDataSize, D3DX_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT,
        0, NULL, NULL, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromFileExW( LPDIRECT3DDEVICE8 pDevice, LPCWSTR pSrcFile,
    UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture )
{
    HRESULT hr;
    std::vector<BYTE> data;

    if( pSrcFile == NULL )
        return D3DERR_INVALIDCALL;
    if( FAILED( hr = ReadFileData( pSrcFile, data ) ) )
        return hr;

    return D3DXCreateTextureFromFileInMemoryEx( pDevice, &data[0], (UINT)data.size(), Width, Height,
        MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromFileExA( LPDIRECT3DDEVICE8 pDevice, LPCSTR pSrcFile,
    UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture )
{
    if( pSrcFile == NULL )
        return D3DERR_INVALIDCALL;
    std::vector<WCHAR> w = ToWide( pSrcFile );
    return D3DXCreateTextureFromFileExW( pDevice, &w[0], Width, Height, MipLevels, Usage, Format,
        Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromFileW( LPDIRECT3DDEVICE8 pDevice, LPCWSTR pSrcFile, LPDIRECT3DTEXTURE8* ppTexture )
{
    return D3DXCreateTextureFromFileExW( pDevice, pSrcFile, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
        0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromFileA( LPDIRECT3DDEVICE8 pDevice, LPCSTR pSrcFile, LPDIRECT3DTEXTURE8* ppTexture )
{
    return D3DXCreateTextureFromFileExA( pDevice, pSrcFile, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
        0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromResourceExW( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCWSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture )
{
    HRESULT hr;
    std::vector<BYTE> data;

    if( FAILED( hr = ReadResourceData( hSrcModule, pSrcResource, data ) ) )
        return hr;

    return D3DXCreateTextureFromFileInMemoryEx( pDevice, &data[0], (UINT)data.size(), Width, Height,
        MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromResourceExA( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture )
{
    if( IS_INTRESOURCE( pSrcResource ) )
        return D3DXCreateTextureFromResourceExW( pDevice, hSrcModule, (LPCWSTR)pSrcResource, Width,
            Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture );

    std::vector<WCHAR> w = ToWide( pSrcResource );
    return D3DXCreateTextureFromResourceExW( pDevice, hSrcModule, &w[0], Width, Height, MipLevels,
        Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromResourceW( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCWSTR pSrcResource, LPDIRECT3DTEXTURE8* ppTexture )
{
    return D3DXCreateTextureFromResourceExW( pDevice, hSrcModule, pSrcResource, D3DX_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT,
        0, NULL, NULL, ppTexture );
}

HRESULT WINAPI D3DXCreateTextureFromResourceA( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCSTR pSrcResource, LPDIRECT3DTEXTURE8* ppTexture )
{
    return D3DXCreateTextureFromResourceExA( pDevice, hSrcModule, pSrcResource, D3DX_DEFAULT,
        D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT,
        0, NULL, NULL, ppTexture );
}
