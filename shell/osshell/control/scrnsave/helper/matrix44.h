//-----------------------------------------------------------------------------
// File: matrix44.h
//
// Desc: Minimal 4x4 float matrix.  Layout-compatible with D3DMATRIX, which is
//       how the screen savers hand it to IDirect3DDevice9::SetTransform().
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\matrix44.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_MATRIX44_H
#define _HELPER_MATRIX44_H

class Matrix44f
{
public:
    float m[4][4];

    void Identity();
};

#endif // _HELPER_MATRIX44_H
