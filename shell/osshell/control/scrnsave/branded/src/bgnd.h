//-----------------------------------------------------------------------------
// File: bgnd.h
//
// Desc: CBgnd - the background of the flare scene: four aurora bands seen by
//       their own camera.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\bgnd.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_BGND_H
#define _BRANDED_BGND_H

#include "grapobj.h"
#include "camera.h"
#include "aurora.h"

#define BGND_AURORAS    4

class CBgnd : public CGrapObj
{
public:
                    CBgnd();
    virtual         ~CBgnd();

    virtual HRESULT Init();
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
    CAurora*        m_pAuroras;
    UINT            m_cAuroras;

private:
    static AURORAPARMS s_asAuroraParms[BGND_AURORAS];
};

#if defined(_WIN64)
static_assert( sizeof(CBgnd) == 0x108, "CBgnd layout" );
#endif

#endif // _BRANDED_BGND_H
