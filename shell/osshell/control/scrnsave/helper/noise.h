//-----------------------------------------------------------------------------
// File: noise.h
//
// Desc: Gradient ("improved Perlin") noise.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\noise.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_NOISE_H
#define _HELPER_NOISE_H

#include "point.h"

// Gradient noise in the range (-1, 1); zero at every integer lattice point.
float __cdecl Noise( const Point3f& p );

// Fractal sum of nOctaves of noise:  sum( Noise( p * 2^i ) / 2^i ).
// With bAbsolute set, the absolute value of every octave is summed instead
// ("turbulence" in Perlin's sense).
float __cdecl Turbulence( const Point3f& p, int nOctaves, bool bAbsolute );

#endif // _HELPER_NOISE_H
