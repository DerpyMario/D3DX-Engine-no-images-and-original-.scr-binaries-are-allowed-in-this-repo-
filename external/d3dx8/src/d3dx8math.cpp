//-----------------------------------------------------------------------------
// File: d3dx8math.cpp
//
// Desc: Math functions and the matrix stack of the D3DX 8 subset used by the
//       Direct3D 8 screen savers (clean-room implementation of the formulas
//       documented for D3DX; see ../README.md).
//-----------------------------------------------------------------------------
#include <windows.h>
#include <new>
#include "d3dx8math.h"


//===========================================================================
// Vectors
//===========================================================================

D3DXVECTOR2* WINAPI D3DXVec2Normalize( D3DXVECTOR2 *pOut, CONST D3DXVECTOR2 *pV )
{
    FLOAT fLength = D3DXVec2Length( pV );

    if( fLength > 0.0f )
    {
        FLOAT fInv = 1.0f / fLength;
        pOut->x = pV->x * fInv;
        pOut->y = pV->y * fInv;
    }
    else
    {
        pOut->x = pOut->y = 0.0f;
    }
    return pOut;
}

D3DXVECTOR3* WINAPI D3DXVec3Normalize( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV )
{
    FLOAT fLength = D3DXVec3Length( pV );

    if( fLength > 0.0f )
    {
        FLOAT fInv = 1.0f / fLength;
        pOut->x = pV->x * fInv;
        pOut->y = pV->y * fInv;
        pOut->z = pV->z * fInv;
    }
    else
    {
        pOut->x = pOut->y = pOut->z = 0.0f;
    }
    return pOut;
}

// (x, y, z, 1) * M
D3DXVECTOR4* WINAPI D3DXVec3Transform( D3DXVECTOR4 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM )
{
    D3DXVECTOR4 v;

    v.x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31 + pM->_41;
    v.y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32 + pM->_42;
    v.z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33 + pM->_43;
    v.w = pV->x * pM->_14 + pV->y * pM->_24 + pV->z * pM->_34 + pM->_44;

    *pOut = v;
    return pOut;
}

// (x, y, z, 1) * M, projected back into w = 1
D3DXVECTOR3* WINAPI D3DXVec3TransformCoord( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM )
{
    D3DXVECTOR4 v;

    D3DXVec3Transform( &v, pV, pM );
    if( v.w != 0.0f && v.w != 1.0f )
    {
        FLOAT fInv = 1.0f / v.w;
        v.x *= fInv;
        v.y *= fInv;
        v.z *= fInv;
    }
    pOut->x = v.x;
    pOut->y = v.y;
    pOut->z = v.z;
    return pOut;
}

// (x, y, z, 0) * M
D3DXVECTOR3* WINAPI D3DXVec3TransformNormal( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM )
{
    D3DXVECTOR3 v;

    v.x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31;
    v.y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32;
    v.z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33;

    *pOut = v;
    return pOut;
}


//===========================================================================
// Matrices
//===========================================================================

