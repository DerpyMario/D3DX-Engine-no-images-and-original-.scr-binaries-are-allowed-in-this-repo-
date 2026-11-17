//-----------------------------------------------------------------------------
// File: aurora.cpp
//
// Desc: CAurora - see aurora.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\aurora.obj
//-----------------------------------------------------------------------------
#include "aurora.h"
#include "ssview.h"

struct AURORAVERTEX
{
    FLOAT x, y, z;
    FLOAT u, v;
};

#define D3DFVF_AURORAVERTEX     ( D3DFVF_XYZ | D3DFVF_TEX1 )




//-----------------------------------------------------------------------------
// Name: CAurora()
//-----------------------------------------------------------------------------
CAurora::CAurora()
{
    m_AuroraParms.idTexture = RESID_NONE;
    m_AuroraParms.fWrap = 1.0f;
    m_AuroraParms.fWidth = 1.0f;
    m_AuroraParms.fHeight = 1.0f;
    m_AuroraParms.fAmp0 = 0.0f;
    m_AuroraParms.fFreq0 = 0.0f;
    m_AuroraParms.fSpeed0 = 0.0f;
    m_AuroraParms.fAmp1 = 0.0f;
    m_AuroraParms.fFreq1 = 0.0f;
    m_AuroraParms.fSpeed1 = 0.0f;
    m_AuroraParms.fAmp2 = 0.0f;
    m_AuroraParms.fFreq2 = 0.0f;
    m_AuroraParms.fSpeed2 = 0.0f;
    m_AuroraParms.fScroll = 0.0f;
    m_AuroraParms.fYaw = 0.0f;
    m_AuroraParms.fPitch = 0.0f;
    m_AuroraParms.fRoll = 0.0f;
    m_AuroraParms.cSegments = 100;
    m_AuroraParms.vPos = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: ~CAurora()
//-----------------------------------------------------------------------------
CAurora::~CAurora()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CAurora::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();
}




