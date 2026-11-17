//-----------------------------------------------------------------------------
// File: pill.cpp
//
// Desc: CPill - see pill.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\pill.obj
//-----------------------------------------------------------------------------
#include <new>
#include "pill.h"
#include "linearanim.h"




//-----------------------------------------------------------------------------
// Name: CPill()
//-----------------------------------------------------------------------------
CPill::CPill()
{
    m_fWait = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CPill::Init( PILLPARMS* pParms )
{
    HRESULT hr;

    CAnimMesh::Init( &pParms->mesh );
    memcpy( &m_mLocal, &pParms->mLocal, sizeof(D3DXMATRIX) );

    if( pParms->mesh.idAnim != RESID_NONE )
    {
        m_pAnim = new( std::nothrow ) CLinearAnim;

        if( FAILED( hr = m_pAnim->Load( pParms->mesh.idAnim ) ) )
            return hr;

        m_pAnim->m_eLoop = (CAnimation::LoopMode)pParms->mesh.eLoop;
        m_pAnim->m_fDelay = pParms->fDelay;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Setup()
// Desc: Puts the pill back to its initial state (when the scene restarts).
//-----------------------------------------------------------------------------
void CPill::Setup( PILLPARMS* pParms )
{
    memcpy( &m_Parms, &pParms->mesh, sizeof(ANIMMESHPARMS) );
    memcpy( &m_mLocal, &pParms->mLocal, sizeof(D3DXMATRIX) );

    if( m_pAnim != NULL )
    {
        m_pAnim->m_eLoop = (CAnimation::LoopMode)pParms->mesh.eLoop;
        m_pAnim->m_fDelay = pParms->fDelay;
    }
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CPill::Go( FLOAT fTime )
{
    CAnimMesh::Go( fTime );
    m_pAnim->Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CPill::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    CAnimation::State eState = m_pAnim->m_eState;

    if( eState == CAnimation::STATE_PLAYING )
    {
        CAnimMesh::OnFrameMove( fTime, fElapsedTime, mParent );

        // landed: wait a while before the next flight
        if( m_pAnim->m_eState == CAnimation::STATE_DONE )
            m_fWait = RandFloat( 0.25f, 4.0f );
    }
    else if( eState == CAnimation::STATE_DONE )
    {
        m_fWait = m_fWait - fElapsedTime;

        if( m_fWait < 0.0f )
        {
            m_pAnim->Go( fTime );
            OnFrameMove( fTime, fElapsedTime, mParent );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
// Desc: The pill is only visible while it flies.
//-----------------------------------------------------------------------------
void CPill::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    if( m_pAnim->m_eState == CAnimation::STATE_PLAYING && m_pAnim->m_fDelay <= 0.0f )
    {
        D3DXMATRIX mProj = *g_pProjStack->GetTop();
        D3DXMATRIX mView = *g_pViewStack->GetTop();
        D3DXMATRIX mWorldView = m_mWorld * mView;

        m_pEffect->SetTexture( "g_tTexture0", m_ppTextures[0] );
        m_pEffect->SetMatrix( "g_mWorldView", &mWorldView );
        m_pEffect->SetMatrix( "g_mProj", &mProj );

        CAnimMesh::OnFrameRender( fTime, fElapsedTime );
    }
}
