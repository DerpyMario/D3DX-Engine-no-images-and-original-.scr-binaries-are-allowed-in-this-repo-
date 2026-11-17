//-----------------------------------------------------------------------------
// File: flarescene.h
//
// Desc: CFlareScene - the "energy" flare with its pills, in front of the
//       auroras and the slowly changing background colour.  The flare flies
//       in from the side, stays for four to five minutes while the camera
//       circles it, and flies out again; then the scene asks for the wipe
//       back to the logo.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\flarescene.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_FLARESCENE_H
#define _BRANDED_FLARESCENE_H

#include "scene.h"

#define FLARE_FLY_IN_TIME       6.0f    // seconds
#define FLARE_FLY_OUT_TIME      4.0f
#define FLARE_FLY_DISTANCE      100.0f

class CFlareScene : public CScene
{
public:
                    CFlareScene();
    virtual         ~CFlareScene();

    virtual HRESULT Init();
    virtual void    Reset();
    virtual void    Go( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );

private:
    virtual void    FinalCleanup();

    void            SetupFlight();

public:
    FLOAT           m_fHoldTime;        // scene time at which the flare starts to fly out
    FLOAT           m_fEndTime;         // ... and is gone
};

#if defined(_WIN64)
static_assert( sizeof(CFlareScene) == 0x108, "CFlareScene layout" );
#endif

#endif // _BRANDED_FLARESCENE_H
