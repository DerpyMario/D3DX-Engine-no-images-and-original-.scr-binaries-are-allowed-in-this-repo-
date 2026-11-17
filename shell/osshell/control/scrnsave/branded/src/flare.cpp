//-----------------------------------------------------------------------------
// File: flare.cpp
//
// Desc: CFlare - see flare.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\flare.obj
//-----------------------------------------------------------------------------
#include <new>
#include "flare.h"
#include "ssview.h"
#include "resource.h"

#define FLARE_HOME      1.0f, 0.0f, 0.0f, 0.0f,                 \
                        0.0f, 1.0f, 0.0f, 0.0f,                 \
                        0.0f, 0.0f, 1.0f, 0.0f,                 \
                        0.712f, 1.06999993f, 12.084f, 1.0f

#define PILL_MESH       CAnimation::LOOP_ONCE, IDR_X_PILL, IDR_FX_PILL, IDR_TGA_PILL, 1

D3DXMATRIX CFlare::s_mHome( FLARE_HOME );

//                              animation           mesh            delay   local matrix
PILLPARMS CFlare::s_sPill1Parms =
                            { { IDR_ANIM_PILL1,     PILL_MESH },    0.75f,  D3DXMATRIX( FLARE_HOME ) };

PILLPARMS CFlare::s_asPill2Parms[FLARE_PILLS2] =
{
                            { { IDR_ANIM_PILL2_00,  PILL_MESH },    4.0f,   D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_01,  PILL_MESH },    4.5f,   D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_02,  PILL_MESH },    4.2f,   D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_03,  PILL_MESH },    4.7f,   D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_04,  PILL_MESH },    4.4f,   D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_05,  PILL_MESH },    4.9f,   D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_06,  PILL_MESH },    4.55f,  D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_07,  PILL_MESH },    4.15f,  D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_08,  PILL_MESH },    4.75f,  D3DXMATRIX( FLARE_HOME ) },
                            { { IDR_ANIM_PILL2_09,  PILL_MESH },    4.35f,  D3DXMATRIX( FLARE_HOME ) },
};




