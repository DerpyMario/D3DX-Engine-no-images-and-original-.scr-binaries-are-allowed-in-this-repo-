//-----------------------------------------------------------------------------
// File: texture.h
//
// Desc: Texture loading helpers for the Direct3D 9 screen savers.
//
//       The library went through two generations:
//
//       Vista RTM (Aurora.scr, Bubbles.scr; also ssBranded.scr, which only
//       links the TGA loader):
//           Textures are Targa files stored as "BMP" resources and are decoded
//           by the library's own reader.
//
//       Vista SP1 (ssText3d.scr):
//           Textures are JPEG files stored as "JPG" resources and are decoded
//           through the Windows Imaging Component.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\texture.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_TEXTURE_H
#define _HELPER_TEXTURE_H

#include <windows.h>
#include <d3d9.h>

struct IWICImagingFactory;
struct IWICBitmapDecoder;

namespace Texture
{
    // Creates a D3DPOOL_DEFAULT texture from a Targa file image.
    HRESULT __cdecl CreateTextureFromTGAFileInMemory( IDirect3DDevice9* pd3dDevice,
                                                      const BYTE* pData, UINT cbData,
                                                      UINT uMipLevels, DWORD dwUsage, D3DFORMAT format,
                                                      IDirect3DTexture9** ppTexture );

    // Creates a D3DPOOL_DEFAULT texture from the first frame of a WIC decoder.
    HRESULT __cdecl LoadTextureFromDecoder( IDirect3DDevice9* pd3dDevice,
                                            IWICImagingFactory* pFactory, IWICBitmapDecoder* pDecoder,
                                            UINT uMipLevels, DWORD dwUsage, D3DFORMAT format,
                                            IDirect3DTexture9** ppTexture );

    // SP1: loads an image resource of the executable through WIC.
    HRESULT __cdecl LoadTextureFromResource( IDirect3DDevice9* pd3dDevice,
                                             const WCHAR* pszName, const WCHAR* pszType,
                                             UINT uMipLevels, DWORD dwUsage, D3DFORMAT format,
                                             IDirect3DTexture9** ppTexture );

    // RTM: loads a Targa resource of the executable (mip-mapped A8R8G8B8).
    HRESULT __cdecl LoadTextureFromResource( IDirect3DDevice9* pd3dDevice,
                                             const WCHAR* pszName, const WCHAR* pszType,
                                             IDirect3DTexture9** ppTexture );
}

#endif // _HELPER_TEXTURE_H
