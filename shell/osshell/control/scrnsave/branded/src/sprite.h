//-----------------------------------------------------------------------------
// File: sprite.h
//
// Desc: CSprite - a textured quad that plays a sequence of textures.  The
//       x coordinate of its key frame animation is not a position but the
//       frame number; the effect cross-fades between the two frames around
//       it (g_tTexture0, g_tTexture1, g_fFrac).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\sprite.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_SPRITE_H
#define _BRANDED_SPRITE_H

#include "animmesh.h"

class CSprite : public CAnimMesh
{
public:
    using CAnimMesh::Init;

    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT Init( ANIMMESHPARMS* pParms );
};

#endif // _BRANDED_SPRITE_H