//-----------------------------------------------------------------------------
// Name: CFlare()
//-----------------------------------------------------------------------------
CFlare::CFlare()
{
    m_pPathAnim = NULL;
    m_pPostProcess = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CFlare()
//-----------------------------------------------------------------------------
CFlare::~CFlare()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CFlare::Init()
{
    HRESULT hr;

    memcpy( &m_mLocal, &s_mHome, sizeof(D3DXMATRIX) );

    ANIMMESHPARMS parms = { RESID_NONE, CAnimation::LOOP_ONCE, IDR_X_FLARE, IDR_FX_FLARE, IDR_TGA_FLARE, 1 };
    if( FAILED( hr = CAnimMesh::Init( &parms ) ) )
        return hr;

    m_pPostProcess = new( std::nothrow ) CPostProcess;

    CAMPARMS cam = { 40.0f, D3DXVECTOR3( 107.0f, 34.0f, 0.0f ), D3DXVECTOR3( 0.0f, 0.0f, 0.0f ),
                     D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) };
    m_Camera.Setup( &cam );

    if( FAILED( hr = m_Pill1.Init( &s_sPill1Parms ) ) )
        return hr;

    for( int i = 0; i < FLARE_PILLS2; i++ )
    {
        if( FAILED( hr = m_aPill2[i].Init( &s_asPill2Parms[i] ) ) )
            return hr;
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Puts the flare, its camera and the pills back to their start.
//-----------------------------------------------------------------------------
void CFlare::Reset()
{
    memcpy( &m_mLocal, &s_mHome, sizeof(D3DXMATRIX) );

    ANIMMESHPARMS parms = { RESID_NONE, CAnimation::LOOP_ONCE, IDR_X_FLARE, IDR_FX_FLARE, IDR_TGA_FLARE, 1 };
    memcpy( &m_Parms, &parms, sizeof(ANIMMESHPARMS) );

    CAMPARMS cam = { 40.0f, D3DXVECTOR3( 107.0f, 34.0f, 0.0f ), D3DXVECTOR3( 0.0f, 0.0f, 0.0f ),
                     D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) };
    m_Camera.Setup( &cam );

    m_Pill1.Setup( &s_sPill1Parms );

    for( int i = 0; i < FLARE_PILLS2; i++ )
        m_aPill2[i].Setup( &s_asPill2Parms[i] );
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CFlare::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();

    if( m_pPostProcess != NULL )
    {
        delete m_pPostProcess;
        m_pPostProcess = NULL;
    }

    if( m_pPathAnim != NULL )
    {
        delete m_pPathAnim;
        m_pPathAnim = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CFlare::Go( FLOAT fTime )
{
    if( m_pAnim != NULL )
        m_pAnim->Go( fTime );

    if( m_Camera.m_pAnim != NULL )
        m_Camera.m_pAnim->Go( fTime );

    m_Pill1.Go( fTime );

    for( int i = 0; i < FLARE_PILLS2; i++ )
        m_aPill2[i].Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CFlare::Stop( FLOAT fTime )
{
    if( m_pAnim != NULL )
        m_pAnim->Stop( fTime );

    if( m_Camera.m_pAnim != NULL )
        m_Camera.m_pAnim->Stop( fTime );

    m_Pill1.Stop( fTime );

    for( int i = 0; i < FLARE_PILLS2; i++ )
        m_aPill2[i].Stop( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CFlare::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    D3DXVECTOR3 vSize1;     // of the big pill
    D3DXVECTOR3 vSize2;     // of the small ones

    m_Camera.OnFrameMove( fTime, fElapsedTime, mParent );
    CAnimMesh::OnFrameMove( fTime, fElapsedTime, mParent );

    if( m_pAnim != NULL )
        m_pAnim->Randomize( 5.0f, 0.1f );

    // the pills are stretched along their flight while the flare flies
    if( g_eState == SS_MSFLARE_TO_FLARE )
    {
        vSize1 = D3DXVECTOR3( 4.0f, 1.0f, 1.0f );
        vSize2 = D3DXVECTOR3( 2.6f, 0.7f, 0.7f );
    }
    else if( g_eState == SS_FLARE || g_eState == SS_FLARE_TO_LOGO )
    {
        vSize1 = D3DXVECTOR3( 2.0f, 0.5f, 0.5f );
        vSize2 = vSize1;
    }
    else
    {
        vSize1 = D3DXVECTOR3( 4.0f, 1.0f, 1.0f );
        vSize2 = vSize1;
    }

    D3DXVECTOR3 vScale1 = vSize1 * 0.75f;
    D3DXVECTOR3 vScale2 = vSize2 * 0.75f;

    m_Pill1.OnFrameMove( fTime, fElapsedTime, mParent );
    if( m_Pill1.m_pEffect != NULL )
        m_Pill1.m_pEffect->SetFloatArray( "g_vScale", (FLOAT*)&vScale1, 3 );

    for( int i = 0; i < FLARE_PILLS2; i++ )
    {
        m_aPill2[i].OnFrameMove( fTime, fElapsedTime, mParent );
        if( m_aPill2[i].m_pEffect != NULL )
            m_aPill2[i].m_pEffect->SetFloatArray( "g_vScale", (FLOAT*)&vScale2, 3 );
    }
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
// Desc: Draws the flare and the pills into the scene texture of the post
//       processing, which then puts them onto the back buffer.
//-----------------------------------------------------------------------------
void CFlare::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    IDirect3DSurface9* pBackBuffer;

    m_Camera.SetView();

    g_pView->m_pd3dDevice->GetRenderTarget( 0, &pBackBuffer );

    m_pRenderTarget = m_pPostProcess->m_pSceneSurface;
    g_pView->m_pd3dDevice->SetRenderTarget( 0, m_pRenderTarget );
    g_pView->m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0 );

    D3DXMATRIX mProj = *g_pProjStack->GetTop();
    D3DXMATRIX mView = *g_pViewStack->GetTop();
    D3DXMATRIX mWorldView = m_mWorld * mView;

    m_pEffect->SetMatrix( "g_mWorldView", &mWorldView );
    m_pEffect->SetMatrix( "g_mProj", &mProj );

    CAnimMesh::OnFrameRender( fTime, fElapsedTime );

    m_Pill1.OnFrameRender( fTime, fElapsedTime );

    for( int i = 0; i < FLARE_PILLS2; i++ )
        m_aPill2[i].OnFrameRender( fTime, fElapsedTime );

    m_pPostProcess->SetBloomScale( 1.65f );
    m_pPostProcess->OnFrameRender( pBackBuffer );

    if( pBackBuffer != NULL )
    {
        pBackBuffer->Release();
        pBackBuffer = NULL;
    }

    m_Camera.RestoreView();
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CFlare::OnCreateDevice()
{
    HRESULT hr;

    if( FAILED( hr = CAnimMesh::OnCreateDevice() ) )
        return hr;

    if( FAILED( hr = m_pPostProcess->OnCreateDevice() ) )
        return hr;

    if( FAILED( hr = m_Pill1.OnCreateDevice() ) )
        return hr;

    for( int i = 0; i < FLARE_PILLS2; i++ )
    {
        if( FAILED( hr = m_aPill2[i].OnCreateDevice() ) )
            return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
//-----------------------------------------------------------------------------
HRESULT CFlare::OnResetDevice()
{
    HRESULT hr;

    m_Camera.CreateProjection();

    if( FAILED( hr = CAnimMesh::OnResetDevice() ) )
        return hr;

    if( FAILED( hr = m_pPostProcess->OnResetDevice() ) )
        return hr;

    if( FAILED( hr = m_Pill1.OnResetDevice() ) )
        return hr;

    for( int i = 0; i < FLARE_PILLS2; i++ )
    {
        if( FAILED( hr = m_aPill2[i].OnResetDevice() ) )
            return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CFlare::OnLostDevice()
{
    CAnimMesh::OnLostDevice();

    if( m_pPostProcess != NULL )
        m_pPostProcess->OnLostDevice();

    m_Pill1.OnLostDevice();

    for( int i = 0; i < FLARE_PILLS2; i++ )
        m_aPill2[i].OnLostDevice();
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CFlare::OnDestroyDevice()
{
    CAnimMesh::OnDestroyDevice();

    if( m_pPostProcess != NULL )
        m_pPostProcess->OnDestroyDevice();

    m_Pill1.OnDestroyDevice();

    for( int i = 0; i < FLARE_PILLS2; i++ )
        m_aPill2[i].OnDestroyDevice();
}
