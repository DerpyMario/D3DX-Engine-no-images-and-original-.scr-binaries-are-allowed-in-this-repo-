//-----------------------------------------------------------------------------
// File: camera.cpp
//
// Desc: CCamera - see camera.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\camera.obj
//-----------------------------------------------------------------------------
#include "camera.h"
#include "animation.h"
#include "ssview.h"

#define CAMERA_ZNEAR    0.1f
#define CAMERA_ZFAR     300.0f




//-----------------------------------------------------------------------------
// Name: CCamera()
//-----------------------------------------------------------------------------
CCamera::CCamera()
{
    m_fFov = 60.0f;
    m_pAnim = NULL;
    m_vEye = D3DXVECTOR3( 0.0f, 0.0f, -35.0f );
    m_vAt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_vUp = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    m_mAnim = g_mIdent;

    CreateProjection();
    D3DXMatrixLookAtLH( &m_mView, &m_vEye, &m_vAt, &m_vUp );
}




//-----------------------------------------------------------------------------
// Name: ~CCamera()
//-----------------------------------------------------------------------------
CCamera::~CCamera()
{
    if( m_pAnim != NULL )
    {
        delete m_pAnim;
        m_pAnim = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: Setup()
// Desc: Places the camera; any animation is dropped.
//-----------------------------------------------------------------------------
void CCamera::Setup( CAMPARMS* pParms )
{
    m_fFov = pParms->fFov;
    m_vEye = pParms->vEye;
    m_vAt = pParms->vAt;
    m_vUp = pParms->vUp;

    if( m_pAnim != NULL )
    {
        delete m_pAnim;
        m_pAnim = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
//-----------------------------------------------------------------------------
void CCamera::OnFrameMove( FLOAT fTime, FLOAT fElapsedTime, D3DXMATRIX& mParent )
{
    if( m_pAnim != NULL )
    {
        m_pAnim->OnFrameMove( fTime, fElapsedTime, m_mAnim );

        m_vEye = D3DXVECTOR3( m_mAnim._41, m_mAnim._42, m_mAnim._43 );
        m_vUp = D3DXVECTOR3( m_mAnim._21, m_mAnim._22, m_mAnim._23 );
        m_vAt = D3DXVECTOR3( m_mAnim._31 + m_vEye.x, m_mAnim._32 + m_vEye.y, m_mAnim._33 + m_vEye.z );
    }

    D3DXMatrixLookAtLH( &m_mView, &m_vEye, &m_vAt, &m_vUp );
    m_mView = m_mView * mParent;

    CreateProjection();
}




//-----------------------------------------------------------------------------
// Name: CreateProjection()
// Desc: On a screen that is higher than wide the field of view is widened
//       so that the horizontal one becomes 90 degrees.
//-----------------------------------------------------------------------------
void CCamera::CreateProjection()
{
    FLOAT fAspect = 4.0f / 3.0f;

    if( g_pView != NULL )
    {
        fAspect = (FLOAT)g_pView->m_d3dsdBackBuffer.Width / (FLOAT)g_pView->m_d3dsdBackBuffer.Height;

        if( g_pView->m_d3dsdBackBuffer.Width < g_pView->m_d3dsdBackBuffer.Height )
            m_fFov = D3DXToDegree( atanf( 1.0f / fAspect ) ) * 2.0f;
    }

    D3DXMatrixPerspectiveFovLH( &m_mProj, D3DXToRadian( m_fFov ), fAspect, CAMERA_ZNEAR, CAMERA_ZFAR );
}




//-----------------------------------------------------------------------------
// Name: SetView()
//-----------------------------------------------------------------------------
void CCamera::SetView()
{
    g_pProjStack->Push();
    g_pProjStack->LoadMatrix( &m_mProj );
    g_pView->m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_mProj );

    g_pViewStack->Push();
    g_pViewStack->LoadMatrix( &m_mView );
    g_pView->m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_mView );
}




//-----------------------------------------------------------------------------
// Name: RestoreView()
//-----------------------------------------------------------------------------
void CCamera::RestoreView()
{
    g_pProjStack->Pop();
    g_pView->m_pd3dDevice->SetTransform( D3DTS_PROJECTION, g_pProjStack->GetTop() );

    g_pViewStack->Pop();
    g_pView->m_pd3dDevice->SetTransform( D3DTS_VIEW, g_pViewStack->GetTop() );
}
