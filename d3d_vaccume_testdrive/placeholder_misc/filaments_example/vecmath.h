//-----------------------------------------------------------------------------
// This binary is an example placeholder
//-----------------------------------------------------------------------------
#ifndef FILAMENTS_VECMATH_H
#define FILAMENTS_VECMATH_H

#include <d3d9.h>
#include <math.h>

struct Vec3
{
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3( float X, float Y, float Z ) : x(X), y(Y), z(Z) {}

    Vec3 operator+( const Vec3& v ) const { return Vec3( x + v.x, y + v.y, z + v.z ); }
    Vec3 operator-( const Vec3& v ) const { return Vec3( x - v.x, y - v.y, z - v.z ); }
    Vec3 operator*( float s )       const { return Vec3( x * s, y * s, z * s ); }
};

inline float Dot( const Vec3& a, const Vec3& b )
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross( const Vec3& a, const Vec3& b )
{
    return Vec3( a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x );
}

inline float Length( const Vec3& v )
{
    return sqrtf( Dot( v, v ) );
}

inline Vec3 Normalize( const Vec3& v )
{
    float len = Length( v );
    if( len < 1e-8f )
        return Vec3( 0, 0, 0 );
    return v * ( 1.0f / len );
}


//-----------------------------------------------------------------------------
// Matrices (D3DMATRIX is row-major; D3D multiplies row-vector * matrix)
//-----------------------------------------------------------------------------
inline D3DMATRIX MatIdentity()
{
    D3DMATRIX m = {};
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

inline D3DMATRIX MatPerspectiveFovLH( float fovY, float aspect, float zn, float zf )
{
    float yScale = 1.0f / tanf( fovY * 0.5f );
    float xScale = yScale / aspect;

    D3DMATRIX m = {};
    m._11 = xScale;
    m._22 = yScale;
    m._33 = zf / ( zf - zn );
    m._34 = 1.0f;
    m._43 = -zn * zf / ( zf - zn );
    return m;
}

inline D3DMATRIX MatLookAtLH( const Vec3& eye, const Vec3& at, const Vec3& up )
{
    Vec3 zaxis = Normalize( at - eye );
    Vec3 xaxis = Normalize( Cross( up, zaxis ) );
    Vec3 yaxis = Cross( zaxis, xaxis );

    D3DMATRIX m = {};
    m._11 = xaxis.x; m._12 = yaxis.x; m._13 = zaxis.x; m._14 = 0.0f;
    m._21 = xaxis.y; m._22 = yaxis.y; m._23 = zaxis.y; m._24 = 0.0f;
    m._31 = xaxis.z; m._32 = yaxis.z; m._33 = zaxis.z; m._34 = 0.0f;
    m._41 = -Dot( xaxis, eye );
    m._42 = -Dot( yaxis, eye );
    m._43 = -Dot( zaxis, eye );
    m._44 = 1.0f;
    return m;
}


//-----------------------------------------------------------------------------
// HSV (h in [0,1)) to a 0xAARRGGBB colour, alpha forced opaque
//-----------------------------------------------------------------------------
inline D3DCOLOR HsvToColor( float h, float s, float v, float scale )
{
    h -= floorf( h );
    float r = v, g = v, b = v;
    float i = floorf( h * 6.0f );
    float f = h * 6.0f - i;
    float p = v * ( 1.0f - s );
    float q = v * ( 1.0f - s * f );
    float t = v * ( 1.0f - s * ( 1.0f - f ) );

    switch( (int)i % 6 )
    {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    int ri = (int)( r * scale * 255.0f );
    int gi = (int)( g * scale * 255.0f );
    int bi = (int)( b * scale * 255.0f );
    ri = ri < 0 ? 0 : ( ri > 255 ? 255 : ri );
    gi = gi < 0 ? 0 : ( gi > 255 ? 255 : gi );
    bi = bi < 0 ? 0 : ( bi > 255 ? 255 : bi );
    return D3DCOLOR_ARGB( 255, ri, gi, bi );
}

#endif // FILAMENTS_VECMATH_H
