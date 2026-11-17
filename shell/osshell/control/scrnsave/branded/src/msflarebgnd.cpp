//-----------------------------------------------------------------------------
// File: msflarebgnd.cpp
//
// Desc: CMSFlareBgnd - see msflarebgnd.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\msflarebgnd.obj
//-----------------------------------------------------------------------------
#include "msflarebgnd.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CMSFlareBgnd::Init()
{
    ANIMMESHPARMS parms = { RESID_NONE, CAnimation::LOOP_ONCE, IDR_X_QUAD, IDR_FX_FSBANNER, IDR_TGA_MSFLAREBGND, 1 };

    return CAnimMesh::Init( &parms );
}
