//-----------------------------------------------------------------------------
// File: point.h
//
// Desc: Small 2D / 3D float vector classes used by the screen saver helper
//       library.
//
//       Only the class names (Point2f, Point3f) and their layout are known from
//       the shipped binaries: every member function was inlined by the
//       compiler, so the operator set below is simply what the callers need.
//-----------------------------------------------------------------------------
#ifndef _HELPER_POINT_H
#define _HELPER_POINT_H

#include <math.h>

//-----------------------------------------------------------------------------
// Name: class Point2f
//-----------------------------------------------------------------------------
class Point2f
{
public:
    float x, y;

    Point2f() { }
    Point2f( float fx, float fy ) : x( fx ), y( fy ) { }

    Point2f  operator+( const Point2f& p ) const { return Point2f( x + p.x, y + p.y ); }
    Point2f  operator-( const Point2f& p ) const { return Point2f( x - p.x, y - p.y ); }
    Point2f  operator*( float f ) const          { return Point2f( x * f, y * f ); }
    Point2f  operator/( float f ) const          { return Point2f( x / f, y / f ); }
    Point2f& operator+=( const Point2f& p )      { x += p.x; y += p.y; return *this; }
    Point2f& operator-=( const Point2f& p )      { x -= p.x; y -= p.y; return *this; }
    Point2f& operator*=( float f )               { x *= f; y *= f; return *this; }

    float Dot( const Point2f& p ) const          { return x * p.x + y * p.y; }
    float LengthSquared() const                  { return x * x + y * y; }
    float Length() const                         { return sqrtf( x * x + y * y ); }
};

//-----------------------------------------------------------------------------
// Name: class Point3f
//-----------------------------------------------------------------------------
class Point3f
{
public:
    float x, y, z;

    Point3f() { }
    Point3f( float fx, float fy, float fz ) : x( fx ), y( fy ), z( fz ) { }

    Point3f  operator+( const Point3f& p ) const { return Point3f( x + p.x, y + p.y, z + p.z ); }
    Point3f  operator-( const Point3f& p ) const { return Point3f( x - p.x, y - p.y, z - p.z ); }
    Point3f  operator*( float f ) const          { return Point3f( x * f, y * f, z * f ); }
    Point3f  operator/( float f ) const          { return Point3f( x / f, y / f, z / f ); }
    Point3f& operator+=( const Point3f& p )      { x += p.x; y += p.y; z += p.z; return *this; }
    Point3f& operator-=( const Point3f& p )      { x -= p.x; y -= p.y; z -= p.z; return *this; }
    Point3f& operator*=( float f )               { x *= f; y *= f; z *= f; return *this; }

    float Dot( const Point3f& p ) const          { return x * p.x + y * p.y + z * p.z; }
    float Length() const                         { return sqrtf( y * y + x * x + z * z ); }

    // Returns a unit-length copy.  (No zero-length check, like the original.)
    Point3f Normalized() const
    {
        float fLength = Length();
        return Point3f( x / fLength, y / fLength, z / fLength );
    }
};

#endif // _HELPER_POINT_H
