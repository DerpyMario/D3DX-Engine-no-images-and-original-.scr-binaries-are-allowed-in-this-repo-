//-----------------------------------------------------------------------------
// File: logoscene.cpp
//
// Desc: CLogoScene - see logoscene.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\logoscene.obj
//-----------------------------------------------------------------------------
#include "logoscene.h"
#include "ssview.h"




//-----------------------------------------------------------------------------
// Name: Go()
//-----------------------------------------------------------------------------
void CLogoScene::Go( FLOAT fTime )
{
    CScene::Go( fTime );

    g_pView->m_Logo.Go( fTime );
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CLogoScene::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    CScene::OnFrameMove( fTime, fElapsedTime, mParent );

    if( fTime - m_fStartTime > 2.1 && g_eState == SS_LOGO )
        g_eMsg = SSMSG_LOGO_DONE;

    g_pView->m_Logo.OnFrameMove( fTime, fElapsedTime, g_mIdent );
}




//-----------------------------------------------------------------------------
// Name: OnFrameRender()
//-----------------------------------------------------------------------------
void CLogoScene::OnFrameRender( FLOAT fTime, FLOAT fElapsedTime )
{
    g_pView->m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0 );

    g_pView->m_Logo.OnFrameRender( fTime, fElapsedTime );
}
