//-----------------------------------------------------------------------------
// File: flare.h
//
// Desc: CFlare - the "energy" flare: a glowing mesh with pills flying
//       through it, seen by a camera that circles it.  The flare and the
//       pills are drawn into the scene texture of a CPostProcess, which adds
//       the glow ("bloom") and puts the result onto the back buffer.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\flare.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_FLARE_H
#define _BRANDED_FLARE_H

#include "animmesh.h"
#include "camera.h"
#include "pill.h"
#include "postprocess.h"

#define FLARE_PILLS2    10      // the small pills

class CFlare : public CAnimMesh
{
public:
                    CFlare();
    virtual         ~CFlare();

    using CAnimMesh::Init;

    virtual HRESULT Init();
    virtual void    Reset();
    virtual void    Go( FLOAT fTime );
    virtual void    Stop( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT OnCreateDevice();
    virtual HRESULT OnResetDevice();
    virtual void    OnLostDevice();
    virtual void    OnDestroyDevice();

private:
    virtual void    FinalCleanup();

public:
    CCamera         m_Camera;
    CAnimation*     m_pPathAnim;                // never created
    CPostProcess*   m_pPostProcess;
    CPill           m_Pill1;                    // the big pill
    CPill           m_aPill2[FLARE_PILLS2];

private:
    static D3DXMATRIX   s_mHome;                // where the flare is
    static PILLPARMS    s_sPill1Parms;
    static PILLPARMS    s_asPill2Parms[FLARE_PILLS2];
};

#if defined(_WIN64)
static_assert( sizeof(CFlare) == 0xe18, "CFlare layout" );
#endif

#endif // _BRANDED_FLARE_H
