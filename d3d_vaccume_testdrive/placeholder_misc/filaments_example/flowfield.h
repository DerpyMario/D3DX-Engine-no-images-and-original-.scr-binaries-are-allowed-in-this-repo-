#ifndef FILAMENTS_FLOWFIELD_H
#define FILAMENTS_FLOWFIELD_H

#include "vecmath.h"

class FlowField
{
public:
    explicit FlowField( unsigned seed = 1 ) : m_seed( seed ) {}

    // Scalar value noise in [-1,1].
    float Noise( float x, float y, float z ) const;

    // Divergence-free flow at p and time t (world units per second-ish).
    Vec3 Flow( const Vec3& p, float t ) const;

private:
    float Lattice( int ix, int iy, int iz ) const;
    Vec3  Potential( const Vec3& p, float t ) const;

    unsigned m_seed;
};

#endif // FILAMENTS_FLOWFIELD_H
