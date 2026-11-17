//-----------------------------------------------------------------------------
// File: logo.h
//
// Desc: CLogo - the glowing Windows logo: an animated sprite in front of its
//       own camera.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\logo.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_LOGO_H
#define _BRANDED_LOGO_H

#include "grapobj.h"
#include "camera.h"
#include "sprite.h"

class CLogo : public CGrapObj
{
public:
                    CLogo();
    virtual         ~CLogo();

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
    int             m_iReserved;        // never used
    UINT            m_cSprites;         // only the first one is used
    CCamera         m_Camera;
    CSprite*        m_pSprites;

private:
    static ANIMMESHPARMS s_asSParms[1];
};

#if defined(_WIN64)
static_assert( sizeof(CLogo) == 0x108, "CLogo layout" );
#endif

#endif // _BRANDED_LOGO_H
