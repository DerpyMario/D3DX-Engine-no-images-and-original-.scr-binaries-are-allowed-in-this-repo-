//-----------------------------------------------------------------------------
// File: animmesh.cpp
//
// Desc: CAnimMesh - see animmesh.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\animmesh.obj
//-----------------------------------------------------------------------------
#include <new>
#include "animmesh.h"
#include "ssview.h"




//-----------------------------------------------------------------------------
// Name: CAnimMesh()
//-----------------------------------------------------------------------------
CAnimMesh::CAnimMesh()
{
    m_pMesh = NULL;
    m_pAnim = NULL;
    m_pEffect = NULL;
    m_pRenderTarget = NULL;
    m_dwReserved = 0;
    m_ppTextures = NULL;

    m_Parms.idAnim = RESID_NONE;
    m_Parms.eLoop = CAnimation::LOOP_ONCE;
    m_Parms.idMesh = RESID_NONE;
    m_Parms.idEffect = RESID_NONE;
    m_Parms.idTexture = RESID_NONE;
    m_Parms.cTextures = 0;

    D3DXMatrixIdentity( &m_mLocal );
    D3DXMatrixIdentity( &m_mAnim );
    D3DXMatrixIdentity( &m_mWorld );
}




//-----------------------------------------------------------------------------
// Name: ~CAnimMesh()
//-----------------------------------------------------------------------------
CAnimMesh::~CAnimMesh()
{
    CAnimMesh::FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CAnimMesh::Init( ANIMMESHPARMS* pParms )
{
    HRESULT hr = S_OK;

    memcpy( &m_Parms, pParms, sizeof(ANIMMESHPARMS) );

    if( m_Parms.cTextures != 0 )
    {
        m_ppTextures = new( std::nothrow ) IDirect3DTexture9*[m_Parms.cTextures];
        if( m_ppTextures == NULL )
        {
            hr = E_OUTOFMEMORY;
        }
        else
        {
            for( UINT i = 0; i < m_Parms.cTextures; i++ )
                m_ppTextures[i] = NULL;
        }
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CAnimMesh::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();

    if( m_pAnim != NULL )
    {
        delete m_pAnim;
        m_pAnim = NULL;
    }

    if( m_ppTextures != NULL )
    {
        delete[] m_ppTextures;
        m_ppTextures = NULL;
    }

    m_dwReserved = 0;
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CAnimMesh::Go( FLOAT fTime )
{
    if( m_pAnim != NULL )
        m_pAnim->Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CAnimMesh::Stop( FLOAT fTime )
{
    if( m_pAnim != NULL )
        m_pAnim->Stop( fTime );
}




//-----------------------------------------------------------------------------
// Name: GetState()
//-----------------------------------------------------------------------------
CAnimation::State CAnimMesh::GetState()
{
    if( m_pAnim != NULL )
        return m_pAnim->m_eState;

    return CAnimation::STATE_EMPTY;
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CAnimMesh::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    if( m_pAnim != NULL )
        m_pAnim->OnFrameMove( fTime, fElapsedTime, m_mAnim );

    m_mWorld = m_mAnim * m_mLocal * mParent;
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CAnimMesh::OnFrameRender( FLOAT /*fTime*/, FLOAT /*fElapsedTime*/ )
{
    if( m_pEffect == NULL )
        return;

    if( m_Parms.cTextures == 1 )
        m_pEffect->SetTexture( "g_tTexture0", m_ppTextures[0] );

    m_pEffect->SetTechnique( "Render" );

    UINT cPasses;
    m_pEffect->Begin( &cPasses, 0 );

    for( UINT iPass = 0; iPass < cPasses; iPass++ )
    {
        m_pEffect->BeginPass( iPass );
        m_pMesh->DrawSubset( 0 );
        m_pEffect->EndPass();
    }

    m_pEffect->End();
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
// Desc: Loads the mesh (optimised for the vertex cache) and the effect.
//-----------------------------------------------------------------------------
HRESULT CAnimMesh::OnCreateDevice()
{
    HRESULT hr;

    if( m_pMesh != NULL )
    {
        m_pMesh->Release();
        m_pMesh = NULL;
    }

    if( LOWORD( m_Parms.idMesh ) == 0 )
    {
        hr = D3DERR_INVALIDCALL;
    }
    else
    {
        hr = D3DXLoadMeshFromXResource( NULL, MAKEINTRESOURCEA( LOWORD( m_Parms.idMesh ) ),
                                        MAKEINTRESOURCEA( 10 )   /* RT_RCDATA */, D3DXMESH_MANAGED,
                                        g_pView->m_pd3dDevice, NULL, NULL, NULL, NULL, &m_pMesh );
    }

    if( FAILED( hr ) )
        return hr;

    DWORD* pAdjacency = new( std::nothrow ) DWORD[m_pMesh->GetNumFaces() * 3];
    if( pAdjacency == NULL )
        return E_OUTOFMEMORY;

    if( FAILED( hr = m_pMesh->GenerateAdjacency( 1e-6f, pAdjacency ) ) )
        return hr;

    if( FAILED( hr = m_pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, pAdjacency, NULL, NULL, NULL ) ) )
        return hr;

    delete[] pAdjacency;

    if( FAILED( hr = D3DXCreateEffectFromResourceW( g_pView->m_pd3dDevice, NULL,
                                                    MAKEINTRESOURCEW( LOWORD( m_Parms.idEffect ) ),
                                                    NULL, NULL, EFFECT_FLAGS, NULL, &m_pEffect, NULL ) ) )
    {
        return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
// Desc: (Re)creates the textures, which live in the default pool.
//-----------------------------------------------------------------------------
HRESULT CAnimMesh::OnResetDevice()
{
    HRESULT hr;

    if( m_pEffect != NULL && FAILED( hr = m_pEffect->OnResetDevice() ) )
        return hr;

    for( UINT i = 0; i < m_Parms.cTextures; i++ )
    {
        if( m_ppTextures[i] != NULL )
        {
            m_ppTextures[i]->Release();
            m_ppTextures[i] = NULL;
        }

        if( FAILED( hr = MSLoadTextureFromResource( g_pView->m_pd3dDevice,
                                                    MAKEINTRESOURCEW( (WORD)( i + m_Parms.idTexture ) ),
                                                    RT_RCDATA, &m_ppTextures[i] ) ) )
        {
            return hr;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CAnimMesh::OnLostDevice()
{
    if( m_pEffect != NULL )
        m_pEffect->OnLostDevice();

    for( UINT i = 0; i < m_Parms.cTextures; i++ )
    {
        if( m_ppTextures[i] != NULL )
        {
            m_ppTextures[i]->Release();
            m_ppTextures[i] = NULL;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CAnimMesh::OnDestroyDevice()
{
    if( m_pMesh != NULL )
    {
        m_pMesh->Release();
        m_pMesh = NULL;
    }

    if( m_pEffect != NULL )
    {
        m_pEffect->Release();
        m_pEffect = NULL;
    }

    for( UINT i = 0; i < m_Parms.cTextures; i++ )
    {
        if( m_ppTextures[i] != NULL )
        {
            m_ppTextures[i]->Release();
            m_ppTextures[i] = NULL;
        }
    }
}
