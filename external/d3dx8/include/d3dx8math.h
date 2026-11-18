//-----------------------------------------------------------------------------
// File: d3dx8math.h
//
// Desc: The math part of the D3DX 8 utility library, as far as the Direct3D 8
//       screen savers of Windows XP use it.
//
//       This is NOT the Microsoft header.  It is a clean-room replacement
//       written from the documented D3DX 8 interface (type layouts, function
//       names and the formulas given in the DirectX 8 documentation), so
//       that the reconstructed screen savers can be built without the
//       DirectX 8 SDK, which is no longer distributed.  The original screen
//       savers link Microsoft's static d3dx8.lib.  See ../README.md.
//-----------------------------------------------------------------------------
#ifndef __D3DX8MATH_H__
#define __D3DX8MATH_H__

#include <d3d8.h>
#include <math.h>
#include <string.h>

#ifndef D3DXINLINE
#ifdef __cplusplus
#define D3DXINLINE inline
#else
#define D3DXINLINE static __inline
#endif
#endif

//===========================================================================
// General purpose utilities
//===========================================================================
#define D3DX_PI    ((FLOAT)  3.141592654f)
#define D3DX_1BYPI ((FLOAT)  0.318309886f)

#define D3DXToRadian( degree ) ((degree) * (D3DX_PI / 180.0f))
#define D3DXToDegree( radian ) ((radian) * (180.0f / D3DX_PI))


//===========================================================================
// Vectors
//===========================================================================

//--------------------------
// 2D Vector
//--------------------------
typedef struct D3DXVECTOR2
{
#ifdef __cplusplus
public:
    D3DXVECTOR2() {}
    D3DXVECTOR2( CONST FLOAT * pf ) : x(pf[0]), y(pf[1]) {}
    D3DXVECTOR2( FLOAT fx, FLOAT fy ) : x(fx), y(fy) {}

    operator FLOAT* () { return (FLOAT *) &x; }
    operator CONST FLOAT* () const { return (CONST FLOAT *) &x; }

    D3DXVECTOR2& operator += ( CONST D3DXVECTOR2& v ) { x += v.x; y += v.y; return *this; }
    D3DXVECTOR2& operator -= ( CONST D3DXVECTOR2& v ) { x -= v.x; y -= v.y; return *this; }
    D3DXVECTOR2& operator *= ( FLOAT f ) { x *= f; y *= f; return *this; }
    D3DXVECTOR2& operator /= ( FLOAT f ) { FLOAT fInv = 1.0f / f; x *= fInv; y *= fInv; return *this; }

    D3DXVECTOR2 operator + () const { return *this; }
    D3DXVECTOR2 operator - () const { return D3DXVECTOR2(-x, -y); }

    D3DXVECTOR2 operator + ( CONST D3DXVECTOR2& v ) const { return D3DXVECTOR2(x + v.x, y + v.y); }
    D3DXVECTOR2 operator - ( CONST D3DXVECTOR2& v ) const { return D3DXVECTOR2(x - v.x, y - v.y); }
    D3DXVECTOR2 operator * ( FLOAT f ) const { return D3DXVECTOR2(x * f, y * f); }
    D3DXVECTOR2 operator / ( FLOAT f ) const { FLOAT fInv = 1.0f / f; return D3DXVECTOR2(x * fInv, y * fInv); }

    friend D3DXVECTOR2 operator * ( FLOAT f, CONST D3DXVECTOR2& v ) { return D3DXVECTOR2(f * v.x, f * v.y); }

    BOOL operator == ( CONST D3DXVECTOR2& v ) const { return x == v.x && y == v.y; }
    BOOL operator != ( CONST D3DXVECTOR2& v ) const { return x != v.x || y != v.y; }

public:
#endif //__cplusplus
    FLOAT x, y;
} D3DXVECTOR2, *LPD3DXVECTOR2;


