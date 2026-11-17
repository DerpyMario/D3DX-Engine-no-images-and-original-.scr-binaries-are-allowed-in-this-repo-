//-----------------------------------------------------------------------------
// File: linearanim.cpp
//
// Desc: CLinearAnim - see linearanim.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\linearanim.obj
//-----------------------------------------------------------------------------
#include <stdio.h>
#include "linearanim.h"




//-----------------------------------------------------------------------------
// Name: LoadKey()
//-----------------------------------------------------------------------------
HRESULT CLinearAnim::LoadKey( ANIM_KEY* pKey, int /*cchLine*/, char* pszLine, size_t* /*pcchLine*/ )
{
    UINT nFrame = 0;

    if( m_eType == KEY_ROTPOS )
    {
        if( sscanf_s( pszLine, "%d %f %f %f %f %f %f %f", &nFrame,
                      &pKey->qRot.w, &pKey->qRot.x, &pKey->qRot.y, &pKey->qRot.z,
                      &pKey->vPos.x, &pKey->vPos.y, &pKey->vPos.z ) == 8 )
        {
            // the files hold the inverse rotation
            D3DXQUATERNION q;
            D3DXQuaternionInverse( &q, &pKey->qRot );
            pKey->qRot = q;

            pKey->fTime = (FLOAT)nFrame / ANIM_FRAMES_PER_SECOND;
            return S_OK;
        }
    }
    else if( m_eType == KEY_XFORM )
    {
        D3DXMATRIX m;

        if( sscanf_s( pszLine, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", &nFrame,
                      &m._11, &m._12, &m._13, &m._14,
                      &m._21, &m._22, &m._23, &m._24,
                      &m._31, &m._32, &m._33, &m._34,
                      &m._41, &m._42, &m._43, &m._44 ) == 17 )
        {
            D3DXVECTOR3 vScale;
            D3DXMatrixDecompose( &vScale, &pKey->qRot, &pKey->vPos, &m );

            pKey->fTime = (FLOAT)nFrame / ANIM_FRAMES_PER_SECOND;
            return S_OK;
        }
    }

    return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: DoTween()
//-----------------------------------------------------------------------------
void CLinearAnim::DoTween( ANIM_KEY* pKey0, ANIM_KEY* pKey1, FLOAT fLerp, D3DXQUATERNION& qRot, D3DXVECTOR3& vPos )
{
    D3DXQuaternionSlerp( &qRot, &pKey0->qRot, &pKey1->qRot, fLerp );
    D3DXVec3Lerp( &vPos, &pKey0->vPos, &pKey1->vPos, fLerp );
}
