//-----------------------------------------------------------------------------
// File: msflare.h
//
// Desc: CMSFlare - the Microsoft flare: an animated sprite in front of its
//       own camera.  Identical to CLogo apart from the camera and the
//       pictures (the linker merged the code of the two classes).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\msflare.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_MSFLARE_H
#define _BRANDED_MSFLARE_H

#include "grapobj.h"
#include "camera.h"
#include "sprite.h"

class CMSFlare : public CGrapObj
{
public:
                    CMSFlare();
    virtual         ~CMSFlare();

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
static_assert( sizeof(CMSFlare) == 0x108, "CMSFlare layout" );
#endif

#endif // _BRANDED_MSFLARE_H
