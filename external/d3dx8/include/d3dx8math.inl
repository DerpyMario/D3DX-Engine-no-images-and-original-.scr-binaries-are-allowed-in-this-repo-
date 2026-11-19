//-----------------------------------------------------------------------------
// File: d3dx8math.inl
//
// Desc: Inline functions of d3dx8math.h (clean-room replacement, see
//       ../README.md).
//-----------------------------------------------------------------------------
#ifndef __D3DX8MATH_INL__
#define __D3DX8MATH_INL__

#ifdef __cplusplus

//--------------------------
// Matrix operators
//--------------------------
D3DXINLINE D3DXMATRIX& D3DXMATRIX::operator *= ( CONST D3DXMATRIX& mat )
{
    D3DXMatrixMultiply( this, this, &mat );
    return *this;
}

D3DXINLINE D3DXMATRIX& D3DXMATRIX::operator += ( CONST D3DXMATRIX& mat )
{
    for( int i = 0; i < 16; i++ )
        (&_11)[i] += (&mat._11)[i];
    return *this;
}

D3DXINLINE D3DXMATRIX& D3DXMATRIX::operator -= ( CONST D3DXMATRIX& mat )
{
    for( int i = 0; i < 16; i++ )
        (&_11)[i] -= (&mat._11)[i];
    return *this;
}

D3DXINLINE D3DXMATRIX& D3DXMATRIX::operator *= ( FLOAT f )
{
    for( int i = 0; i < 16; i++ )
        (&_11)[i] *= f;
    return *this;
}

D3DXINLINE D3DXMATRIX D3DXMATRIX::operator * ( CONST D3DXMATRIX& mat ) const
{
    D3DXMATRIX matT;
    D3DXMatrixMultiply( &matT, this, &mat );
    return matT;
}

D3DXINLINE D3DXMATRIX D3DXMATRIX::operator + ( CONST D3DXMATRIX& mat ) const
{
    D3DXMATRIX matT = *this;
    matT += mat;
    return matT;
}

D3DXINLINE D3DXMATRIX D3DXMATRIX::operator - ( CONST D3DXMATRIX& mat ) const
{
    D3DXMATRIX matT = *this;
    matT -= mat;
    return matT;
}

D3DXINLINE D3DXMATRIX D3DXMATRIX::operator * ( FLOAT f ) const
{
    D3DXMATRIX matT = *this;
    matT *= f;
    return matT;
}

#endif //__cplusplus


//--------------------------
// 2D Vector
//--------------------------
D3DXINLINE FLOAT D3DXVec2Length( CONST D3DXVECTOR2 *pV )
{
    return sqrtf(pV->x * pV->x + pV->y * pV->y);
}

D3DXINLINE FLOAT D3DXVec2Dot( CONST D3DXVECTOR2 *pV1, CONST D3DXVECTOR2 *pV2 )
{
    return pV1->x * pV2->x + pV1->y * pV2->y;
}


//--------------------------
// 3D Vector
//--------------------------
D3DXINLINE FLOAT D3DXVec3Length( CONST D3DXVECTOR3 *pV )
{
    return sqrtf(pV->x * pV->x + pV->y * pV->y + pV->z * pV->z);
}

D3DXINLINE FLOAT D3DXVec3LengthSq( CONST D3DXVECTOR3 *pV )
{
    return pV->x * pV->x + pV->y * pV->y + pV->z * pV->z;
}

D3DXINLINE FLOAT D3DXVec3Dot( CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z;
}

D3DXINLINE D3DXVECTOR3* D3DXVec3Cross( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    D3DXVECTOR3 v;

    v.x = pV1->y * pV2->z - pV1->z * pV2->y;
    v.y = pV1->z * pV2->x - pV1->x * pV2->z;
    v.z = pV1->x * pV2->y - pV1->y * pV2->x;

    *pOut = v;
    return pOut;
}

D3DXINLINE D3DXVECTOR3* D3DXVec3Add( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    pOut->x = pV1->x + pV2->x;
    pOut->y = pV1->y + pV2->y;
    pOut->z = pV1->z + pV2->z;
    return pOut;
}

D3DXINLINE D3DXVECTOR3* D3DXVec3Subtract( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    pOut->x = pV1->x - pV2->x;
    pOut->y = pV1->y - pV2->y;
    pOut->z = pV1->z - pV2->z;
    return pOut;
}

D3DXINLINE D3DXVECTOR3* D3DXVec3Scale( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, FLOAT s )
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
    return pOut;
}

D3DXINLINE D3DXVECTOR3* D3DXVec3Lerp( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2, FLOAT s )
{
    pOut->x = pV1->x + s * (pV2->x - pV1->x);
    pOut->y = pV1->y + s * (pV2->y - pV1->y);
    pOut->z = pV1->z + s * (pV2->z - pV1->z);
    return pOut;
}


//--------------------------
// Matrix
//--------------------------
D3DXINLINE D3DXMATRIX* D3DXMatrixIdentity( D3DXMATRIX *pOut )
{
    pOut->m[0][1] = pOut->m[0][2] = pOut->m[0][3] =
    pOut->m[1][0] = pOut->m[1][2] = pOut->m[1][3] =
    pOut->m[2][0] = pOut->m[2][1] = pOut->m[2][3] =
    pOut->m[3][0] = pOut->m[3][1] = pOut->m[3][2] = 0.0f;

    pOut->m[0][0] = pOut->m[1][1] = pOut->m[2][2] = pOut->m[3][3] = 1.0f;
    return pOut;
}

D3DXINLINE BOOL D3DXMatrixIsIdentity( CONST D3DXMATRIX *pM )
{
    return pM->m[0][0] == 1.0f && pM->m[0][1] == 0.0f && pM->m[0][2] == 0.0f && pM->m[0][3] == 0.0f &&
           pM->m[1][0] == 0.0f && pM->m[1][1] == 1.0f && pM->m[1][2] == 0.0f && pM->m[1][3] == 0.0f &&
           pM->m[2][0] == 0.0f && pM->m[2][1] == 0.0f && pM->m[2][2] == 1.0f && pM->m[2][3] == 0.0f &&
           pM->m[3][0] == 0.0f && pM->m[3][1] == 0.0f && pM->m[3][2] == 0.0f && pM->m[3][3] == 1.0f;
}

#endif // __D3DX8MATH_INL__
