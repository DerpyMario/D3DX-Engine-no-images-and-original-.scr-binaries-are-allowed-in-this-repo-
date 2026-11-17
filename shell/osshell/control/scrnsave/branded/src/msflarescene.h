//-----------------------------------------------------------------------------
// File: msflarescene.h
//
// Desc: CMSFlareScene - the Microsoft flare in front of its banner.  The
//       background of the flare scene (CSolidColorWipe, CBgnd) is already
//       moved here, unseen, so that it is under way when the flare scene is
//       wiped in.  After 3.5 seconds it asks for that wipe.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\msflarescene.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_MSFLARESCENE_H
#define _BRANDED_MSFLARESCENE_H

#include "scene.h"

class CMSFlareScene : public CScene
{
public:
    virtual void    Go( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
};

#if defined(_WIN64)
static_assert( sizeof(CMSFlareScene) == 0x100, "CMSFlareScene layout" );
#endif

#endif // _BRANDED_MSFLARESCENE_H
