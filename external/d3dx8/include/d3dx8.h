//-----------------------------------------------------------------------------
// File: d3dx8.h
//
// Desc: The subset of the D3DX 8 utility library that the Direct3D 8 screen
//       savers of Windows XP use.
//
//       This is NOT the Microsoft header: it is a clean-room replacement
//       written from the documented D3DX 8 interface, so that the
//       reconstructed screen savers can be built without the DirectX 8 SDK.
//       See ../README.md.
//-----------------------------------------------------------------------------
#ifndef __D3DX8_H__
#define __D3DX8_H__

#include <d3d8.h>
#include <limits.h>

#ifndef D3DX_DEFAULT
#define D3DX_DEFAULT ULONG_MAX
#endif

#include "d3dx8math.h"
#include "d3dx8core.h"
#include "d3dx8mesh.h"
#include "d3dx8tex.h"

// Errors
#define _FACDD  0x876
#define MAKE_DDHRESULT( code )  MAKE_HRESULT( 1, _FACDD, code )

enum _D3DXERR {
    D3DXERR_CANNOTMODIFYINDEXBUFFER     = MAKE_DDHRESULT(2900),
    D3DXERR_INVALIDMESH                 = MAKE_DDHRESULT(2901),
    D3DXERR_CANNOTATTRSORT              = MAKE_DDHRESULT(2902),
    D3DXERR_SKINNINGNOTSUPPORTED        = MAKE_DDHRESULT(2903),
    D3DXERR_TOOMANYINFLUENCES           = MAKE_DDHRESULT(2904),
    D3DXERR_INVALIDDATA                 = MAKE_DDHRESULT(2905),
    D3DXERR_LOADEDMESHASNODATA          = MAKE_DDHRESULT(2906),
};

#endif // __D3DX8_H__
