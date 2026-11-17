//-----------------------------------------------------------------------------
// File: crosswipescene.h
//
// Desc: CCrossWipeScene - a wipe from one scene to another.  Both scenes go
//       on moving; each one is drawn into a render target texture of its
//       own, and CWipe blends the two along a wipe map.  A missing scene is
//       replaced by a solid colour.  When the wipe is done, the scene posts
//       its message (CROSSWIPEPARMS::eMsg).
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\crosswipescene.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_CROSSWIPESCENE_H
#define _BRANDED_CROSSWIPESCENE_H

#include "scene.h"
#include "wipe.h"

struct CROSSWIPEPARMS
{
    FLOAT       fDuration;
    FLOAT       fWidth;
    DWORD       idWipeMap;      // Targa resource, or RESID_NONE
    CGrapObj*   pSrc0;          // from (NULL: cColor0)
    CGrapObj*   pSrc1;          // to (NULL: cColor1)
    D3DXCOLOR   cColor0;
    D3DXCOLOR   cColor1;
    SSMsg       eMsg;           // posted when the wipe is done
};

class CCrossWipeScene : public CScene
{
public:
                    CCrossWipeScene();
    virtual         ~CCrossWipeScene();

    virtual HRESULT Init();
    virtual void    Go( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT OnCreateDevice();
    virtual HRESULT OnResetDevice();
    virtual void    OnLostDevice();
    virtual void    OnDestroyDevice();

    void            Setup( CROSSWIPEPARMS* pParms );
    void            CreateResources();

private:
    virtual void    FinalCleanup();

public:
    CWipe               m_Wipe;
    CROSSWIPEPARMS      m_CWParms;
    WIPEPARMS           m_WipeParms;        // handed to m_Wipe by Go()
    IDirect3DTexture9*  m_pWipeMap;
    IDirect3DTexture9*  m_pSrc0Texture;     // render target for pSrc0
    D3DXCOLOR           m_cSrc0Color;       // (never used)
    IDirect3DTexture9*  m_pSrc1Texture;     // render target for pSrc1
    D3DXCOLOR           m_cSrc1Color;       // (never used)
};

#if defined(_WIN64)
static_assert( sizeof(CROSSWIPEPARMS) == 0x48, "CROSSWIPEPARMS layout" );
static_assert( sizeof(CCrossWipeScene) == 0x328, "CCrossWipeScene layout" );
#endif

#endif // _BRANDED_CROSSWIPESCENE_H
