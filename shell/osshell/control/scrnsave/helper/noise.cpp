//-----------------------------------------------------------------------------
// File: noise.cpp
//
// Desc: Gradient ("improved Perlin") noise.
//
//       This is Ken Perlin's 2002 reference algorithm (quintic fade curve,
//       twelve edge gradients selected by the low four hash bits) with three
//       local quirks that are reproduced here exactly as shipped:
//
//         * the permutation is hashed in Z, Y, X order,
//         * the permutation table is not Perlin's but a table of its own,
//         * gradient cases 12..15 are (x+y, -x+y, -y+z, -y-z).
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\noise.obj
//-----------------------------------------------------------------------------
#include <math.h>
#include "noise.h"

// 256-entry permutation of 0..255 (stored as ints in the image)
static const int s_Permutation[256] =
{
    225, 155, 210, 108, 175, 199, 221, 144, 203, 116,  70, 213,  69, 158,  33, 252,
      5,  82, 173, 133, 222, 139, 174,  27,   9,  71,  90, 246,  75, 130,  91, 191,
    169, 138,   2, 151, 194, 235,  81,   7,  25, 113, 228, 159, 205, 253, 134, 142,
    248,  65, 224, 217,  22, 121, 229,  63,  89, 103,  96, 104, 156,  17, 201, 129,
     36,   8, 165, 110, 237, 117, 231,  56, 132, 211, 152,  20, 181, 111, 239, 218,
    170, 163,  51, 172, 157,  47,  80, 212, 176, 250,  87,  49,  99, 242, 136, 189,
    162, 115,  44,  43, 124,  94, 150,  16, 141, 247,  32,  10, 198, 223, 255,  72,
     53, 131,  84,  57, 220, 197,  58,  50, 208,  11, 241,  28,   3, 192,  62, 202,
     18, 215, 153,  24,  76,  41,  15, 179,  39,  46,  55,   6, 128, 167,  23, 188,
    106,  34, 187, 140, 164,  73, 112, 182, 244, 195, 227,  13,  35,  77, 196, 185,
     26, 200, 226, 119,  31, 123, 168, 125, 249,  68, 183, 230, 177, 135, 160, 180,
     12,   1, 243, 148, 102, 166,  38, 238, 251,  37, 240, 126,  64,  74, 161,  40,
    184, 149, 171, 178, 101,  66,  29,  59, 146,  61, 254, 107,  42,  86, 154,   4,
    236, 232, 120,  21, 233, 209,  45,  98, 193, 114,  78,  19, 206,  14, 118, 127,
     48,  79, 147,  85,  30, 207, 219,  54,  88, 234, 190, 122,  95,  67, 143, 109,
    137, 214, 145,  93,  92, 100, 245,   0, 216, 186,  60,  83, 105,  97, 204,  52,
};




//-----------------------------------------------------------------------------
// Name: Fade()
// Desc: 6t^5 - 15t^4 + 10t^3
//-----------------------------------------------------------------------------
static inline float Fade( float t )
{
    return t * t * t * ( t * t * 6.0f - t * 15.0f + 10.0f );
}




//-----------------------------------------------------------------------------
// Name: Lerp()
//-----------------------------------------------------------------------------
static inline float Lerp( float t, float a, float b )
{
    return ( 1.0f - t ) * a + b * t;
}




//-----------------------------------------------------------------------------
// Name: Floor()
// Desc: Integer floor without calling into the CRT.
//-----------------------------------------------------------------------------
static inline int Floor( float f )
{
    int i = (int)f;
    if( f < 0.0f )
        i--;
    return i;
}




//-----------------------------------------------------------------------------
// Name: GDot()
// Desc: Dot product of (x, y, z) with one of the gradient directions.
//-----------------------------------------------------------------------------
static float GDot( float x, float y, float z, int nHash )
{
    switch( nHash )
    {
        case  0: return  x + y;
        case  1: return -x + y;
        case  2: return  x - y;
        case  3: return -x - y;
        case  4: return  x + z;
        case  5: return -x + z;
        case  6: return  x - z;
        case  7: return -x - z;
        case  8: return  y + z;
        case  9: return -y + z;
        case 10: return  y - z;
        case 11: return -y - z;
        case 12: return  x + y;
        case 13: return -x + y;
        case 14: return -y + z;
        case 15: return -y - z;
    }
    return 0.0f;
}




//-----------------------------------------------------------------------------
// Name: Noise()
//-----------------------------------------------------------------------------
float __cdecl Noise( const Point3f& p )
{
    const int* perm = s_Permutation;

    // Unit cube that contains the point
    int X = Floor( p.x );
    int Y = Floor( p.y );
    int Z = Floor( p.z );

    // Relative position of the point inside that cube
    float x = p.x - (float)X;
    float y = p.y - (float)Y;
    float z = p.z - (float)Z;

    // Hash the coordinates of the eight cube corners
    int A  = perm[Z & 255] + Y;
    int AA = perm[A & 255] + X;
    int AB = perm[( A + 1 ) & 255] + X;
    int B  = perm[( Z + 1 ) & 255] + Y;
    int BA = perm[B & 255] + X;
    int BB = perm[( B + 1 ) & 255] + X;

    float g000 = GDot( x,        y,        z,        perm[AA & 255] & 15 );
    float g100 = GDot( x - 1.0f, y,        z,        perm[( AA + 1 ) & 255] & 15 );
    float g010 = GDot( x,        y - 1.0f, z,        perm[AB & 255] & 15 );
    float g110 = GDot( x - 1.0f, y - 1.0f, z,        perm[( AB + 1 ) & 255] & 15 );
    float g001 = GDot( x,        y,        z - 1.0f, perm[BA & 255] & 15 );
    float g101 = GDot( x - 1.0f, y,        z - 1.0f, perm[( BA + 1 ) & 255] & 15 );
    float g011 = GDot( x,        y - 1.0f, z - 1.0f, perm[BB & 255] & 15 );
    float g111 = GDot( x - 1.0f, y - 1.0f, z - 1.0f, perm[( BB + 1 ) & 255] & 15 );

    // Blend the eight contributions
    float u = Fade( x );
    float v = Fade( y );
    float w = Fade( z );

    return Lerp( w, Lerp( v, Lerp( u, g000, g100 ),
                             Lerp( u, g010, g110 ) ),
                    Lerp( v, Lerp( u, g001, g101 ),
                             Lerp( u, g011, g111 ) ) );
}




//-----------------------------------------------------------------------------
// Name: Turbulence()
//-----------------------------------------------------------------------------
float __cdecl Turbulence( const Point3f& p, int nOctaves, bool bAbsolute )
{
    float fSum = 0.0f;
    float fScale = 1.0f;

    if( bAbsolute )
    {
        for( int i = 0; i < nOctaves; i++ )
        {
            fSum += fabsf( Noise( p * fScale ) / fScale );
            fScale *= 2.0f;
        }
    }
    else
    {
        for( int i = 0; i < nOctaves; i++ )
        {
            fSum += Noise( p * fScale ) / fScale;
            fScale *= 2.0f;
        }
    }

    return fSum;
}
