//-----------------------------------------------------------------------------
// File: logoscene.h
//
// Desc: CLogoScene - the glowing Windows logo on black.  After 2.1 seconds
//       it asks for the wipe to the Microsoft flare scene.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\logoscene.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_LOGOSCENE_H
#define _BRANDED_LOGOSCENE_H

#include "scene.h"

class CLogoScene : public CScene
{
public:
    virtual void    Go( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
};

#if defined(_WIN64)
static_assert( sizeof(CLogoScene) == 0x100, "CLogoScene layout" );
#endif

#endif // _BRANDED_LOGOSCENE_H