//--------------------------
// 3D Vector
//--------------------------
#ifdef __cplusplus
typedef struct D3DXVECTOR3 : public D3DVECTOR
{
public:
    D3DXVECTOR3() {}
    D3DXVECTOR3( CONST FLOAT * pf ) { x = pf[0]; y = pf[1]; z = pf[2]; }
    D3DXVECTOR3( CONST D3DVECTOR& v ) { x = v.x; y = v.y; z = v.z; }
    D3DXVECTOR3( FLOAT fx, FLOAT fy, FLOAT fz ) { x = fx; y = fy; z = fz; }

    operator FLOAT* () { return (FLOAT *) &x; }
    operator CONST FLOAT* () const { return (CONST FLOAT *) &x; }

    D3DXVECTOR3& operator += ( CONST D3DXVECTOR3& v ) { x += v.x; y += v.y; z += v.z; return *this; }
    D3DXVECTOR3& operator -= ( CONST D3DXVECTOR3& v ) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    D3DXVECTOR3& operator *= ( FLOAT f ) { x *= f; y *= f; z *= f; return *this; }
    D3DXVECTOR3& operator /= ( FLOAT f ) { FLOAT fInv = 1.0f / f; x *= fInv; y *= fInv; z *= fInv; return *this; }

    D3DXVECTOR3 operator + () const { return *this; }
    D3DXVECTOR3 operator - () const { return D3DXVECTOR3(-x, -y, -z); }

    D3DXVECTOR3 operator + ( CONST D3DXVECTOR3& v ) const { return D3DXVECTOR3(x + v.x, y + v.y, z + v.z); }
    D3DXVECTOR3 operator - ( CONST D3DXVECTOR3& v ) const { return D3DXVECTOR3(x - v.x, y - v.y, z - v.z); }
    D3DXVECTOR3 operator * ( FLOAT f ) const { return D3DXVECTOR3(x * f, y * f, z * f); }
    D3DXVECTOR3 operator / ( FLOAT f ) const { FLOAT fInv = 1.0f / f; return D3DXVECTOR3(x * fInv, y * fInv, z * fInv); }

    friend D3DXVECTOR3 operator * ( FLOAT f, CONST struct D3DXVECTOR3& v ) { return D3DXVECTOR3(f * v.x, f * v.y, f * v.z); }

    BOOL operator == ( CONST D3DXVECTOR3& v ) const { return x == v.x && y == v.y && z == v.z; }
    BOOL operator != ( CONST D3DXVECTOR3& v ) const { return x != v.x || y != v.y || z != v.z; }
} D3DXVECTOR3, *LPD3DXVECTOR3;
#else
typedef struct _D3DVECTOR D3DXVECTOR3, *LPD3DXVECTOR3;
#endif


//--------------------------
// 4D Vector
//--------------------------
typedef struct D3DXVECTOR4
{
#ifdef __cplusplus
public:
    D3DXVECTOR4() {}
    D3DXVECTOR4( CONST FLOAT* pf ) : x(pf[0]), y(pf[1]), z(pf[2]), w(pf[3]) {}
    D3DXVECTOR4( FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw ) : x(fx), y(fy), z(fz), w(fw) {}

    operator FLOAT* () { return (FLOAT *) &x; }
    operator CONST FLOAT* () const { return (CONST FLOAT *) &x; }

    D3DXVECTOR4 operator + ( CONST D3DXVECTOR4& v ) const { return D3DXVECTOR4(x + v.x, y + v.y, z + v.z, w + v.w); }
    D3DXVECTOR4 operator - ( CONST D3DXVECTOR4& v ) const { return D3DXVECTOR4(x - v.x, y - v.y, z - v.z, w - v.w); }
    D3DXVECTOR4 operator * ( FLOAT f ) const { return D3DXVECTOR4(x * f, y * f, z * f, w * f); }

public:
#endif //__cplusplus
    FLOAT x, y, z, w;
} D3DXVECTOR4, *LPD3DXVECTOR4;


