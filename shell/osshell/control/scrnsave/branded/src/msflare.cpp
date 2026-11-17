//-----------------------------------------------------------------------------
// File: msflare.cpp
//
// Desc: CMSFlare - see msflare.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\msflare.obj
//-----------------------------------------------------------------------------
#include <new>
#include "msflare.h"
#include "resource.h"

ANIMMESHPARMS CMSFlare::s_asSParms[1] =
{
    { IDR_ANIM_MSFLARE, CAnimation::LOOP_ONCE, IDR_X_QUAD, IDR_FX_LOGOGLOW, IDR_TGA_MSFLARE_FIRST, IDR_TGA_MSFLARE_COUNT },
};




//-----------------------------------------------------------------------------
// Name: CMSFlare()
//-----------------------------------------------------------------------------
CMSFlare::CMSFlare()
{
    m_cSprites = 1;
    m_pSprites = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CMSFlare()
//-----------------------------------------------------------------------------
CMSFlare::~CMSFlare()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CMSFlare::Init()
{
    CAMPARMS cam = { 45.0f, D3DXVECTOR3( 4.0f, 1.3f, -10.5f ), D3DXVECTOR3( 0.8f, 0.4f, 0.0f ),
                     D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) };

    m_Camera.Setup( &cam );

    m_pSprites = new( std::nothrow ) CSprite[m_cSprites];
    if( m_pSprites == NULL )
        return E_OUTOFMEMORY;

    m_pSprites->Init( s_asSParms );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CMSFlare::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();

    if( m_pSprites != NULL )
    {
        delete[] m_pSprites;
        m_pSprites = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CMSFlare::Go( FLOAT fTime )
{
    m_pSprites->Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CMSFlare::Stop( FLOAT fTime )
{
    m_pSprites->Stop( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CMSFlare::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    D3DXMATRIX m;

    m_Camera.OnFrameMove( fTime, fElapsedTime, mParent );

    D3DXMatrixTranslation( &m, 0.0f, 0.0f, -5.0f );
    m = m * mParent;

    m_pSprites->OnFrameMove( fTime, fElapsedTime, m );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CMSFlare::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    m_Camera.SetView();
    m_pSprites->OnFrameRender( fTime, fElapsedTime );
    m_Camera.RestoreView();
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CMSFlare::OnCreateDevice()
{
    HRESULT hr;

    if( m_pSprites != NULL && FAILED( hr = m_pSprites->OnCreateDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
//-----------------------------------------------------------------------------
HRESULT CMSFlare::OnResetDevice()
{
    HRESULT hr;

    m_Camera.CreateProjection();

    if( m_pSprites != NULL && FAILED( hr = m_pSprites->OnResetDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CMSFlare::OnLostDevice()
{
    if( m_pSprites != NULL )
        m_pSprites->OnLostDevice();
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CMSFlare::OnDestroyDevice()
{
    if( m_pSprites != NULL )
        m_pSprites->OnDestroyDevice();
}
