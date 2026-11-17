//-----------------------------------------------------------------------------
// File: wipe.h
//
// Desc: CWipe - a full screen transition from one picture (a texture or a
//       solid colour) to another, along a "wipe map": the pixels change over
//       when the wipe map passes the moving threshold (Wipe.fx).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\wipe.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_WIPE_H
#define _BRANDED_WIPE_H

#include "animmesh.h"

struct WIPEPARMS
{
    FLOAT               fDuration;      // seconds
    FLOAT               fWidth;         // of the soft edge, in wipe map units
    IDirect3DTexture9** ppSrc0;         // NULL: cColor0
    IDirect3DTexture9** ppSrc1;         // NULL: cColor1
    IDirect3DTexture9** ppWipeMap;
    D3DXCOLOR           cColor0;
    D3DXCOLOR           cColor1;
};

class CWipe : public CAnimMesh
{
public:
                    CWipe();
    virtual         ~CWipe();

    using CAnimMesh::Init;

    virtual HRESULT Init();
    virtual void    Go( FLOAT fTime );
    virtual void    Stop( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT OnCreateDevice();
    virtual HRESULT OnResetDevice();
    virtual CAnimation::State GetState();

    HRESULT         Setup();

private:
    virtual void    FinalCleanup();

public:
    WIPEPARMS           m_WipeParms;
    CAnimation::State   m_eState;       // (the same states as an animation)
    FLOAT               m_fUnused;
    FLOAT               m_fProgress;    // 0 .. 1
    FLOAT               m_fStartTime;
    FLOAT               m_fLevel0;      // the threshold band, for the effect
    FLOAT               m_fLevel1;
};

#if defined(_WIN64)
static_assert( sizeof(WIPEPARMS) == 0x40, "WIPEPARMS layout" );
static_assert( sizeof(CWipe) == 0x168, "CWipe layout" );
#endif

#endif // _BRANDED_WIPE_H
