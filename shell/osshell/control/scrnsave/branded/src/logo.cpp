//-----------------------------------------------------------------------------
// File: logo.cpp
//
// Desc: CLogo - see logo.h.  (CMSFlare in msflare.cpp is the same code with
//       another camera and other pictures.)
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\logo.obj
//-----------------------------------------------------------------------------
#include <new>
#include "logo.h"
#include "resource.h"

ANIMMESHPARMS CLogo::s_asSParms[1] =
{
    { IDR_ANIM_LOGO, CAnimation::LOOP_ONCE, IDR_X_QUAD, IDR_FX_LOGOGLOW, IDR_TGA_LOGO_FIRST, IDR_TGA_LOGO_COUNT },
};




//-----------------------------------------------------------------------------
// Name: CLogo()
//-----------------------------------------------------------------------------
CLogo::CLogo()
{
    m_cSprites = 1;
    m_pSprites = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CLogo()
//-----------------------------------------------------------------------------
CLogo::~CLogo()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CLogo::Init()
{
    CAMPARMS cam = { 45.0f, D3DXVECTOR3( 0.0f, 0.0f, -15.0f ), D3DXVECTOR3( 0.0f, 0.0f, 0.0f ),
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
void CLogo::FinalCleanup()
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
void CLogo::Go( FLOAT fTime )
{
    m_pSprites->Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CLogo::Stop( FLOAT fTime )
{
    m_pSprites->Stop( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CLogo::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
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
void CLogo::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    m_Camera.SetView();
    m_pSprites->OnFrameRender( fTime, fElapsedTime );
    m_Camera.RestoreView();
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CLogo::OnCreateDevice()
{
    HRESULT hr;

    if( m_pSprites != NULL && FAILED( hr = m_pSprites->OnCreateDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
//-----------------------------------------------------------------------------
HRESULT CLogo::OnResetDevice()
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
void CLogo::OnLostDevice()
{
    if( m_pSprites != NULL )
        m_pSprites->OnLostDevice();
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CLogo::OnDestroyDevice()
{
    if( m_pSprites != NULL )
        m_pSprites->OnDestroyDevice();
}