//-----------------------------------------------------------------------------
// Name: LoadMesh()
// Desc: Builds the strip: cSegments quads from x = -0.5 to 0.5, y = -0.5 to
//       0.5, with the texture once along it.
//-----------------------------------------------------------------------------
HRESULT CAurora::LoadMesh()
{
    HRESULT       hr;
    UINT          cSegments = m_AuroraParms.cSegments;
    UINT          cFaces = cSegments * 2;
    AURORAVERTEX* pVertices;
    DWORD*        pIndices;
    DWORD*        pAttributes;
    UINT          i;

    m_cVertices = cSegments * 6;
    m_cIndices = cSegments * 6;

    if( m_pMesh != NULL )
    {
        m_pMesh->Release();
        m_pMesh = NULL;
    }

    if( FAILED( hr = D3DXCreateMeshFVF( cFaces, m_cVertices, D3DXMESH_MANAGED | D3DXMESH_32BIT,
                                        D3DFVF_AURORAVERTEX, g_pView->m_pd3dDevice, &m_pMesh ) ) )
    {
        return hr;
    }

    if( FAILED( hr = m_pMesh->LockVertexBuffer( 0, (void**)&pVertices ) ) )
        return hr;

    for( i = 0; i < m_AuroraParms.cSegments; i++, pVertices += 6 )
    {
        FLOAT fSegments = (FLOAT)m_AuroraParms.cSegments;
        FLOAT u0 = (FLOAT)i / fSegments;
        FLOAT u1 = ( i == m_AuroraParms.cSegments - 1 ) ? 1.0f : (FLOAT)( i + 1 ) / fSegments;
        FLOAT x0 = u0 + -0.5f;
        FLOAT x1 = u1 + -0.5f;

        // (the texture coordinate of the last edge is not clamped like its position)
        u1 = (FLOAT)( i + 1 ) / fSegments;

        pVertices[0].x = x0;  pVertices[0].y = -0.5f;  pVertices[0].z = 0.0f;  pVertices[0].u = u0;  pVertices[0].v = 1.0f;
        pVertices[1].x = x0;  pVertices[1].y = 0.5f;   pVertices[1].z = 0.0f;  pVertices[1].u = u0;  pVertices[1].v = 0.0f;
        pVertices[2].x = x1;  pVertices[2].y = 0.5f;   pVertices[2].z = 0.0f;  pVertices[2].u = u1;  pVertices[2].v = 0.0f;
        pVertices[3].x = x1;  pVertices[3].y = 0.5f;   pVertices[3].z = 0.0f;  pVertices[3].u = u1;  pVertices[3].v = 0.0f;
        pVertices[4].x = x1;  pVertices[4].y = -0.5f;  pVertices[4].z = 0.0f;  pVertices[4].u = u1;  pVertices[4].v = 1.0f;
        pVertices[5].x = x0;  pVertices[5].y = -0.5f;  pVertices[5].z = 0.0f;  pVertices[5].u = u0;  pVertices[5].v = 1.0f;
    }

    m_pMesh->UnlockVertexBuffer();

    if( FAILED( hr = m_pMesh->LockIndexBuffer( 0, (void**)&pIndices ) ) )
        return hr;

    for( i = 0; i < m_cIndices; i++ )
        pIndices[i] = i;

    m_pMesh->UnlockIndexBuffer();

    if( FAILED( hr = m_pMesh->LockAttributeBuffer( 0, &pAttributes ) ) )
        return hr;

    for( i = 0; i < cFaces; i++ )
        pAttributes[i] = 0;

    m_pMesh->UnlockAttributeBuffer();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
// Desc: Scrolls the texture and advances the waves.
//-----------------------------------------------------------------------------
void CAurora::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    static bool   s_bFirstFrame = true;
    AURORAVERTEX* pVertices;
    UINT          i, j;

    if( s_bFirstFrame )
    {
        m_fStartTime = fTime;
        s_bFirstFrame = false;
    }

    CAnimMesh::OnFrameMove( fTime, fElapsedTime, mParent );

    m_cVertices = m_AuroraParms.cSegments * 6;

    m_pMesh->LockVertexBuffer( 0, (void**)&pVertices );

    for( i = 0; i < m_AuroraParms.cSegments; i++ )
    {
        for( j = 0; j < 6; j++ )
            pVertices[i * 6 + j].u = fElapsedTime * m_AuroraParms.fScroll + pVertices[i * 6 + j].u;
    }

    // keep the coordinates small
    FLOAT fWrap = m_AuroraParms.fWrap * 2.0f;
    FLOAT fShift;

    if( pVertices[0].u < -fWrap )
        fShift = fWrap;
    else if( pVertices[0].u > fWrap )
        fShift = -fWrap;
    else
        fShift = 0.0f;

    if( fShift != 0.0f )
    {
        for( i = 0; i < m_AuroraParms.cSegments; i++ )
        {
            for( j = 0; j < 6; j++ )
                pVertices[i * 6 + j].u = fShift + pVertices[i * 6 + j].u;
        }
    }

    m_pMesh->UnlockVertexBuffer();

    m_pEffect->SetFloat( "g_fPhase0", fTime * m_AuroraParms.fSpeed0 );
    m_pEffect->SetFloat( "g_fPhase1", fTime * m_AuroraParms.fSpeed1 );
    m_pEffect->SetFloat( "g_fPhase2", fTime * m_AuroraParms.fSpeed2 );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CAurora::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    D3DXMATRIX mProj = *g_pProjStack->GetTop();
    D3DXMATRIX mView = *g_pViewStack->GetTop();
    D3DXMATRIX mWorldViewProj = m_mWorld * mView * mProj;

    m_pEffect->SetMatrix( "g_mWorldViewProj", &mWorldViewProj );

    CAnimMesh::OnFrameRender( fTime, fElapsedTime );
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CAurora::OnCreateDevice()
{
    HRESULT hr;

    if( FAILED( hr = LoadMesh() ) )
        return hr;

    if( FAILED( hr = D3DXCreateEffectFromResourceW( g_pView->m_pd3dDevice, NULL,
                                                    MAKEINTRESOURCEW( LOWORD( m_Parms.idEffect ) ),
                                                    NULL, NULL, EFFECT_FLAGS, NULL, &m_pEffect, NULL ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fAmp0", m_AuroraParms.fAmp0 ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fFreq0", m_AuroraParms.fFreq0 ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fAmp1", m_AuroraParms.fAmp1 ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fFreq1", m_AuroraParms.fFreq1 ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fAmp2", m_AuroraParms.fAmp2 ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fFreq2", m_AuroraParms.fFreq2 ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fWid", m_AuroraParms.fWidth ) ) ||
        FAILED( hr = m_pEffect->SetFloat( "g_fHgt", m_AuroraParms.fHeight ) ) )
    {
        return hr;
    }

    return S_OK;
}
