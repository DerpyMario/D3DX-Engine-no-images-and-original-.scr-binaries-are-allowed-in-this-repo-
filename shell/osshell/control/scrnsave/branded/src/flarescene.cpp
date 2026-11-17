//-----------------------------------------------------------------------------
// File: flarescene.cpp
//
// Desc: CFlareScene - see flarescene.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\flarescene.obj
//-----------------------------------------------------------------------------
#include <new>
#include "flarescene.h"
#include "toricanim.h"
#include "ssview.h"




//-----------------------------------------------------------------------------
// Name: CFlareScene()
//-----------------------------------------------------------------------------
CFlareScene::CFlareScene()
{
    m_fHoldTime = 0.0f;
    m_fEndTime = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: ~CFlareScene()
//-----------------------------------------------------------------------------
CFlareScene::~CFlareScene()
{
    FinalCleanup();
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
//-----------------------------------------------------------------------------
void CFlareScene::FinalCleanup()
{
    OnLostDevice();
    OnDestroyDevice();
}




//-----------------------------------------------------------------------------
// Name: SetupFlight()
// Desc: Gives the flare's camera a new path around the flare, and chooses
//       how long the flare stays.
//-----------------------------------------------------------------------------
void CFlareScene::SetupFlight()
{
    CToricAnim* pToric = new( std::nothrow ) CToricAnim;
    if( pToric != NULL )
    {
        TORICPARMS toric;

        toric.vCenter = D3DXVECTOR3( 8.75f, 0.0f, 0.0f );
        toric.vLookAt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
        toric.vUp = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
        toric.fFreq1 = 1.0f / 41.0f;
        toric.fPhase1 = 0.0f;
        toric.fFreq2 = 1.0f / 14.0f;
        toric.fPhase2 = 0.0f;
        toric.fRadius1X = 4.125f;
        toric.fRadius1Z = 5.0f;
        toric.fRadius2X = 4.125f;
        toric.fRadius2Y = 6.0f;
        memcpy( &pToric->m_Parms, &toric, sizeof(TORICPARMS) );

        if( g_pView->m_Flare.m_Camera.m_pAnim != NULL )
            delete g_pView->m_Flare.m_Camera.m_pAnim;
        g_pView->m_Flare.m_Camera.m_pAnim = pToric;
    }

    // four to five minutes
    m_fHoldTime = RandFloat( 0.0f, 60.0f ) + 240.0f;
    m_fEndTime = m_fHoldTime + FLARE_FLY_OUT_TIME;
}




//-----------------------------------------------------------------------------
// Name: Init()
//-----------------------------------------------------------------------------
HRESULT CFlareScene::Init()
{
    SetupFlight();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Gets the scene ready for the next round.
//-----------------------------------------------------------------------------
void CFlareScene::Reset()
{
    g_pView->m_Flare.Reset();

    SetupFlight();
}




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CFlareScene::Go( FLOAT fTime )
{
    CScene::Go( fTime );

    g_pView->m_Bgnd.Go( fTime );
    g_pView->m_Flare.Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CFlareScene::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    static D3DXMATRIX s_mOffset = g_mIdent;     // moves the flare in and out

    FLOAT t = fTime - m_fStartTime;

    switch( g_eState )
    {
        case SS_MSFLARE_TO_FLARE:
        case SS_FLARE_IN:
            // fly in fast, then slow down
            D3DXMatrixTranslation( &s_mOffset,
                                   ( 1.0f - ExpLerp( t, 0.0f, FLARE_FLY_IN_TIME, 1.0f / 300.0f ) ) * FLARE_FLY_DISTANCE,
                                   0.0f, 0.0f );
            if( t > FLARE_FLY_IN_TIME )
                g_eState = SS_FLARE;
            break;

        case SS_FLARE:
            if( t > m_fHoldTime )
                g_eState = SS_FLARE_OUT;
            break;

        case SS_FLARE_OUT:
            // start slowly, then fly out fast
            D3DXMatrixTranslation( &s_mOffset,
                                   ExpLerp( t, m_fHoldTime, m_fEndTime, 300.0f ) * FLARE_FLY_DISTANCE,
                                   0.0f, 0.0f );
            if( t > m_fEndTime )
                g_eMsg = SSMSG_FLARE_DONE;
            break;

        default:
            break;
    }

    CScene::OnFrameMove( fTime, fElapsedTime, mParent );

    g_pView->m_SolidColorWipe.OnFrameMove( fTime, fElapsedTime, g_mIdent );
    g_pView->m_Bgnd.OnFrameMove( fTime, fElapsedTime, g_mIdent );
    g_pView->m_Flare.OnFrameMove( fTime, fElapsedTime, s_mOffset );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CFlareScene::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    g_pView->m_SolidColorWipe.OnFrameRender( fTime, fElapsedTime );
    g_pView->m_Bgnd.OnFrameRender( fTime, fElapsedTime );
    g_pView->m_Flare.OnFrameRender( fTime, fElapsedTime );
}
