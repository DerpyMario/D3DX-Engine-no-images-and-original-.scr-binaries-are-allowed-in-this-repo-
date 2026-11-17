//-----------------------------------------------------------------------------
// File: scene.h
//
// Desc: CScene - base of the scenes of the show (logo, Microsoft flare,
//       flare and the cross wipes between them).  A scene has a camera and
//       remembers when it was started; the graphic objects it shows are
//       members of the current view (g_pView).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\scene.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_SCENE_H
#define _BRANDED_SCENE_H

#include "grapobj.h"
#include "camera.h"

class CScene : public CGrapObj
{
public:
                    CScene();
    virtual         ~CScene();

    virtual void    Go( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual HRESULT OnResetDevice();

private:
    virtual void    FinalCleanup();

public:
    CCamera         m_Camera;
    FLOAT           m_fStartTime;       // -1: not started yet
};

#if defined(_WIN64)
static_assert( sizeof(CScene) == 0x100, "CScene layout" );
#endif

#endif // _BRANDED_SCENE_H
