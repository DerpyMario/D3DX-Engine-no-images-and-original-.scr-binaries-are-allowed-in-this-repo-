//-----------------------------------------------------------------------------
// File: animmesh.h
//
// Desc: CAnimMesh - a mesh from an X file resource, drawn with an effect
//       file resource and a set of Targa textures, and optionally moved by a
//       key frame animation.
//
//       The effect has a technique "Render"; with one texture it gets it as
//       g_tTexture0.  The world matrix is m_mAnim * m_mLocal * parent.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\animmesh.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_ANIMMESH_H
#define _BRANDED_ANIMMESH_H

#include "grapobj.h"
#include "animation.h"

#define RESID_NONE      ((DWORD)-1)

struct ANIMMESHPARMS
{
    DWORD           idAnim;         // RT_RCDATA resource of the key frames, or RESID_NONE
    DWORD           eLoop;          // CAnimation::LoopMode
    DWORD           idMesh;         // RT_RCDATA resource of the X file
    DWORD           idEffect;       // RT_RCDATA resource of the effect
    DWORD           idTexture;      // first of cTextures consecutive RT_RCDATA Targa resources
    DWORD           cTextures;
};

class CAnimMesh : public CGrapObj
{
public:
                    CAnimMesh();
    virtual         ~CAnimMesh();

    using CGrapObj::Init;

    virtual void    Reset()                                         {}
    virtual void    Go( FLOAT fTime );
    virtual void    Stop( FLOAT fTime );
    virtual void    OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent );
    virtual void    OnFrameRender( FLOAT fTime, FLOAT fElapsedTime );
    virtual HRESULT OnCreateDevice();
    virtual HRESULT OnResetDevice();
    virtual void    OnLostDevice();
    virtual void    OnDestroyDevice();

    virtual HRESULT Init( ANIMMESHPARMS* pParms );
    virtual CAnimation::State GetState();

protected:
    virtual void    FinalCleanup();

public:
    ID3DXMesh*          m_pMesh;
    CAnimation*         m_pAnim;
    ID3DXEffect*        m_pEffect;
    IDirect3DSurface9*  m_pRenderTarget;    // only CFlare uses it
    D3DXMATRIX          m_mLocal;
    D3DXMATRIX          m_mAnim;
    D3DXMATRIX          m_mWorld;
    ANIMMESHPARMS       m_Parms;
    DWORD               m_dwReserved;       // cleared, never read
    IDirect3DTexture9** m_ppTextures;
};

#if defined(_WIN64)
static_assert( sizeof(ANIMMESHPARMS) == 0x18, "ANIMMESHPARMS layout" );
static_assert( sizeof(CAnimMesh) == 0x110, "CAnimMesh layout" );
#endif

#endif // _BRANDED_ANIMMESH_H
