//-----------------------------------------------------------------------------
// File: wipe.cpp
//
// Desc: CWipe - see wipe.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\wipe.obj
//-----------------------------------------------------------------------------
#include "wipe.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Name: CWipe()
//-----------------------------------------------------------------------------
CWipe::CWipe()
{
    m_eState = CAnimation::STATE_EMPTY;
    m_fStartTime = 0.0f;
    ZeroMemory( &m_WipeParms, sizeof(m_WipeParms) );
}




//-----------------------------------------------------------------------------
// Name: ~CWipe()
//-----------------------------------------------------------------------------
CWipe::~CWipe()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CWipe::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CWipe::Init()
{
    ANIMMESHPARMS parms = { RESID_NONE, CAnimation::LOOP_ONCE, IDR_X_QUAD, IDR_FX_WIPE, RESID_NONE, 0 };

    return CAnimMesh::Init( &parms );
}




//-----------------------------------------------------------------------------
// Name: Go()
// Desc: Starts (or continues) the wipe, like CAnimation::Go().
//-----------------------------------------------------------------------------
void CWipe::Go( FLOAT fTime )
{
    if( m_pAnim != NULL )
        m_pAnim->Go( fTime );

    switch( m_eState )
    {
        case CAnimation::STATE_STOPPED:
            m_eState = CAnimation::STATE_PLAYING;
            break;

        case CAnimation::STATE_LOADED:
        case CAnimation::STATE_PLAYING:
        case CAnimation::STATE_DONE:
            m_fProgress = 0.0f;
            m_fStartTime = fTime;
            m_eState = CAnimation::STATE_PLAYING;
            break;

        default:
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: Stop()
//-----------------------------------------------------------------------------
void CWipe::Stop( FLOAT fTime )
{
    if( m_pAnim != NULL )
        m_pAnim->Stop( fTime );

    if( m_eState == CAnimation::STATE_PLAYING || m_eState == CAnimation::STATE_DONE )
        m_eState = CAnimation::STATE_STOPPED;
}




//-----------------------------------------------------------------------------
// Name: Setup()
// Desc: Hands the pictures to the effect.
//-----------------------------------------------------------------------------
HRESULT CWipe::Setup()
{
    HRESULT hr;

    if( m_pEffect == NULL )
        return S_OK;

    if( FAILED( hr = m_pEffect->SetFloat( "g_fWidth", m_WipeParms.fWidth ) ) )
        return hr;

    if( FAILED( hr = m_pEffect->SetFloatArray( "g_cColor0", (FLOAT*)&m_WipeParms.cColor0, 3 ) ) )
        return hr;

    if( m_WipeParms.ppSrc0 == NULL )
    {
        if( FAILED( hr = m_pEffect->SetTexture( "g_tSrc0", NULL ) ) )
            return hr;
        hr = m_pEffect->SetBool( "g_bSolidColor0", TRUE );
    }
    else
    {
        if( FAILED( hr = m_pEffect->SetTexture( "g_tSrc0", *m_WipeParms.ppSrc0 ) ) )
            return hr;
        hr = m_pEffect->SetBool( "g_bSolidColor0", FALSE );
    }
    if( FAILED( hr ) )
        return hr;

    if( FAILED( hr = m_pEffect->SetFloatArray( "g_cColor1", (FLOAT*)&m_WipeParms.cColor1, 3 ) ) )
        return hr;

    if( m_WipeParms.ppSrc1 == NULL )
    {
        if( FAILED( hr = m_pEffect->SetTexture( "g_tSrc1", NULL ) ) )
            return hr;
        hr = m_pEffect->SetBool( "g_bSolidColor1", TRUE );
    }
    else
    {
        if( FAILED( hr = m_pEffect->SetTexture( "g_tSrc1", *m_WipeParms.ppSrc1 ) ) )
            return hr;
        hr = m_pEffect->SetBool( "g_bSolidColor1", FALSE );
    }
    if( FAILED( hr ) )
        return hr;

    if( FAILED( hr = m_pEffect->SetTexture( "g_tWipeMap",
                                            m_WipeParms.ppWipeMap != NULL ? *m_WipeParms.ppWipeMap : NULL ) ) )
    {
        return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetState()
//-----------------------------------------------------------------------------
CAnimation::State CWipe::GetState()
{
    return m_eState;
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CWipe::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    CAnimMesh::OnFrameMove( fTime, fElapsedTime, mParent );

    if( m_fProgress >= 1.0f )
        m_eState = CAnimation::STATE_DONE;

    if( m_eState == CAnimation::STATE_STOPPED )
    {
        m_fStartTime = fElapsedTime + m_fStartTime;
    }
    else if( m_eState == CAnimation::STATE_PLAYING || m_eState == CAnimation::STATE_DONE )
    {
        FLOAT t = fTime - m_fStartTime;

        if( m_WipeParms.fDuration < fTime - m_fStartTime )
            t = m_WipeParms.fDuration;

        m_fProgress = t / m_WipeParms.fDuration;
    }

    // the threshold band moves from above the wipe map (1 + width) down to
    // below it (-width)
    m_fLevel1 = ( 1.0f - m_fProgress ) * ( m_WipeParms.fWidth + 1.0f );
    m_fLevel0 = m_fLevel1 - m_WipeParms.fWidth;
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CWipe::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    m_pEffect->SetFloat( "g_fLevel0", m_fLevel0 );
    m_pEffect->SetFloat( "g_fLevel1", m_fLevel1 );

    CAnimMesh::OnFrameRender( fTime, fElapsedTime );
}




//-----------------------------------------------------------------------------
// Name: OnCreateDevice()
//-----------------------------------------------------------------------------
HRESULT CWipe::OnCreateDevice()
{
    HRESULT hr;

    if( FAILED( hr = CAnimMesh::OnCreateDevice() ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OnResetDevice()
//-----------------------------------------------------------------------------
HRESULT CWipe::OnResetDevice()
{
    HRESULT hr;

    if( FAILED( hr = CAnimMesh::OnResetDevice() ) )
        return hr;

    if( FAILED( hr = Setup() ) )
        return hr;

    return S_OK;
}
