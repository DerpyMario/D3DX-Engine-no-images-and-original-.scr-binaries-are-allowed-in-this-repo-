//-----------------------------------------------------------------------------
// File: toricanim.cpp
//
// Desc: CToricAnim - see toricanim.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\toricanim.obj
//-----------------------------------------------------------------------------
#include "toricanim.h"




//-----------------------------------------------------------------------------
// Name: CToricAnim()
//-----------------------------------------------------------------------------
CToricAnim::CToricAnim()
{
    m_Parms.vCenter = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_Parms.vLookAt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_Parms.vUp = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    m_Parms.fFreq1 = 0.628f;
    m_Parms.fPhase1 = 0.0f;
    m_Parms.fFreq2 = 0.0f;
    m_Parms.fPhase2 = 0.0f;
    m_Parms.fRadius1X = 1.0f;
    m_Parms.fRadius1Z = 1.0f;
    m_Parms.fRadius2X = 0.0f;
    m_Parms.fRadius2Y = 0.0f;

    // nothing to load
    m_eState = STATE_LOADED;
}




//-----------------------------------------------------------------------------
// Name: GetLength()
// Desc: The period of the slower circle.
//-----------------------------------------------------------------------------
FLOAT CToricAnim::GetLength()
{
    FLOAT fFreq1 = m_Parms.fFreq1;
    if( fFreq1 <= 0.0001f )
        fFreq1 = 0.0001f;

    FLOAT fFreq2 = m_Parms.fFreq2;
    if( fFreq2 <= 0.0001f )
        fFreq2 = 0.0001f;

    FLOAT fLength = 1.0f / fFreq1;
    if( fLength <= 1.0f / fFreq2 )
        fLength = 1.0f / fFreq2;

    return fLength;
}




//-----------------------------------------------------------------------------
// Name: OnFrameMove()
// Desc: Returns the camera's world matrix (right, up, direction, position).
//-----------------------------------------------------------------------------
void CToricAnim::OnFrameMove( FLOAT fTime, FLOAT /*fElapsedTime*/, D3DXMATRIX& mOut )
{
    FLOAT fAngle2 = ( ( fTime - m_fStartTime ) * m_Parms.fFreq2 + m_Parms.fPhase2 ) * 6.28f;
    FLOAT fAngle1 = ( ( fTime - m_fStartTime ) * m_Parms.fFreq1 + m_Parms.fPhase1 ) * 6.28f;

    D3DXVECTOR3 vPos = D3DXVECTOR3( cosf( fAngle2 ) * m_Parms.fRadius2X, sinf( fAngle2 ) * m_Parms.fRadius2Y, 0.0f ) +
                       D3DXVECTOR3( cosf( fAngle1 ) * m_Parms.fRadius1X, 0.0f, sinf( fAngle1 ) * m_Parms.fRadius1Z ) +
                       m_Parms.vCenter;

    D3DXVECTOR3 vDir = m_Parms.vLookAt - vPos;
    D3DXVec3Normalize( &vDir, &vDir );

    D3DXVECTOR3 vUp = m_Parms.vUp;
    D3DXVec3Normalize( &vUp, &vUp );

    D3DXVECTOR3 vRight;
    D3DXVec3Cross( &vRight, &vUp, &vDir );
    D3DXVec3Cross( &vUp, &vDir, &vRight );

    mOut._11 = vRight.x;
    mOut._12 = vRight.y;
    mOut._13 = vRight.z;
    mOut._14 = 0.0f;
    mOut._21 = vUp.x;
    mOut._22 = vUp.y;
    mOut._23 = vUp.z;
    mOut._24 = 0.0f;
    mOut._31 = vDir.x;
    mOut._32 = vDir.y;
    mOut._33 = vDir.z;
    mOut._34 = 0.0f;
    mOut._41 = vPos.x;
    mOut._42 = vPos.y;
    mOut._43 = vPos.z;
    mOut._44 = 1.0f;
}
