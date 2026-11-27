//-----------------------------------------------------------------------------
// This code is an example placeholder
//-----------------------------------------------------------------------------
#include "flowfield.h"

//-----------------------------------------------------------------------------
// A cheap, well-mixed integer hash -> float in [-1,1].
//-----------------------------------------------------------------------------
float FlowField::Lattice( int ix, int iy, int iz ) const
{
    unsigned h = m_seed;
    h ^= (unsigned)ix * 0x8da6b343u;
    h ^= (unsigned)iy * 0xd8163841u;
    h ^= (unsigned)iz * 0xcb1ab31fu;
    h *= 0x27d4eb2du;
    h ^= h >> 15;
    // map the top bits to [-1,1]
    return (float)( h >> 8 ) * ( 2.0f / 16777215.0f ) - 1.0f;
}

static inline float Smooth( float t )
{
    return t * t * ( 3.0f - 2.0f * t );
}

//-----------------------------------------------------------------------------
// Trilinearly interpolated value noise.
//-----------------------------------------------------------------------------
float FlowField::Noise( float x, float y, float z ) const
{
    int ix = (int)floorf( x ), iy = (int)floorf( y ), iz = (int)floorf( z );
    float fx = Smooth( x - ix ), fy = Smooth( y - iy ), fz = Smooth( z - iz );

    float c000 = Lattice( ix,     iy,     iz     );
    float c100 = Lattice( ix + 1, iy,     iz     );
    float c010 = Lattice( ix,     iy + 1, iz     );
    float c110 = Lattice( ix + 1, iy + 1, iz     );
    float c001 = Lattice( ix,     iy,     iz + 1 );
    float c101 = Lattice( ix + 1, iy,     iz + 1 );
    float c011 = Lattice( ix,     iy + 1, iz + 1 );
    float c111 = Lattice( ix + 1, iy + 1, iz + 1 );

    float x00 = c000 + fx * ( c100 - c000 );
    float x10 = c010 + fx * ( c110 - c010 );
    float x01 = c001 + fx * ( c101 - c001 );
    float x11 = c011 + fx * ( c111 - c011 );
    float y0  = x00 + fy * ( x10 - x00 );
    float y1  = x01 + fy * ( x11 - x01 );
    return y0 + fz * ( y1 - y0 );
}

//-----------------------------------------------------------------------------
// A vector potential: three decorrelated noise samples, drifting with time.
//-----------------------------------------------------------------------------
Vec3 FlowField::Potential( const Vec3& p, float t ) const
{
    const float s = 0.45f;          // spatial frequency
    float tx = t * 0.05f;
    return Vec3(
        Noise( p.x * s,            p.y * s,            p.z * s + tx          ),
        Noise( p.x * s + 31.3f,    p.y * s - 17.1f,    p.z * s + 11.7f + tx  ),
        Noise( p.x * s - 42.8f,    p.y * s + 25.9f,    p.z * s -  7.4f + tx  ) );
}

//-----------------------------------------------------------------------------
// curl( potential ) by central differences -> a divergence-free flow.  A mild
// inward term keeps the filaments from wandering off to infinity.
//-----------------------------------------------------------------------------
Vec3 FlowField::Flow( const Vec3& p, float t ) const
{
    const float e = 0.1f;
    Vec3 dx( e, 0, 0 ), dy( 0, e, 0 ), dz( 0, 0, e );

    Vec3 px0 = Potential( p - dx, t ), px1 = Potential( p + dx, t );
    Vec3 py0 = Potential( p - dy, t ), py1 = Potential( p + dy, t );
    Vec3 pz0 = Potential( p - dz, t ), pz1 = Potential( p + dz, t );

    float inv = 1.0f / ( 2.0f * e );
    Vec3 curl(
        ( ( py1.z - py0.z ) - ( pz1.y - pz0.y ) ) * inv,
        ( ( pz1.x - pz0.x ) - ( px1.z - px0.z ) ) * inv,
        ( ( px1.y - px0.y ) - ( py1.x - py0.x ) ) * inv );

    return curl - p * 0.08f;        // gentle pull back toward the origin
}