//===========================================================================
// Matrices
//===========================================================================
#ifdef __cplusplus
typedef struct D3DXMATRIX : public D3DMATRIX
{
public:
    D3DXMATRIX() {}
    D3DXMATRIX( CONST FLOAT * pf ) { memcpy( &_11, pf, sizeof(D3DXMATRIX) ); }
    D3DXMATRIX( CONST D3DMATRIX& mat ) { memcpy( &_11, &mat, sizeof(D3DXMATRIX) ); }
    D3DXMATRIX( FLOAT f11, FLOAT f12, FLOAT f13, FLOAT f14,
                FLOAT f21, FLOAT f22, FLOAT f23, FLOAT f24,
                FLOAT f31, FLOAT f32, FLOAT f33, FLOAT f34,
                FLOAT f41, FLOAT f42, FLOAT f43, FLOAT f44 )
    {
        _11 = f11; _12 = f12; _13 = f13; _14 = f14;
        _21 = f21; _22 = f22; _23 = f23; _24 = f24;
        _31 = f31; _32 = f32; _33 = f33; _34 = f34;
        _41 = f41; _42 = f42; _43 = f43; _44 = f44;
    }

    FLOAT& operator () ( UINT iRow, UINT iCol ) { return m[iRow][iCol]; }
    FLOAT  operator () ( UINT iRow, UINT iCol ) const { return m[iRow][iCol]; }

    operator FLOAT* () { return (FLOAT *) &_11; }
    operator CONST FLOAT* () const { return (CONST FLOAT *) &_11; }

    D3DXMATRIX& operator *= ( CONST D3DXMATRIX& mat );
    D3DXMATRIX& operator += ( CONST D3DXMATRIX& mat );
    D3DXMATRIX& operator -= ( CONST D3DXMATRIX& mat );
    D3DXMATRIX& operator *= ( FLOAT f );

    D3DXMATRIX operator * ( CONST D3DXMATRIX& mat ) const;
    D3DXMATRIX operator + ( CONST D3DXMATRIX& mat ) const;
    D3DXMATRIX operator - ( CONST D3DXMATRIX& mat ) const;
    D3DXMATRIX operator * ( FLOAT f ) const;

    BOOL operator == ( CONST D3DXMATRIX& mat ) const { return 0 == memcmp( this, &mat, sizeof(D3DXMATRIX) ); }
    BOOL operator != ( CONST D3DXMATRIX& mat ) const { return 0 != memcmp( this, &mat, sizeof(D3DXMATRIX) ); }
} D3DXMATRIX, *LPD3DXMATRIX;

// A matrix aligned on 16 bytes (D3DX 8.1)
typedef struct _D3DXMATRIXA16 : public D3DXMATRIX
{
    _D3DXMATRIXA16() {}
    _D3DXMATRIXA16( CONST D3DXMATRIX& rhs ) : D3DXMATRIX(rhs) {}
    _D3DXMATRIXA16& operator = ( CONST D3DXMATRIX& rhs ) { memcpy( &_11, &rhs, sizeof(D3DXMATRIX) ); return *this; }
} _D3DXMATRIXA16;

#if defined(_MSC_VER)
#define D3DXMATRIXA16 __declspec(align(16)) _D3DXMATRIXA16
#elif defined(__GNUC__)
#define D3DXMATRIXA16 _D3DXMATRIXA16 __attribute__((aligned(16)))
#else
#define D3DXMATRIXA16 _D3DXMATRIXA16
#endif

#else
typedef struct _D3DMATRIX D3DXMATRIX, *LPD3DXMATRIX;
#endif


//===========================================================================
// Quaternions and planes
//===========================================================================
typedef struct D3DXQUATERNION
{
#ifdef __cplusplus
public:
    D3DXQUATERNION() {}
    D3DXQUATERNION( FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw ) : x(fx), y(fy), z(fz), w(fw) {}
    operator FLOAT* () { return (FLOAT *) &x; }
    operator CONST FLOAT* () const { return (CONST FLOAT *) &x; }
#endif //__cplusplus
    FLOAT x, y, z, w;
} D3DXQUATERNION, *LPD3DXQUATERNION;

typedef struct D3DXPLANE
{
#ifdef __cplusplus
public:
    D3DXPLANE() {}
    D3DXPLANE( FLOAT fa, FLOAT fb, FLOAT fc, FLOAT fd ) : a(fa), b(fb), c(fc), d(fd) {}
    operator FLOAT* () { return (FLOAT *) &a; }
    operator CONST FLOAT* () const { return (CONST FLOAT *) &a; }
#endif //__cplusplus
    FLOAT a, b, c, d;
} D3DXPLANE, *LPD3DXPLANE;


