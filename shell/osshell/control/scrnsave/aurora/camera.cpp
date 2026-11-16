//-----------------------------------------------------------------------------
// File: camera.cpp
//
// Desc: The (gently swaying) camera of the Aurora screen saver.
//
//       Original object (417 bytes of code):
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\camera.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <math.h>
#include "random.h"
#include "camera.h"




//-----------------------------------------------------------------------------
// Name: Camera::Update()
// Desc: Moves the eye point around a small ellipse.  The two axes run at
//       slightly different rates (1 : 0.99), so the path never quite repeats.
//-----------------------------------------------------------------------------
void Camera::Update()
{
    static float s_fTime = RandomFloat() * -10000.0f;

    s_fTime += 0.01f;
    if( s_fTime > 10000.0f )
        s_fTime = RandomFloat() * -10000.0f;

    m_eye.x = sinf( s_fTime * 0.99f ) * 0.2f + 1.0f;
    m_eye.y = cosf( s_fTime ) * 0.2f - 2.0f;
    m_eye.z = 0.4f;

    m_direction = ( m_lookAt - m_eye ).Normalized();
}
