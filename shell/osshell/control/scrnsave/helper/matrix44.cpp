//-----------------------------------------------------------------------------
// File: matrix44.cpp
//
// Desc: Minimal 4x4 float matrix.
//
//       Original object (51 bytes of code):
//           shell\osshell\control\scrnsave\helper\objfre\amd64\matrix44.obj
//-----------------------------------------------------------------------------
#include "matrix44.h"




//-----------------------------------------------------------------------------
// Name: Matrix44f::Identity()
//-----------------------------------------------------------------------------
void Matrix44f::Identity()
{
    for( int i = 0; i < 4; i++ )
    {
        for( int j = 0; j < 4; j++ )
        {
            m[i][j] = ( i == j ) ? 1.0f : 0.0f;
        }
    }
}
