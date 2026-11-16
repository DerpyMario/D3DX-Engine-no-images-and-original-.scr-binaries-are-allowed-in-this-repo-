//-----------------------------------------------------------------------------
// File: texture.cpp
//
// Desc: Texture loading helpers for the Direct3D 9 screen savers.
//
//       Reconstructed from texture.obj as linked into Aurora.scr, Bubbles.scr
//       and ssBranded.scr (Targa path) and into ssText3d.scr (WIC path).
//
//       Note on parameters: the shipped images were built with link-time code
//       generation, and every caller passes the same (uMipLevels, dwUsage,
//       format) triple - (0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8) - so the
//       compiler folded those constants into the function bodies.  That the
//       three parameters feed IDirect3DDevice9::CreateTexture() is therefore
//       an inference from the prototypes, not something visible in the code.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\texture.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <objbase.h>
#include <d3d9.h>
#include <wincodec.h>
#include "texture.h"
#include "image.h"
#include "targa.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif




//-----------------------------------------------------------------------------
// Name: IsPowerOfTwo() / NextPowerOfTwo()
// Desc: (inlined in the shipped code)
//-----------------------------------------------------------------------------
static inline bool IsPowerOfTwo( UINT u )
{
    return ( u & ( u - 1 ) ) == 0;
}

static inline UINT NextPowerOfTwo( UINT u )
{
    if( !IsPowerOfTwo( u ) )
    {
        // Double the value, then strip low bits until only the top one is left
        u *= 2;
        while( !IsPowerOfTwo( u ) )
            u &= u - 1;
    }
    return u;
}




