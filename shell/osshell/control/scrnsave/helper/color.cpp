//-----------------------------------------------------------------------------
// File: color.cpp
//
// Desc: Colour helpers.
//
//       Original object (161 bytes of code, linked into Bubbles.scr):
//           shell\osshell\control\scrnsave\helper\objfre\amd64\color.obj
//-----------------------------------------------------------------------------
#include "color.h"




//-----------------------------------------------------------------------------
// Name: HueToRGB()
// Desc: One channel of the standard HSL -> RGB conversion.
//-----------------------------------------------------------------------------
float __cdecl HueToRGB( float v1, float v2, float vH )
{
    if( vH < 0.0f )
        vH += 1.0f;
    else if( vH >= 1.0f )
        vH -= 1.0f;

    if( vH * 6.0f < 1.0f )
        return ( v2 - v1 ) * 6.0f * vH + v1;

    if( vH * 2.0f < 1.0f )
        return v2;

    if( vH * 3.0f < 2.0f )
        return ( v2 - v1 ) * ( ( 2.0f / 3.0f ) - vH ) * 6.0f + v1;

    return v1;
}