//===========================================================================
// Colors
//===========================================================================
typedef struct D3DXCOLOR
{
#ifdef __cplusplus
public:
    D3DXCOLOR() {}
    D3DXCOLOR( DWORD argb )
    {
        CONST FLOAT f = 1.0f / 255.0f;
        r = f * (FLOAT) (unsigned char) (argb >> 16);
        g = f * (FLOAT) (unsigned char) (argb >>  8);
        b = f * (FLOAT) (unsigned char) (argb >>  0);
        a = f * (FLOAT) (unsigned char) (argb >> 24);
    }
    D3DXCOLOR( CONST D3DCOLORVALUE& c ) : r(c.r), g(c.g), b(c.b), a(c.a) {}
    D3DXCOLOR( FLOAT fr, FLOAT fg, FLOAT fb, FLOAT fa ) : r(fr), g(fg), b(fb), a(fa) {}

    operator DWORD () const
    {
        DWORD dwR = r >= 1.0f ? 0xff : r <= 0.0f ? 0x00 : (DWORD) (r * 255.0f + 0.5f);
        DWORD dwG = g >= 1.0f ? 0xff : g <= 0.0f ? 0x00 : (DWORD) (g * 255.0f + 0.5f);
        DWORD dwB = b >= 1.0f ? 0xff : b <= 0.0f ? 0x00 : (DWORD) (b * 255.0f + 0.5f);
        DWORD dwA = a >= 1.0f ? 0xff : a <= 0.0f ? 0x00 : (DWORD) (a * 255.0f + 0.5f);
        return (dwA << 24) | (dwR << 16) | (dwG << 8) | dwB;
    }
    operator D3DCOLORVALUE& () { return *((D3DCOLORVALUE *) &r); }
    operator CONST D3DCOLORVALUE& () const { return *((CONST D3DCOLORVALUE *) &r); }
#endif //__cplusplus
    FLOAT r, g, b, a;
} D3DXCOLOR, *LPD3DXCOLOR;


//===========================================================================
// D3DX math functions
//===========================================================================

// inline

D3DXINLINE FLOAT D3DXVec2Length( CONST D3DXVECTOR2 *pV );
D3DXINLINE FLOAT D3DXVec2Dot( CONST D3DXVECTOR2 *pV1, CONST D3DXVECTOR2 *pV2 );

D3DXINLINE FLOAT D3DXVec3Length( CONST D3DXVECTOR3 *pV );
D3DXINLINE FLOAT D3DXVec3LengthSq( CONST D3DXVECTOR3 *pV );
D3DXINLINE FLOAT D3DXVec3Dot( CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 );
D3DXINLINE D3DXVECTOR3* D3DXVec3Cross( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 );
D3DXINLINE D3DXVECTOR3* D3DXVec3Add( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 );
D3DXINLINE D3DXVECTOR3* D3DXVec3Subtract( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 );
D3DXINLINE D3DXVECTOR3* D3DXVec3Scale( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, FLOAT s );
D3DXINLINE D3DXVECTOR3* D3DXVec3Lerp( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2, FLOAT s );

D3DXINLINE D3DXMATRIX* D3DXMatrixIdentity( D3DXMATRIX *pOut );
D3DXINLINE BOOL D3DXMatrixIsIdentity( CONST D3DXMATRIX *pM );

