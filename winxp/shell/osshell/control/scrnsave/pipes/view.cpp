//-----------------------------------------------------------------------------
// File: view.cpp
//
// Desc: VIEW: the view of the grid of nodes.
//
//       Reconstructed from view.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\view.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"




//-----------------------------------------------------------------------------
// Name: VIEW()
// Desc:
//-----------------------------------------------------------------------------
VIEW::VIEW()
{
    m_winSize.cy = 0;
    m_zNear = 1.0f;
    m_winSize.cx = 0;
    m_yRot = 0.0f;
    m_bProjMode = TRUE;

    // Grid of 16 x 16 x 16 nodes, 7 units apart, seen from 75 units away
    m_zTrans = -75.0f;
    m_viewDist = 75.0f;
    m_numDiv = 16;
    m_divSize = 7.0f;

    m_fov = PI_OVER_2;
}




//-----------------------------------------------------------------------------
// Name: SetProjMatrix()
// Desc: Sets the view and projection matrices of the device
//-----------------------------------------------------------------------------
void VIEW::SetProjMatrix( IDirect3DDevice8* pd3dDevice )
{
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXVECTOR3 vEye( 0.0f, 0.0f, -m_zTrans );
    D3DXMATRIX matView;
    D3DXMATRIX matProj;

    D3DXMatrixLookAtLH( &matView, &vEye, &vAt, &vUp );
    pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMatrixPerspectiveFovLH( &matProj, m_fov, m_aspectRatio, m_zNear, m_zFar );
    pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
}




//-----------------------------------------------------------------------------
// Name: CalcNodeArraySize()
// Desc: The number of nodes in each direction: m_numDiv - 1 in the longer
//       direction of the window, in proportion in the other one.
//-----------------------------------------------------------------------------
void VIEW::CalcNodeArraySize( IPOINT3D* pNumNodes )
{
    if( m_winSize.cx >= m_winSize.cy )
    {
        pNumNodes->x = m_numDiv - 1;
        pNumNodes->y = (int)( (m_numDiv - 1) / m_aspectRatio );
        if( pNumNodes->y < 1 )
            pNumNodes->y = 1;
        pNumNodes->z = pNumNodes->x;
    }
    else
    {
        pNumNodes->y = m_numDiv - 1;
        pNumNodes->x = (int)( (m_numDiv - 1) * m_aspectRatio );
        if( pNumNodes->x < 1 )
            pNumNodes->x = 1;
        pNumNodes->z = pNumNodes->y;
    }
}




//-----------------------------------------------------------------------------
// Name: SetWinSize()
// Desc: Returns TRUE if the size changed
//-----------------------------------------------------------------------------
BOOL VIEW::SetWinSize( int width, int height )
{
    if( width == m_winSize.cx && height == m_winSize.cy )
        return FALSE;

    m_winSize.cx = width;
    m_winSize.cy = height;

    m_aspectRatio = m_winSize.cy == 0 ? 1.0f : (FLOAT)m_winSize.cx / m_winSize.cy;

    FLOAT size = m_numDiv * m_divSize;
    if( m_winSize.cx < m_winSize.cy )
    {
        m_world.y = size;
        m_world.x = m_aspectRatio * size;
    }
    else
    {
        m_world.x = size;
        m_world.y = size / m_aspectRatio;
    }
    m_world.z = size;

    m_zFar = m_viewDist + m_world.z * 2;

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: IncrementSceneRotation()
// Desc:
//-----------------------------------------------------------------------------
void VIEW::IncrementSceneRotation()
{
    m_yRot += 9.73156f;
    if( m_yRot >= 360.0f )
        m_yRot -= 360.0f;
}
