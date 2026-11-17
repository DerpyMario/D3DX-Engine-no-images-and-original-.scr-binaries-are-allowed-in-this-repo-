//-----------------------------------------------------------------------------
// File: bgnd.cpp
//
// Desc: CBgnd - see bgnd.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\bgnd.obj
//-----------------------------------------------------------------------------
#include <new>
#include "bgnd.h"
#include "toricanim.h"
#include "resource.h"

// (the waves are given as amplitude, frequency and phase speed)
AURORAPARMS CBgnd::s_asAuroraParms[BGND_AURORAS] =
{
    //  wave 0                           wave 1                              wave 2                              scroll        wrap  width  height  yaw   pitch   roll   position                            segments  texture
    { 3.0f, 26.07528f,  0.024271844f, 3.0f, -12.5664f,   -0.032051284f, 3.0f, 18.8496f,   -0.023809524f, 0.017482517f, 1.0f, 50.0f, 200.0f, 0.0f, -30.0f, -3.0f, D3DXVECTOR3( -35.0f, 0.0f, 0.0f ), 1000, IDR_TGA_AURORA },
    { 3.0f, 13.257551f, 0.023809524f, 3.0f, -35.814236f, -0.029411765f, 3.0f, 14.451359f,  0.027777778f, 0.01908397f,  1.0f, 50.0f, 200.0f, 0.0f, -30.0f, -3.0f, D3DXVECTOR3( -10.0f, 0.0f, 0.0f ), 1000, IDR_TGA_AURORA },
    { 3.0f, 26.07528f,  0.025f,       3.0f, -12.5664f,   -0.03125f,     3.0f, 18.8496f,   -0.022727273f, 0.016778523f, 1.0f, 50.0f, 200.0f, 0.0f, -30.0f,  3.0f, D3DXVECTOR3(  15.0f, 0.0f, 0.0f ), 1000, IDR_TGA_AURORA },
    { 3.0f, 13.257551f, 0.023364486f, 3.0f, -35.814236f, -0.030487806f, 3.0f, 14.451359f,  0.02631579f,  0.019379845f, 1.0f, 50.0f, 200.0f, 0.0f, -30.0f,  3.0f, D3DXVECTOR3(  40.0f, 0.0f, 0.0f ), 1000, IDR_TGA_AURORA },
};




//-----------------------------------------------------------------------------
// Name: CBgnd()
//-----------------------------------------------------------------------------
CBgnd::CBgnd()
{
    m_pAuroras = NULL;
    m_cAuroras = BGND_AURORAS;
}