//-----------------------------------------------------------------------------
// Name: GetTextureSize()
// Desc: Rounds the image size up to powers of two if the device cannot handle
//       anything else.  (inlined in the shipped code)
//-----------------------------------------------------------------------------
static inline void GetTextureSize( IDirect3DDevice9* pd3dDevice, UINT uWidth, UINT uHeight,
                                   UINT* puTexWidth, UINT* puTexHeight )
{
    *puTexWidth  = uWidth;
    *puTexHeight = uHeight;

    if( !( IsPowerOfTwo( uWidth ) && IsPowerOfTwo( uHeight ) ) )
    {
        D3DCAPS9 caps;
        if( FAILED( pd3dDevice->GetDeviceCaps( &caps ) ) ||
            ( ( caps.TextureCaps & D3DPTEXTURECAPS_POW2 ) &&
              !( caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ) ) )
        {
            *puTexWidth  = NextPowerOfTwo( uWidth );
            *puTexHeight = NextPowerOfTwo( uHeight );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: Texture::CreateTextureFromTGAFileInMemory()
// Desc: Decodes a true-colour Targa image into a system-memory texture and
//       uploads that into a freshly created D3DPOOL_DEFAULT texture.
//-----------------------------------------------------------------------------
HRESULT __cdecl Texture::CreateTextureFromTGAFileInMemory( IDirect3DDevice9* pd3dDevice,
                                                           const BYTE* pData, UINT cbData,
                                                           UINT uMipLevels, DWORD dwUsage, D3DFORMAT format,
                                                           IDirect3DTexture9** ppTexture )
{
    HRESULT hr;
    UINT uWidth;
    UINT uHeight;
    UINT uTexWidth;
    UINT uTexHeight;

    (void)cbData;

    *ppTexture = NULL;

    if( Image::GetImageType( pData ) != Image::IMAGE_TGA )
        return E_INVALIDARG;

    if( !Image::GetWidthAndHeight( pData, uWidth, uHeight ) )
        return E_FAIL;

    GetTextureSize( pd3dDevice, uWidth, uHeight, &uTexWidth, &uTexHeight );

    hr = pd3dDevice->CreateTexture( uTexWidth, uTexHeight, uMipLevels, dwUsage, format,
                                    D3DPOOL_DEFAULT, ppTexture, NULL );
    if( FAILED( hr ) )
    {
        hr = E_INVALIDARG;
    }
    else
    {
        if( *ppTexture == NULL )
            return E_INVALIDARG;

        IDirect3DTexture9* pSysMemTexture = NULL;
        hr = pd3dDevice->CreateTexture( uTexWidth, uTexHeight, 1, 0, D3DFMT_A8R8G8B8,
                                        D3DPOOL_SYSTEMMEM, &pSysMemTexture, NULL );
        if( SUCCEEDED( hr ) && pSysMemTexture != NULL )
        {
            IDirect3DSurface9* pSysMemSurface = NULL;
            IDirect3DSurface9* pSurface;
            D3DLOCKED_RECT lr;

            pSysMemTexture->GetSurfaceLevel( 0, &pSysMemSurface );
            pSysMemSurface->LockRect( &lr, NULL, 0 );

            Targa::TargaReader* pReader = new Targa::TargaReader( pData );

            for( UINT y = 0; y < uHeight; y++ )
            {
                // Targa images are stored bottom-up unless the descriptor says otherwise
                UINT uRow = pReader->IsTopDown() ? y : ( uHeight - y - 1 );
                BYTE* pDest = (BYTE*)lr.pBits + uRow * lr.Pitch;

                for( UINT x = 0; x < uWidth; x++ )
                {
                    BYTE r, g, b, a;

                    if( pReader->IsDone() )
                        return E_FAIL;
                    if( !pReader->GetRGBA( r, g, b, a ) )
                        return E_FAIL;

                    pDest[0] = b;
                    pDest[1] = g;
                    pDest[2] = r;
                    pDest[3] = a;
                    pDest += 4;
                }
            }

            delete pReader;

            pSysMemSurface->UnlockRect();

            ( *ppTexture )->GetSurfaceLevel( 0, &pSurface );
            pd3dDevice->UpdateSurface( pSysMemSurface, NULL, pSurface, NULL );

            SAFE_RELEASE( pSurface );
            SAFE_RELEASE( pSysMemSurface );
            SAFE_RELEASE( pSysMemTexture );
            return S_OK;
        }

        ( *ppTexture )->Release();
    }

    *ppTexture = NULL;
    return hr;
}




//-----------------------------------------------------------------------------
// Name: Texture::LoadTextureFromResource()  [Vista RTM flavour]
// Desc: Loads a Targa image that is stored as a resource of the executable.
//       The screen savers keep their textures in resources of type "BMP",
//       although the data is in Targa format.
//-----------------------------------------------------------------------------
HRESULT __cdecl Texture::LoadTextureFromResource( IDirect3DDevice9* pd3dDevice,
                                                  const WCHAR* pszName, const WCHAR* pszType,
                                                  IDirect3DTexture9** ppTexture )
{
    HRSRC hResInfo = FindResourceW( NULL, pszName, pszType );
    if( hResInfo != NULL )
    {
        DWORD cbData = SizeofResource( NULL, hResInfo );
        if( cbData != 0 )
        {
            HGLOBAL hResData = LoadResource( NULL, hResInfo );
            if( hResData != NULL )
            {
                const BYTE* pData = (const BYTE*)LockResource( hResData );
                if( pData != NULL )
                {
                    HRESULT hr = CreateTextureFromTGAFileInMemory( pd3dDevice, pData, cbData, 0,
                                                                   D3DUSAGE_AUTOGENMIPMAP,
                                                                   D3DFMT_A8R8G8B8, ppTexture );
                    if( FAILED( hr ) )
                    {
                        *ppTexture = NULL;
                        return hr;
                    }
                }
            }
        }
    }

    return ( *ppTexture != NULL ) ? S_OK : E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: Texture::LoadTextureFromDecoder()  [Vista SP1]
// Desc: Converts the first frame of a WIC decoder into an A8R8G8B8 texture.
//       Understands 24bpp BGR/RGB, 32bpp BGR/BGRA/PBGRA and 4/8bpp indexed
//       frames; everything else fails with E_FAIL.
//-----------------------------------------------------------------------------
HRESULT __cdecl Texture::LoadTextureFromDecoder( IDirect3DDevice9* pd3dDevice,
                                                 IWICImagingFactory* pFactory, IWICBitmapDecoder* pDecoder,
                                                 UINT uMipLevels, DWORD dwUsage, D3DFORMAT format,
                                                 IDirect3DTexture9** ppTexture )
{
    HRESULT hr;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICPalette* pPalette = NULL;
    WICColor* pColors = NULL;
    UINT cFrames = 0;
    UINT uWidth = 0;
    UINT uHeight = 0;
    UINT cColors = 0;
    UINT cActualColors;
    int  nBitsPerPixel = 32;
    int  nIndexedBits = 8;
    bool bIndexed = false;
    bool bSourceAlpha = false;

    hr = pDecoder->GetFrameCount( &cFrames );
    if( cFrames < 1 )
        return E_FAIL;

    if( SUCCEEDED( hr ) )
        hr = pDecoder->GetFrame( 0, &pFrame );

    if( SUCCEEDED( hr ) )
        hr = pFrame->GetSize( &uWidth, &uHeight );

    if( SUCCEEDED( hr ) )
    {
        WICPixelFormatGUID guidFormat;

        hr = pFrame->GetPixelFormat( &guidFormat );
        if( SUCCEEDED( hr ) )
        {
            if( guidFormat == GUID_WICPixelFormat24bppBGR ||
                guidFormat == GUID_WICPixelFormat24bppRGB )
            {
                nBitsPerPixel = 24;
                bSourceAlpha = false;
            }
            else if( guidFormat == GUID_WICPixelFormat32bppBGR ||
                     guidFormat == GUID_WICPixelFormat32bppBGRA ||
                     guidFormat == GUID_WICPixelFormat32bppPBGRA )
            {
                bSourceAlpha = true;
            }
            else
            {
                if( guidFormat == GUID_WICPixelFormat8bppIndexed )
                {
                    // nIndexedBits is already 8
                }
                else if( guidFormat == GUID_WICPixelFormat4bppIndexed )
                {
                    nIndexedBits = 4;
                }
                else
                {
                    hr = E_FAIL;
                }

                if( SUCCEEDED( hr ) )
                {
                    bSourceAlpha = false;
                    bIndexed = true;
                    nBitsPerPixel = nIndexedBits;
                }
            }
        }
    }

    if( SUCCEEDED( hr ) )
    {
        if( bIndexed )
        {
            // Fetch the colour table
            if( FAILED( hr = pFactory->CreatePalette( &pPalette ) ) ||
                FAILED( hr = pFrame->CopyPalette( pPalette ) ) ||
                FAILED( hr = pPalette->GetColorCount( &cColors ) ) )
            {
                goto Cleanup;
            }

            if( cColors > 0 )
            {
                pColors = new WICColor[cColors];
                if( pColors == NULL )
                    hr = E_OUTOFMEMORY;
                else
                    hr = pPalette->GetColors( cColors, pColors, &cActualColors );
            }
        }

        if( SUCCEEDED( hr ) )
        {
            UINT uTexWidth;
            UINT uTexHeight;

            GetTextureSize( pd3dDevice, uWidth, uHeight, &uTexWidth, &uTexHeight );

            if( FAILED( pd3dDevice->CreateTexture( uTexWidth, uTexHeight, uMipLevels, dwUsage, format,
                                                   D3DPOOL_DEFAULT, ppTexture, NULL ) ) )
            {
                *ppTexture = NULL;
                hr = E_INVALIDARG;
            }
            else
            {
                IDirect3DTexture9* pSysMemTexture;

                hr = pd3dDevice->CreateTexture( uTexWidth, uTexHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                                D3DPOOL_SYSTEMMEM, &pSysMemTexture, NULL );
                if( SUCCEEDED( hr ) )
                {
                    IDirect3DSurface9* pSysMemSurface;
                    IDirect3DSurface9* pSurface;
                    D3DLOCKED_RECT lr;
                    HRESULT hrCopy;

                    pSysMemTexture->GetSurfaceLevel( 0, &pSysMemSurface );
                    hrCopy = pSysMemSurface->LockRect( &lr, NULL, D3DLOCK_DISCARD );

                    // Scratch copy of the decoded frame.  The stride is a very
                    // generous 16 bytes per pixel.
                    UINT  cbStride = uWidth * 16;
                    UINT  cbBuffer = cbStride * uHeight;
                    BYTE* pBuffer = new BYTE[cbBuffer];
                    if( pBuffer != NULL )
                    {
                        // Decode one scan line at a time
                        WICRect rc = { 0, 0, (INT)uWidth, 1 };
                        BYTE*   pLine = pBuffer;

                        for( UINT y = 0; SUCCEEDED( hrCopy ) && y < uHeight; y++ )
                        {
                            hrCopy = pFrame->CopyPixels( &rc, cbStride, cbBuffer - rc.Y * cbStride, pLine );
                            pLine += cbStride;
                            rc.Y++;
                        }

                        // Expand to B, G, R, A
                        pLine = pBuffer;
                        for( UINT y = 0; y < uHeight; y++ )
                        {
                            const BYTE* pSrc = pLine;
                            BYTE* pDest = (BYTE*)lr.pBits + lr.Pitch * y;

                            for( UINT x = 0; x < uWidth; x++ )
                            {
                                if( nBitsPerPixel == 4 )
                                {
                                    // Two pixels per byte, high nibble first
                                    BYTE bIndices = *pSrc++;
                                    WICColor color;

                                    color = pColors[bIndices >> 4];
                                    pDest[0] = (BYTE)( color );
                                    pDest[1] = (BYTE)( color >> 8 );
                                    pDest[2] = (BYTE)( color >> 16 );
                                    pDest[3] = 0xff;

                                    color = pColors[bIndices & 0x0f];
                                    pDest[4] = (BYTE)( color );
                                    pDest[5] = (BYTE)( color >> 8 );
                                    pDest[6] = (BYTE)( color >> 16 );
                                    pDest[7] = 0xff;

                                    pDest += 8;
                                    x++;
                                }
                                else if( nBitsPerPixel == 8 )
                                {
                                    *(DWORD*)pDest = pColors[*pSrc++];
                                    pDest += 4;
                                }
                                else if( nBitsPerPixel == 24 )
                                {
                                    pDest[0] = pSrc[0];
                                    pDest[1] = pSrc[1];
                                    pDest[2] = pSrc[2];
                                    pDest[3] = 0xff;
                                    pSrc += 3;
                                    pDest += 4;
                                }
                                else if( nBitsPerPixel == 32 )
                                {
                                    pDest[0] = pSrc[0];
                                    pDest[1] = pSrc[1];
                                    pDest[2] = pSrc[2];
                                    pDest[3] = bSourceAlpha ? pSrc[3] : 0xff;
                                    pSrc += 4;
                                    pDest += 4;
                                }
                            }

                            pLine += cbStride;
                        }

                        delete[] pBuffer;
                    }

                    pSysMemSurface->UnlockRect();

                    ( *ppTexture )->GetSurfaceLevel( 0, &pSurface );
                    hr = pd3dDevice->UpdateSurface( pSysMemSurface, NULL, pSurface, NULL );

                    SAFE_RELEASE( pSurface );
                    SAFE_RELEASE( pSysMemSurface );
                    SAFE_RELEASE( pSysMemTexture );
                }
            }
        }

        if( pColors != NULL )
            delete[] pColors;
    }

Cleanup:
    SAFE_RELEASE( pPalette );
    SAFE_RELEASE( pFrame );
    return hr;
}




//-----------------------------------------------------------------------------
// Name: Texture::LoadTextureFromResource()  [Vista SP1 flavour]
// Desc: Wraps an image resource of the executable in a stream and decodes it
//       with the Windows Imaging Component.
//-----------------------------------------------------------------------------
HRESULT __cdecl Texture::LoadTextureFromResource( IDirect3DDevice9* pd3dDevice,
                                                  const WCHAR* pszName, const WCHAR* pszType,
                                                  UINT uMipLevels, DWORD dwUsage, D3DFORMAT format,
                                                  IDirect3DTexture9** ppTexture )
{
    HRSRC hResInfo = FindResourceW( NULL, pszName, pszType );
    if( hResInfo == NULL )
        return E_FAIL;

    DWORD cbData = SizeofResource( NULL, hResInfo );
    if( cbData == 0 )
        return E_FAIL;

    HGLOBAL hResData = LoadResource( NULL, hResInfo );
    if( hResData == NULL )
        return E_FAIL;

    const void* pData = LockResource( hResData );
    if( pData == NULL )
        return E_FAIL;

    // CreateStreamOnHGlobal() needs a movable block of its own
    HGLOBAL hGlobal = GlobalAlloc( GMEM_MOVEABLE, cbData );
    if( hGlobal != NULL )
    {
        void* pCopy = GlobalLock( hGlobal );
        if( pCopy != NULL )
        {
            HRESULT hr;
            IStream* pStream = NULL;

            memcpy( pCopy, pData, cbData );
            GlobalUnlock( hGlobal );

            hr = CreateStreamOnHGlobal( hGlobal, TRUE, &pStream );
            if( SUCCEEDED( hr ) && pStream != NULL )
            {
                IWICImagingFactory* pFactory = NULL;
                IWICBitmapDecoder* pDecoder = NULL;

                hr = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                       IID_IWICImagingFactory, (void**)&pFactory );
                if( SUCCEEDED( hr ) )
                {
                    hr = pFactory->CreateDecoderFromStream( pStream, NULL, WICDecodeMetadataCacheOnDemand,
                                                            &pDecoder );
                    if( SUCCEEDED( hr ) )
                    {
                        hr = LoadTextureFromDecoder( pd3dDevice, pFactory, pDecoder,
                                                     uMipLevels, dwUsage, format, ppTexture );
                    }
                }

                SAFE_RELEASE( pFactory );
                SAFE_RELEASE( pDecoder );
                SAFE_RELEASE( pStream );
            }

            // (sic) The stream was created with fDeleteOnRelease = TRUE, so on
            // the success path the block has already been freed by the final
            // Release() above.  The shipped code frees it again regardless;
            // GlobalFree() rejects the stale handle.
            GlobalFree( hGlobal );
            return hr;
        }

        GlobalFree( hGlobal );
    }

    return HRESULT_FROM_WIN32( GetLastError() );
}
