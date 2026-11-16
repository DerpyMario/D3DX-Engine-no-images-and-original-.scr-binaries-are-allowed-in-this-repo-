//-----------------------------------------------------------------------------
// File: camera.h
//
// Desc: The (gently swaying) camera of the Aurora screen saver.
//
//       Original object:
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\camera.obj
//-----------------------------------------------------------------------------
#ifndef _AURORA_CAMERA_H
#define _AURORA_CAMERA_H

#include "point.h"

class Camera
{
public:
    // The constructor is not a function of its own in the shipped image; it
    // only exists inlined into AuroraScreensaver::AuroraScreensaver().
    Camera()
    {
        m_fFov      = 0.6f;
        m_fAspect   = 4.0f / 3.0f;
        m_fNear     = 0.1f;
        m_fFar      = 30.0f;
        m_eye       = Point3f( 1.0f, -2.0f, 0.4f );
        m_lookAt    = Point3f( 0.0f, 0.0f, 0.7f );
        m_up        = Point3f( 0.0f, 0.0f, 1.0f );
        m_direction = ( m_lookAt - m_eye ).Normalized();
    }

    void Update();

    Point3f m_eye;          // Z is up; the arcs run along X
    Point3f m_lookAt;
    Point3f m_up;
    float   m_fFov;         // vertical field of view, radians
    float   m_fAspect;
    float   m_fNear;
    float   m_fFar;
    Point3f m_direction;    // unit vector from m_eye to m_lookAt
};

static_assert( sizeof(Camera) == 0x40, "Camera layout" );

#endif // _AURORA_CAMERA_H
