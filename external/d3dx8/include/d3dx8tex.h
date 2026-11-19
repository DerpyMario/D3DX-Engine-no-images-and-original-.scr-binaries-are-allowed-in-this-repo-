//-----------------------------------------------------------------------------
// File: d3dx8tex.h
//
// Desc: Texture loading functions of the D3DX 8 utility library, as far as
//       the Direct3D 8 screen savers use them (clean-room replacement, see
//       ../README.md).  Supported source formats: BMP, JPEG, PNG, GIF, TGA
//       and DDS.
//-----------------------------------------------------------------------------
#ifndef __D3DX8TEX_H__
#define __D3DX8TEX_H__

#include <d3d8.h>

// Filters
#define D3DX_FILTER_NONE      (1 << 0)
#define D3DX_FILTER_POINT     (2 << 0)
#define D3DX_FILTER_LINEAR    (3 << 0)
#define D3DX_FILTER_TRIANGLE  (4 << 0)
#define D3DX_FILTER_BOX       (5 << 0)

#define D3DX_FILTER_MIRROR_U  (1 << 16)
#define D3DX_FILTER_MIRROR_V  (2 << 16)
#define D3DX_FILTER_MIRROR_W  (4 << 16)
#define D3DX_FILTER_MIRROR    (7 << 16)
#define D3DX_FILTER_DITHER    (8 << 16)

typedef enum _D3DXIMAGE_FILEFORMAT
{
    D3DXIFF_BMP         = 0,
    D3DXIFF_JPG         = 1,
    D3DXIFF_TGA         = 2,
    D3DXIFF_PNG         = 3,
    D3DXIFF_DDS         = 4,
    D3DXIFF_PPM         = 5,
    D3DXIFF_DIB         = 6,
    D3DXIFF_FORCE_DWORD = 0x7fffffff
} D3DXIMAGE_FILEFORMAT;

typedef struct _D3DXIMAGE_INFO
{
    UINT                    Width;
    UINT                    Height;
    UINT                    Depth;
    UINT                    MipLevels;
    D3DFORMAT               Format;
    D3DRESOURCETYPE         ResourceType;
    D3DXIMAGE_FILEFORMAT    ImageFileFormat;
} D3DXIMAGE_INFO;

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXCreateTextureFromFileA( LPDIRECT3DDEVICE8 pDevice, LPCSTR pSrcFile, LPDIRECT3DTEXTURE8* ppTexture );
HRESULT WINAPI D3DXCreateTextureFromFileW( LPDIRECT3DDEVICE8 pDevice, LPCWSTR pSrcFile, LPDIRECT3DTEXTURE8* ppTexture );

HRESULT WINAPI D3DXCreateTextureFromFileExA( LPDIRECT3DDEVICE8 pDevice, LPCSTR pSrcFile,
    UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture );
HRESULT WINAPI D3DXCreateTextureFromFileExW( LPDIRECT3DDEVICE8 pDevice, LPCWSTR pSrcFile,
    UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture );

HRESULT WINAPI D3DXCreateTextureFromResourceA( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCSTR pSrcResource, LPDIRECT3DTEXTURE8* ppTexture );
HRESULT WINAPI D3DXCreateTextureFromResourceW( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCWSTR pSrcResource, LPDIRECT3DTEXTURE8* ppTexture );

HRESULT WINAPI D3DXCreateTextureFromResourceExA( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture );
HRESULT WINAPI D3DXCreateTextureFromResourceExW( LPDIRECT3DDEVICE8 pDevice, HMODULE hSrcModule,
    LPCWSTR pSrcResource, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture );

HRESULT WINAPI D3DXCreateTextureFromFileInMemory( LPDIRECT3DDEVICE8 pDevice, LPCVOID pSrcData,
    UINT SrcDataSize, LPDIRECT3DTEXTURE8* ppTexture );
HRESULT WINAPI D3DXCreateTextureFromFileInMemoryEx( LPDIRECT3DDEVICE8 pDevice, LPCVOID pSrcData,
    UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
    D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE8* ppTexture );

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define D3DXCreateTextureFromFile D3DXCreateTextureFromFileW
#define D3DXCreateTextureFromFileEx D3DXCreateTextureFromFileExW
#define D3DXCreateTextureFromResource D3DXCreateTextureFromResourceW
#define D3DXCreateTextureFromResourceEx D3DXCreateTextureFromResourceExW
#else
#define D3DXCreateTextureFromFile D3DXCreateTextureFromFileA
#define D3DXCreateTextureFromFileEx D3DXCreateTextureFromFileExA
#define D3DXCreateTextureFromResource D3DXCreateTextureFromResourceA
#define D3DXCreateTextureFromResourceEx D3DXCreateTextureFromResourceExA
#endif

#endif // __D3DX8TEX_H__
