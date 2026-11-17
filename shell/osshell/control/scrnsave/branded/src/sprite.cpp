//-----------------------------------------------------------------------------
// File: sprite.cpp
//
// Desc: CSprite - see sprite.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\sprite.obj
//-----------------------------------------------------------------------------
#include <new>
#include "sprite.h"
#include "linearanim.h"




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CSprite::Init( ANIMMESHPARMS* pParms )
{
    HRESULT hr = CAnimMesh::Init( pParms );

    if( SUCCEEDED( hr ) && pParms->idAnim != RESID_NONE )
    {
        m_pAnim = new( std::nothrow ) CLinearAnim;

        hr = m_pAnim->Load( pParms->idAnim );
        if( SUCCEEDED( hr ) )
            m_pAnim->m_eLoop = (CAnimation::LoopMode)pParms->eLoop;
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CSprite::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    if( m_pAnim != NULL )
        m_pAnim->OnFrameMove( fTime, fElapsedTime, m_mAnim );

    if( m_pEffect != NULL && m_Parms.cTextures > 1 )
    {
        // the animation only selects the frame
        FLOAT fFrame = m_mAnim._41;
        UINT  iFrame = (UINT)fFrame;
        UINT  iNextFrame = m_Parms.cTextures - 1;

        if( iFrame + 1 < iNextFrame )
            iNextFrame = iFrame + 1;

        m_pEffect->SetTexture( "g_tTexture0", m_ppTextures[iFrame] );
        m_pEffect->SetTexture( "g_tTexture1", m_ppTextures[iNextFrame] );
        m_pEffect->SetFloat( "g_fFrac", fFrame - (FLOAT)iFrame );

        m_mAnim = g_mIdent;
    }

    m_mWorld = m_mAnim * m_mLocal * mParent;
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CSprite::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    D3DXMATRIX mProj = *g_pProjStack->GetTop();
    D3DXMATRIX mView = *g_pViewStack->GetTop();
    D3DXMATRIX mWorldViewProj = m_mWorld * mView * mProj;

    if( m_pEffect != NULL )
        m_pEffect->SetMatrix( "g_mWorldViewProj", &mWorldViewProj );

    CAnimMesh::OnFrameRender( fTime, fElapsedTime );
}