//-----------------------------------------------------------------------------
// Name: ~CBgnd()
//-----------------------------------------------------------------------------
CBgnd::~CBgnd()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CBgnd::Init()
{
    CAMPARMS cam = { 45.0f, D3DXVECTOR3( 5.0f, 0.0f, -25.0f ), D3DXVECTOR3( 0.0f, 10.0f, 0.0f ),
                     D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) };

    m_Camera.Setup( &cam );

    // A "path" that stands still: the camera simply looks at the origin from
    // straight ahead.
    CToricAnim* pToric = new( std::nothrow ) CToricAnim;
    if( pToric != NULL )
    {
        TORICPARMS toric;

        toric.vCenter = D3DXVECTOR3( 0.0f, 0.0f, -75.0f );
        toric.vLookAt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
        toric.vUp = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
        toric.fFreq1 = 0.0f;
        toric.fPhase1 = 0.0f;
        toric.fFreq2 = 0.0f;
        toric.fPhase2 = 0.0f;
        toric.fRadius1X = 0.0f;
        toric.fRadius1Z = 0.0f;
        toric.fRadius2X = 0.0f;
        toric.fRadius2Y = 0.0f;
        memcpy( &pToric->m_Parms, &toric, sizeof(TORICPARMS) );

        if( m_Camera.m_pAnim != NULL )
            delete m_Camera.m_pAnim;
        m_Camera.m_pAnim = pToric;
    }

    m_pAuroras = new( std::nothrow ) CAurora[m_cAuroras];
    if( m_pAuroras != NULL )
    {
        for( UINT i = 0; i < m_cAuroras; i++ )
        {
            CAurora*      pAurora = &m_pAuroras[i];
            ANIMMESHPARMS parms;
            HRESULT       hr;

            memcpy( &pAurora->m_AuroraParms, &s_asAuroraParms[i], sizeof(AURORAPARMS) );

            parms.idAnim = RESID_NONE;
            parms.eLoop = CAnimation::LOOP_ONCE;
            parms.idMesh = RESID_NONE;              // CAurora builds its own
            parms.idEffect = IDR_FX_AURORA;
            parms.idTexture = s_asAuroraParms[i].idTexture;
            parms.cTextures = 1;

            if( FAILED( hr = pAurora->CAnimMesh::Init( &parms ) ) )
                return hr;

            // place and turn the band
            pAurora->m_mLocal._41 = pAurora->m_AuroraParms.vPos.x;
            pAurora->m_mLocal._42 = pAurora->m_AuroraParms.vPos.y;
            pAurora->m_mLocal._43 = pAurora->m_AuroraParms.vPos.z;

            D3DXMATRIX mRot;
            D3DXMatrixRotationYawPitchRoll( &mRot, D3DXToRadian( pAurora->m_AuroraParms.fYaw ),
                                            D3DXToRadian( pAurora->m_AuroraParms.fPitch ),
                                            D3DXToRadian( pAurora->m_AuroraParms.fRoll ) );
            D3DXMatrixMultiply( &pAurora->m_mLocal, &pAurora->m_mLocal, &mRot );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CBgnd::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();

    if( m_pAuroras != NULL )
    {
        delete[] m_pAuroras;
        m_pAuroras = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CBgnd::Go( FLOAT fTime )
{
    if( m_Camera.m_pAnim != NULL )
        m_Camera.m_pAnim->Go( fTime );

    for( UINT i = 0; i < m_cAuroras; i++ )
        m_pAuroras[i].Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CBgnd::Stop( FLOAT fTime )
{
    if( m_Camera.m_pAnim != NULL )
        m_Camera.m_pAnim->Stop( fTime );

    for( UINT i = 0; i < m_cAuroras; i++ )
        m_pAuroras[i].Stop( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CBgnd::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    m_Camera.OnFrameMove( fTime, fElapsedTime, mParent );

    for( UINT i = 0; i < m_cAuroras; i++ )
        m_pAuroras[i].OnFrameMove( fTime, fElapsedTime, mParent );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CBgnd::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    m_Camera.SetView();

    for( UINT i = 0; i < m_cAuroras; i++ )
        m_pAuroras[i].OnFrameRender( fTime, fElapsedTime );

    m_Camera.RestoreView();
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CBgnd::OnCreateDevice()
{
    HRESULT hr;

    if( m_pAuroras != NULL )
    {
        for( UINT i = 0; i < m_cAuroras; i++ )
        {
            if( FAILED( hr = m_pAuroras[i].OnCreateDevice() ) )
                return hr;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
//-----------------------------------------------------------------------------
HRESULT CBgnd::OnResetDevice()
{
    HRESULT hr;

    if( m_pAuroras != NULL )
    {
        for( UINT i = 0; i < m_cAuroras; i++ )
        {
            if( FAILED( hr = m_pAuroras[i].OnResetDevice() ) )
                return hr;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnLostDevice()
//-----------------------------------------------------------------------------
void CBgnd::OnLostDevice()
{
    if( m_pAuroras != NULL )
    {
        for( UINT i = 0; i < m_cAuroras; i++ )
            m_pAuroras[i].OnLostDevice();
    }
}




//-----------------------------------------------------------------------------
// Name: OnDestroyDevice()
//-----------------------------------------------------------------------------
void CBgnd::OnDestroyDevice()
{
    if( m_pAuroras != NULL )
    {
        for( UINT i = 0; i < m_cAuroras; i++ )
            m_pAuroras[i].OnDestroyDevice();
    }
}