// non-inline
#ifdef __cplusplus
extern "C" {
#endif

D3DXVECTOR2* WINAPI D3DXVec2Normalize( D3DXVECTOR2 *pOut, CONST D3DXVECTOR2 *pV );

D3DXVECTOR3* WINAPI D3DXVec3Normalize( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV );
D3DXVECTOR4* WINAPI D3DXVec3Transform( D3DXVECTOR4 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM );
D3DXVECTOR3* WINAPI D3DXVec3TransformCoord( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM );
D3DXVECTOR3* WINAPI D3DXVec3TransformNormal( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM );

FLOAT WINAPI D3DXMatrixfDeterminant( CONST D3DXMATRIX *pM );
D3DXMATRIX* WINAPI D3DXMatrixMultiply( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2 );
D3DXMATRIX* WINAPI D3DXMatrixTranspose( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM );
D3DXMATRIX* WINAPI D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM );
D3DXMATRIX* WINAPI D3DXMatrixScaling( D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz );
D3DXMATRIX* WINAPI D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z );
D3DXMATRIX* WINAPI D3DXMatrixRotationX( D3DXMATRIX *pOut, FLOAT Angle );
D3DXMATRIX* WINAPI D3DXMatrixRotationY( D3DXMATRIX *pOut, FLOAT Angle );
D3DXMATRIX* WINAPI D3DXMatrixRotationZ( D3DXMATRIX *pOut, FLOAT Angle );
D3DXMATRIX* WINAPI D3DXMatrixRotationAxis( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pV, FLOAT Angle );
D3DXMATRIX* WINAPI D3DXMatrixRotationYawPitchRoll( D3DXMATRIX *pOut, FLOAT Yaw, FLOAT Pitch, FLOAT Roll );
D3DXMATRIX* WINAPI D3DXMatrixLookAtRH( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pEye, CONST D3DXVECTOR3 *pAt, CONST D3DXVECTOR3 *pUp );
D3DXMATRIX* WINAPI D3DXMatrixLookAtLH( D3DXMATRIX *pOut, CONST D3DXVECTOR3 *pEye, CONST D3DXVECTOR3 *pAt, CONST D3DXVECTOR3 *pUp );
D3DXMATRIX* WINAPI D3DXMatrixPerspectiveFovLH( D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf );
D3DXMATRIX* WINAPI D3DXMatrixPerspectiveOffCenterLH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf );
D3DXMATRIX* WINAPI D3DXMatrixOrthoLH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf );

#ifdef __cplusplus
}
#endif


//===========================================================================
// Matrix stack
//===========================================================================
typedef interface ID3DXMatrixStack ID3DXMatrixStack;
typedef interface ID3DXMatrixStack *LPD3DXMATRIXSTACK;

// {E3357330-CC5E-11d2-A434-00A0C90629A8}
DEFINE_GUID( IID_ID3DXMatrixStack,
0xe3357330, 0xcc5e, 0x11d2, 0xa4, 0x34, 0x0, 0xa0, 0xc9, 0x6, 0x29, 0xa8);

#undef INTERFACE
#define INTERFACE ID3DXMatrixStack

DECLARE_INTERFACE_(ID3DXMatrixStack, IUnknown)
{
    // IUnknown methods
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    // ID3DXMatrixStack methods
    STDMETHOD(Pop)(THIS) PURE;
    STDMETHOD(Push)(THIS) PURE;
    STDMETHOD(LoadIdentity)(THIS) PURE;
    STDMETHOD(LoadMatrix)(THIS_ CONST D3DXMATRIX* pM ) PURE;
    STDMETHOD(MultMatrix)(THIS_ CONST D3DXMATRIX* pM ) PURE;
    STDMETHOD(MultMatrixLocal)(THIS_ CONST D3DXMATRIX* pM ) PURE;
    STDMETHOD(RotateAxis)(THIS_ CONST D3DXVECTOR3* pV, FLOAT Angle) PURE;
    STDMETHOD(RotateAxisLocal)(THIS_ CONST D3DXVECTOR3* pV, FLOAT Angle) PURE;
    STDMETHOD(RotateYawPitchRoll)(THIS_ FLOAT Yaw, FLOAT Pitch, FLOAT Roll) PURE;
    STDMETHOD(RotateYawPitchRollLocal)(THIS_ FLOAT Yaw, FLOAT Pitch, FLOAT Roll) PURE;
    STDMETHOD(Scale)(THIS_ FLOAT x, FLOAT y, FLOAT z) PURE;
    STDMETHOD(ScaleLocal)(THIS_ FLOAT x, FLOAT y, FLOAT z) PURE;
    STDMETHOD(Translate)(THIS_ FLOAT x, FLOAT y, FLOAT z ) PURE;
    STDMETHOD(TranslateLocal)(THIS_ FLOAT x, FLOAT y, FLOAT z) PURE;
    STDMETHOD_(D3DXMATRIX*, GetTop)(THIS) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK *ppStack );

#ifdef __cplusplus
}
#endif

#include "d3dx8math.inl"

#endif // __D3DX8MATH_H__
