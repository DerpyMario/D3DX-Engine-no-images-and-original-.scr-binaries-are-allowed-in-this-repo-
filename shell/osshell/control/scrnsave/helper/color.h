//-----------------------------------------------------------------------------
// File: color.h
//
// Desc: Colour helpers.
//
//       Original object:
//           shell\osshell\control\scrnsave\helper\objfre\amd64\color.obj
//-----------------------------------------------------------------------------
#ifndef _HELPER_COLOR_H
#define _HELPER_COLOR_H

#include <windows.h>

// One channel of the standard HSL -> RGB conversion.  v1 / v2 are the two
// intermediate values derived from lightness and saturation, vH is the hue
// (shifted by +1/3, 0 or -1/3 for red, green and blue).
float __cdecl HueToRGB( float v1, float v2, float vH );

//-----------------------------------------------------------------------------
// Inline helpers.  These never appear as functions of their own in the shipped
// images (they were always expanded inline), so their names are not known.
//-----------------------------------------------------------------------------

// Converts a 0..1 float channel to a byte the way the screen savers do it:
// multiply by 256 and saturate.
inline BYTE ColorFloatToByte( float f )
{
    if( f <= 0.0f )
        return 0;
    if( f >= 1.0f )
        return 255;
    return (BYTE)(int)( f * 256.0f );
}

// Packs four 0..1 floats into a D3DCOLOR (0xAARRGGBB)
inline DWORD ColorToARGB( float r, float g, float b, float a )
{
    return ( (DWORD)ColorFloatToByte( a ) << 24 ) |
           ( (DWORD)ColorFloatToByte( r ) << 16 ) |
           ( (DWORD)ColorFloatToByte( g ) <<  8 ) |
           ( (DWORD)ColorFloatToByte( b ) );
}

#endif // _HELPER_COLOR_H
