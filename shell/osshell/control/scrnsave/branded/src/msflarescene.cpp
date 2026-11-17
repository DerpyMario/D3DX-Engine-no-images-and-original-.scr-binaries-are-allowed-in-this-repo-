//-----------------------------------------------------------------------------
// File: msflarescene.cpp
//
// Desc: CMSFlareScene - see msflarescene.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\msflarescene.obj
//-----------------------------------------------------------------------------
#include "msflarescene.h"
#include "ssview.h"




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CMSFlareScene::Go( FLOAT fTime )
{
    CScene::Go( fTime );

    g_pView->m_MSFlare.Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CMSFlareScene::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    CScene::OnFrameMove( fTime, fElapsedTime, mParent );

    if( fTime - m_fStartTime > 3.5f && g_eState == SS_MSFLARE )
        g_eMsg = SSMSG_MSFLARE_DONE;

    g_pView->m_MSFlareBgnd.OnFrameMove( fTime, fElapsedTime, g_mIdent );
    g_pView->m_MSFlare.OnFrameMove( fTime, fElapsedTime, g_mIdent );
    g_pView->m_SolidColorWipe.OnFrameMove( fTime, fElapsedTime, g_mIdent );
    g_pView->m_Bgnd.OnFrameMove( fTime, fElapsedTime, g_mIdent );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CMSFlareScene::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    g_pView->m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0 );

    g_pView->m_MSFlareBgnd.OnFrameRender( fTime, fElapsedTime );
    g_pView->m_MSFlare.OnFrameRender( fTime, fElapsedTime );
}
