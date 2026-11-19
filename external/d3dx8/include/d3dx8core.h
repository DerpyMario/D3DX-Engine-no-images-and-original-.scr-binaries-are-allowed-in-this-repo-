//-----------------------------------------------------------------------------
// File: d3dx8core.h
//
// Desc: ID3DXBuffer of the D3DX 8 utility library (clean-room replacement,
//       see ../README.md).
//-----------------------------------------------------------------------------
#ifndef __D3DX8CORE_H__
#define __D3DX8CORE_H__

#include <d3d8.h>

typedef interface ID3DXBuffer ID3DXBuffer;
typedef interface ID3DXBuffer *LPD3DXBUFFER;

// {932E6A7E-C68E-45dd-A7BF-53D19C86DB1F}
DEFINE_GUID( IID_ID3DXBuffer,
0x932e6a7e, 0xc68e, 0x45dd, 0xa7, 0xbf, 0x53, 0xd1, 0x9c, 0x86, 0xdb, 0x1f);

#undef INTERFACE
#define INTERFACE ID3DXBuffer

DECLARE_INTERFACE_(ID3DXBuffer, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DXBuffer
    STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
    STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXCreateBuffer( DWORD NumBytes, LPD3DXBUFFER *ppBuffer );

#ifdef __cplusplus
}
#endif

#endif // __D3DX8CORE_H__