D3DXMATRIX* WINAPI D3DXMatrixMultiply( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2 )
{
    D3DXMATRIX m;

    for( int i = 0; i < 4; i++ )
    {
        for( int j = 0; j < 4; j++ )
        {
            m.m[i][j] = pM1->m[i][0] * pM2->m[0][j] +
                        pM1->m[i][1] * pM2->m[1][j] +
                        pM1->m[i][2] * pM2->m[2][j] +
                        pM1->m[i][3] * pM2->m[3][j];
        }
    }

    *pOut = m;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranspose( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM )
{
    D3DXMATRIX m;

    for( int i = 0; i < 4; i++ )
        for( int j = 0; j < 4; j++ )
            m.m[i][j] = pM->m[j][i];

    *pOut = m;
    return pOut;
}

// Determinant of the 3x3 matrix that remains of M without row r and column c
static FLOAT Minor3( CONST D3DXMATRIX *pM, int r, int c )
{
    FLOAT a[3][3];
    int i, j, ii, jj;

    for( i = 0, ii = 0; i < 4; i++ )
    {
        if( i == r )
            continue;
        for( j = 0, jj = 0; j < 4; j++ )
        {
            if( j == c )
                continue;
            a[ii][jj++] = pM->m[i][j];
        }
        ii++;
    }

    return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
           a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
           a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
}

FLOAT WINAPI D3DXMatrixfDeterminant( CONST D3DXMATRIX *pM )
{
    FLOAT det = 0.0f;

    for( int j = 0; j < 4; j++ )
    {
        FLOAT minor = Minor3( pM, 0, j );
        det += (j & 1) ? -pM->m[0][j] * minor : pM->m[0][j] * minor;
    }
    return det;
}

D3DXMATRIX* WINAPI D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM )
{
    D3DXMATRIX m;
    FLOAT det = D3DXMatrixfDeterminant( pM );

    if( pDeterminant )
        *pDeterminant = det;

    if( det == 0.0f )
        return NULL;

    // Adjugate (transposed cofactors) divided by the determinant
    FLOAT fInv = 1.0f / det;
    for( int i = 0; i < 4; i++ )
    {
        for( int j = 0; j < 4; j++ )
        {
            FLOAT cofactor = Minor3( pM, j, i );
            if( (i + j) & 1 )
                cofactor = -cofactor;
            m.m[i][j] = cofactor * fInv;
        }
    }

    *pOut = m;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixScaling( D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz )
{
    D3DXMatrixIdentity( pOut );
    pOut->_11 = sx;
    pOut->_22 = sy;
    pOut->_33 = sz;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z )
{
    D3DXMatrixIdentity( pOut );
    pOut->_41 = x;
    pOut->_42 = y;
    pOut->_43 = z;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationX( D3DXMATRIX *pOut, FLOAT Angle )
{
    FLOAT s = sinf( Angle );
    FLOAT c = cosf( Angle );

    D3DXMatrixIdentity( pOut );
    pOut->_22 = c;
    pOut->_23 = s;
    pOut->_32 = -s;
    pOut->_33 = c;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationY( D3DXMATRIX *pOut, FLOAT Angle )
{
    FLOAT s = sinf( Angle );
    FLOAT c = cosf( Angle );

    D3DXMatrixIdentity( pOut );
    pOut->_11 = c;
    pOut->_13 = -s;
    pOut->_31 = s;
    pOut->_33 = c;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationZ( D3DXMATRIX *pOut, FLOAT Angle )
{
    FLOAT s = sinf( Angle );
    FLOAT c = cosf( Angle );

    D3DXMatrixIdentity( pOut );
    pOut->_11 = c;
    pOut->_12 = s;
    pOut->_21 = -s;
    pOut->_22 = c;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationAxis( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pV, FLOAT Angle )
{
    D3DXVECTOR3 v;
    D3DXVec3Normalize( &v, pV );

    FLOAT s = sinf( Angle );
    FLOAT c = cosf( Angle );
    FLOAT t = 1.0f - c;

    D3DXMatrixIdentity( pOut );
    pOut->_11 = t * v.x * v.x + c;
    pOut->_12 = t * v.x * v.y + s * v.z;
    pOut->_13 = t * v.x * v.z - s * v.y;
    pOut->_21 = t * v.x * v.y - s * v.z;
    pOut->_22 = t * v.y * v.y + c;
    pOut->_23 = t * v.y * v.z + s * v.x;
    pOut->_31 = t * v.x * v.z + s * v.y;
    pOut->_32 = t * v.y * v.z - s * v.x;
    pOut->_33 = t * v.z * v.z + c;
    return pOut;
}

// Roll about z first, then pitch about x, then yaw about y
D3DXMATRIX* WINAPI D3DXMatrixRotationYawPitchRoll( D3DXMATRIX *pOut, FLOAT Yaw, FLOAT Pitch, FLOAT Roll )
{
    D3DXMATRIX matRoll, matPitch, matYaw;

    D3DXMatrixRotationZ( &matRoll, Roll );
    D3DXMatrixRotationX( &matPitch, Pitch );
    D3DXMatrixRotationY( &matYaw, Yaw );

    D3DXMatrixMultiply( pOut, &matRoll, &matPitch );
    D3DXMatrixMultiply( pOut, pOut, &matYaw );
    return pOut;
}

static D3DXMATRIX* LookAt( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pEye,
                           CONST D3DXVECTOR3 *pUp, D3DXVECTOR3 zaxis )
{
    D3DXVECTOR3 xaxis, yaxis;

    D3DXVec3Normalize( &zaxis, &zaxis );
    D3DXVec3Cross( &xaxis, pUp, &zaxis );
    D3DXVec3Normalize( &xaxis, &xaxis );
    D3DXVec3Cross( &yaxis, &zaxis, &xaxis );

    pOut->_11 = xaxis.x; pOut->_12 = yaxis.x; pOut->_13 = zaxis.x; pOut->_14 = 0.0f;
    pOut->_21 = xaxis.y; pOut->_22 = yaxis.y; pOut->_23 = zaxis.y; pOut->_24 = 0.0f;
    pOut->_31 = xaxis.z; pOut->_32 = yaxis.z; pOut->_33 = zaxis.z; pOut->_34 = 0.0f;
    pOut->_41 = -D3DXVec3Dot( &xaxis, pEye );
    pOut->_42 = -D3DXVec3Dot( &yaxis, pEye );
    pOut->_43 = -D3DXVec3Dot( &zaxis, pEye );
    pOut->_44 = 1.0f;
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixLookAtLH( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pEye, CONST D3DXVECTOR3 *pAt, CONST D3DXVECTOR3 *pUp )
{
    return LookAt( pOut, pEye, pUp, *pAt - *pEye );
}

D3DXMATRIX* WINAPI D3DXMatrixLookAtRH( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pEye, CONST D3DXVECTOR3 *pAt, CONST D3DXVECTOR3 *pUp )
{
    return LookAt( pOut, pEye, pUp, *pEye - *pAt );
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveFovLH( D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf )
{
    FLOAT yScale = 1.0f / tanf( fovy / 2.0f );
    FLOAT xScale = yScale / Aspect;

    ZeroMemory( pOut, sizeof(D3DXMATRIX) );
    pOut->_11 = xScale;
    pOut->_22 = yScale;
    pOut->_33 = zf / (zf - zn);
    pOut->_34 = 1.0f;
    pOut->_43 = -zn * zf / (zf - zn);
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveOffCenterLH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
    ZeroMemory( pOut, sizeof(D3DXMATRIX) );
    pOut->_11 = 2.0f * zn / (r - l);
    pOut->_22 = 2.0f * zn / (t - b);
    pOut->_31 = (l + r) / (l - r);
    pOut->_32 = (t + b) / (b - t);
    pOut->_33 = zf / (zf - zn);
    pOut->_34 = 1.0f;
    pOut->_43 = zn * zf / (zn - zf);
    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoLH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf )
{
    D3DXMatrixIdentity( pOut );
    pOut->_11 = 2.0f / w;
    pOut->_22 = 2.0f / h;
    pOut->_33 = 1.0f / (zf - zn);
    pOut->_43 = zn / (zn - zf);
    return pOut;
}


//===========================================================================
// Matrix stack
//===========================================================================
class CD3DXMatrixStack : public ID3DXMatrixStack
{
    LONG        m_cRef;
    UINT        m_uTop;         // index of the current matrix
    UINT        m_uSize;        // capacity of m_pStack
    D3DXMATRIX* m_pStack;

    D3DXMATRIX& Top() { return m_pStack[m_uTop]; }

public:
    CD3DXMatrixStack() : m_cRef(1), m_uTop(0), m_uSize(0), m_pStack(NULL) {}
    virtual ~CD3DXMatrixStack() { delete [] m_pStack; }

    HRESULT Init()
    {
        m_uSize = 16;
        m_pStack = new (std::nothrow) D3DXMATRIX[m_uSize];
        if( m_pStack == NULL )
            return E_OUTOFMEMORY;
        D3DXMatrixIdentity( &m_pStack[0] );
        return S_OK;
    }

    // IUnknown
    STDMETHOD(QueryInterface)( REFIID riid, LPVOID* ppvObj )
    {
        if( IsEqualIID( riid, IID_IUnknown ) || IsEqualIID( riid, IID_ID3DXMatrixStack ) )
        {
            AddRef();
            *ppvObj = this;
            return S_OK;
        }
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement( &m_cRef ); }
    STDMETHOD_(ULONG, Release)()
    {
        ULONG cRef = InterlockedDecrement( &m_cRef );
        if( cRef == 0 )
            delete this;
        return cRef;
    }

    // ID3DXMatrixStack
    STDMETHOD(Pop)()
    {
        if( m_uTop == 0 )
            return E_FAIL;
        m_uTop--;
        return S_OK;
    }
    STDMETHOD(Push)()
    {
        if( m_uTop + 1 >= m_uSize )
        {
            UINT uSize = m_uSize * 2;
            D3DXMATRIX* pStack = new (std::nothrow) D3DXMATRIX[uSize];
            if( pStack == NULL )
                return E_OUTOFMEMORY;
            memcpy( pStack, m_pStack, sizeof(D3DXMATRIX) * m_uSize );
            delete [] m_pStack;
            m_pStack = pStack;
            m_uSize = uSize;
        }
        m_pStack[m_uTop + 1] = m_pStack[m_uTop];
        m_uTop++;
        return S_OK;
    }
    STDMETHOD(LoadIdentity)() { D3DXMatrixIdentity( &Top() ); return S_OK; }
    STDMETHOD(LoadMatrix)( CONST D3DXMATRIX* pM ) { Top() = *pM; return S_OK; }
    STDMETHOD(MultMatrix)( CONST D3DXMATRIX* pM ) { D3DXMatrixMultiply( &Top(), &Top(), pM ); return S_OK; }
    STDMETHOD(MultMatrixLocal)( CONST D3DXMATRIX* pM ) { D3DXMatrixMultiply( &Top(), pM, &Top() ); return S_OK; }
    STDMETHOD(RotateAxis)( CONST D3DXVECTOR3* pV, FLOAT Angle )
    {
        D3DXMATRIX m;
        D3DXMatrixRotationAxis( &m, pV, Angle );
        return MultMatrix( &m );
    }
    STDMETHOD(RotateAxisLocal)( CONST D3DXVECTOR3* pV, FLOAT Angle )
    {
        D3DXMATRIX m;
        D3DXMatrixRotationAxis( &m, pV, Angle );
        return MultMatrixLocal( &m );
    }
    STDMETHOD(RotateYawPitchRoll)( FLOAT Yaw, FLOAT Pitch, FLOAT Roll )
    {
        D3DXMATRIX m;
        D3DXMatrixRotationYawPitchRoll( &m, Yaw, Pitch, Roll );
        return MultMatrix( &m );
    }
    STDMETHOD(RotateYawPitchRollLocal)( FLOAT Yaw, FLOAT Pitch, FLOAT Roll )
    {
        D3DXMATRIX m;
        D3DXMatrixRotationYawPitchRoll( &m, Yaw, Pitch, Roll );
        return MultMatrixLocal( &m );
    }
    STDMETHOD(Scale)( FLOAT x, FLOAT y, FLOAT z )
    {
        D3DXMATRIX m;
        D3DXMatrixScaling( &m, x, y, z );
        return MultMatrix( &m );
    }
    STDMETHOD(ScaleLocal)( FLOAT x, FLOAT y, FLOAT z )
    {
        D3DXMATRIX m;
        D3DXMatrixScaling( &m, x, y, z );
        return MultMatrixLocal( &m );
    }
    STDMETHOD(Translate)( FLOAT x, FLOAT y, FLOAT z )
    {
        D3DXMATRIX m;
        D3DXMatrixTranslation( &m, x, y, z );
        return MultMatrix( &m );
    }
    STDMETHOD(TranslateLocal)( FLOAT x, FLOAT y, FLOAT z )
    {
        D3DXMATRIX m;
        D3DXMatrixTranslation( &m, x, y, z );
        return MultMatrixLocal( &m );
    }
    STDMETHOD_(D3DXMATRIX*, GetTop)() { return &Top(); }
};

HRESULT WINAPI D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK *ppStack )
{
    if( ppStack == NULL )
        return D3DERR_INVALIDCALL;

    CD3DXMatrixStack* pStack = new (std::nothrow) CD3DXMatrixStack;
    if( pStack == NULL )
        return E_OUTOFMEMORY;

    HRESULT hr = pStack->Init();
    if( FAILED( hr ) )
    {
        pStack->Release();
        return hr;
    }

    *ppStack = pStack;
    return S_OK;
}


// {E3357330-CC5E-11d2-A434-00A0C90629A8}
extern "C" const GUID IID_ID3DXMatrixStack =
    { 0xe3357330, 0xcc5e, 0x11d2, { 0xa4, 0x34, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 } };
