//-----------------------------------------------------------------------------
// File: shape3d.h
//
// Desc: Public interface of Shape3D.lib: builds an extruded 3D mesh from a
//       string, in the manner of D3DXCreateText().
//
//       The library is a private copy of the D3DX text mesh code (which in
//       turn descends from the wglUseFontOutlines() implementation of
//       OpenGL), placed in the namespace Shape3D.
//
//       In ssText3d.scr the entry point was inlined into its only caller, so
//       its name is not recorded in the symbol file; the worker it forwards
//       to is called D3DXCreateTextAW there.  CreateTextW is the name chosen
//       by this reconstruction.
//-----------------------------------------------------------------------------
#ifndef _SHAPE3D_H
#define _SHAPE3D_H

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>

namespace Shape3D
{
    //-------------------------------------------------------------------------
    // Name: CreateTextW()
    // Desc: Creates a mesh (D3DFVF_XYZ | D3DFVF_NORMAL, 16 bit indices) that
    //       contains the text in the font selected into hDC.  One unit is the
    //       height of the font's em square; the text starts at the origin,
    //       runs along +x and is extruded from z = 0 to z = -Extrusion.
    //
    //       Deviation       maximum chordal deviation from the true outlines
    //                       (0 selects one font design unit)
    //       ppAdjacency     must be NULL (see fontoutl.cpp)
    //       pGlyphMetrics   optional, one entry per character
    //-------------------------------------------------------------------------
    HRESULT CreateTextW( LPDIRECT3DDEVICE9 pDevice, HDC hDC, LPCWSTR pText, FLOAT Deviation,
                         FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                         LPGLYPHMETRICSFLOAT pGlyphMetrics );
}

#endif // _SHAPE3D_H
