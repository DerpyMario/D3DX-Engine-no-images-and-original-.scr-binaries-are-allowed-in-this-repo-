//-----------------------------------------------------------------------------
// File: xc.cpp
//
// Desc: XC: the cross sections of the flexible pipes.  A cross section is a
//       closed curve in the xy plane made of 4 Bezier arcs, given by its 12
//       control points (counter-clockwise from +x; the joining points are at
//       the indices 0, 3, 6 and 9).
//
//       Reconstructed from xc.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\xc.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include <float.h>

#define XC_NUM_PTS      12      // control points of a 4-arc cross section

// The sides of the box of a RANDOM4ARC_XC
enum { RIGHT = 0, TOP, LEFT, BOTTOM };




//-----------------------------------------------------------------------------
// Name: CalcArcACValues90()
// Desc: The arc control values of a 90 degree turn of the given radius
//       towards dir (a direction relative to the cross section: PLUS_X ..
//       MINUS_Y), for each control point.  The value of the first point is
//       replicated at the end.
//-----------------------------------------------------------------------------
void XC::CalcArcACValues90( int dir, FLOAT radius, FLOAT* acPts )
{
    int i;
    FLOAT sign;
    int offset;
    FLOAT* ppts = (FLOAT*) m_pts;

    // The distance of each point from the centre of the turn
    switch( dir )
    {
        case PLUS_X:
            offset = 0;
            sign = -1.0f;
            break;
        case MINUS_X:
            offset = 0;
            sign = 1.0f;
            break;
        case PLUS_Y:
            offset = 1;
            sign = -1.0f;
            break;
        case MINUS_Y:
            offset = 1;
            sign = 1.0f;
            break;
    }

    for( i = 0; i < m_numPts; i++, ppts += 2 )
        *acPts++ = EVAL_ARC_CTRL * (radius + (sign * ppts[offset]));

    // replicate the first one
    *acPts = *(acPts - m_numPts);
}




//-----------------------------------------------------------------------------
// Name: CalcArcACValuesByDistance()
// Desc: The arc control values of the points: their distance from the
//       centre of the cross section.  The value of the first point is
//       replicated at the end.
//-----------------------------------------------------------------------------
void XC::CalcArcACValuesByDistance( FLOAT* acPts )
{
    int i;
    FLOAT r;
    D3DXVECTOR2* ppts = m_pts;

    for( i = 0; i < m_numPts; i++, ppts++ )
    {
        r = (FLOAT) sqrt( ppts->x*ppts->x + ppts->y*ppts->y );
        *acPts++ = EVAL_ARC_CTRL * r;
    }

    // replicate the first one
    *acPts = *(acPts - m_numPts);
}




//-----------------------------------------------------------------------------
// Name: ELLIPTICAL_XC::SetControlPoints()
// Desc: An ellipse with the radii r1 (x) and r2 (y)
//-----------------------------------------------------------------------------
void ELLIPTICAL_XC::SetControlPoints( FLOAT r1, FLOAT r2 )
{
    FLOAT ac1, ac2;

    ac1 = EVAL_ARC_CTRL * r2;
    ac2 = EVAL_ARC_CTRL * r1;

    // the last 2 points of the right triplet
    m_pts[0].x = r1;
    m_pts[0].y = 0.0f;
    m_pts[1].x = r1;
    m_pts[1].y = ac1;

    // top triplet
    m_pts[2].x = ac2;
    m_pts[2].y = r2;
    m_pts[3].x = 0.0f;
    m_pts[3].y = r2;
    m_pts[4].x = -ac2;
    m_pts[4].y = r2;

    // left triplet
    m_pts[5].x = -r1;
    m_pts[5].y = ac1;
    m_pts[6].x = -r1;
    m_pts[6].y = 0.0f;
    m_pts[7].x = -r1;
    m_pts[7].y = -ac1;

    // bottom triplet
    m_pts[8].x = -ac2;
    m_pts[8].y = -r2;
    m_pts[9].x = 0.0f;
    m_pts[9].y = -r2;
    m_pts[10].x = ac2;
    m_pts[10].y = -r2;

    // the first point of the right triplet
    m_pts[11].x = r1;
    m_pts[11].y = -ac1;
}




//-----------------------------------------------------------------------------
// Name: RANDOM4ARC_XC::SetControlPoints()
// Desc: A random cross section within a box of random size around the
//       origin, at most radius from it
//-----------------------------------------------------------------------------
void RANDOM4ARC_XC::SetControlPoints( FLOAT radius )
{
    int i;
    FLOAT r[4];
    FLOAT rMin = 0.5f * radius;
    FLOAT distx, disty;

    // The distance of each side of the box from the origin
    for( i = 0; i < 4; i++ )
        r[i] = CPipesScreensaver::fRand( rMin, radius );

    // The joining points of the arcs are on the sides of the box
    m_pts[0].x = r[RIGHT];
    m_pts[3].y = r[TOP];
    m_pts[6].x = -r[LEFT];
    m_pts[9].y = -r[BOTTOM];

    // A quarter of the size of the box
    disty = (r[TOP] - -r[BOTTOM]) / 4.0f;
    distx = (r[RIGHT] - -r[LEFT]) / 4.0f;

    // Put them somewhere in the middle half of each side
    m_pts[0].y = CPipesScreensaver::fRand( -r[BOTTOM] + disty, r[TOP] - disty );
    m_pts[6].y = CPipesScreensaver::fRand( -r[BOTTOM] + disty, r[TOP] - disty );
    m_pts[3].x = CPipesScreensaver::fRand( -r[LEFT] + distx, r[RIGHT] - distx );
    m_pts[9].x = CPipesScreensaver::fRand( -r[LEFT] + distx, r[RIGHT] - distx );

    // The arc control points next to them are on the same sides
    m_pts[1].x = m_pts[11].x = m_pts[0].x;
    m_pts[2].y = m_pts[4].y = m_pts[3].y;
    m_pts[5].x = m_pts[7].x = m_pts[6].x;
    m_pts[8].y = m_pts[10].y = m_pts[9].y;

    // right side
    disty = (r[TOP] - m_pts[0].y) / 4.0f;
    m_pts[1].y = CPipesScreensaver::fRand( m_pts[0].y + disty, r[TOP] );
    disty = (m_pts[0].y - -r[BOTTOM]) / 4.0f;
    m_pts[11].y = CPipesScreensaver::fRand( -r[BOTTOM], m_pts[0].y - disty );

    // left side
    disty = (r[TOP] - m_pts[6].y) / 4.0f;
    m_pts[5].y = CPipesScreensaver::fRand( m_pts[6].y + disty, r[TOP] );
    disty = (m_pts[6].y - -r[BOTTOM]) / 4.0f;
    m_pts[7].y = CPipesScreensaver::fRand( -r[BOTTOM], m_pts[6].y - disty );

    // top
    distx = (r[RIGHT] - m_pts[3].x) / 4.0f;
    m_pts[2].x = CPipesScreensaver::fRand( m_pts[3].x + distx, r[RIGHT] );
    distx = (m_pts[3].x - -r[LEFT]) / 4.0f;
    m_pts[4].x = CPipesScreensaver::fRand( -r[LEFT], m_pts[3].x - distx );

    // bottom
    distx = (r[RIGHT] - m_pts[9].x) / 4.0f;
    m_pts[10].x = CPipesScreensaver::fRand( m_pts[9].x + distx, r[RIGHT] );
    distx = (m_pts[9].x - -r[LEFT]) / 4.0f;
    m_pts[8].x = CPipesScreensaver::fRand( -r[LEFT], m_pts[9].x - distx );
}




//-----------------------------------------------------------------------------
// Name: ConvertPtsZ()
// Desc: The control points as 3D points at the given z.  The first point is
//       replicated at the end.
//-----------------------------------------------------------------------------
void XC::ConvertPtsZ( D3DXVECTOR3* newpts, FLOAT z )
{
    int i;
    D3DXVECTOR2* xcPts = m_pts;

    for( i = 0; i < m_numPts; i++, newpts++ )
    {
        newpts->x = xcPts->x;
        newpts->y = xcPts->y;
        xcPts++;
        newpts->z = z;
    }

    *newpts = *(newpts - m_numPts);
}




//-----------------------------------------------------------------------------
// Name: CalcBoundingBox()
// Desc:
//-----------------------------------------------------------------------------
void XC::CalcBoundingBox()
{
    D3DXVECTOR2* ppts = m_pts;
    FLOAT xMin, xMax, yMin, yMax;

    xMax = yMax = -FLT_MAX;
    xMin = yMin = FLT_MAX;

    for( int i = 0; i < m_numPts; i++, ppts++ )
    {
        if( ppts->x < xMin )
            xMin = ppts->x;
        else if( ppts->x > xMax )
            xMax = ppts->x;
        if( ppts->y < yMin )
            yMin = ppts->y;
        else if( ppts->y > yMax )
            yMax = ppts->y;
    }

    m_xLeft   = xMin;
    m_xRight  = xMax;
    m_yBottom = yMin;
    m_yTop    = yMax;
}




//-----------------------------------------------------------------------------
// Name: MinTurnRadius()
// Desc: The smallest radius of a turn towards relDir (a direction relative
//       to the cross section).  Assumes that xRight and yTop are positive,
//       xLeft and yBottom negative.
//-----------------------------------------------------------------------------
FLOAT XC::MinTurnRadius( int relDir )
{
    switch( relDir )
    {
        case PLUS_X:
            return m_xRight;
        case MINUS_X:
            return -m_xLeft;
        case PLUS_Y:
            return m_yTop;
        case MINUS_Y:
            return -m_yBottom;
        default:
            return 0.0f;
    }
}




//-----------------------------------------------------------------------------
// Name: MaxExtent()
// Desc: The largest distance of the bounding box from the origin
//-----------------------------------------------------------------------------
FLOAT XC::MaxExtent()
{
    FLOAT max;

    max = m_xRight;

    if( m_yTop > max )
        max = m_yTop;
    if( -m_xLeft > max )
        max = -m_xLeft;
    if( -m_yBottom > max )
        max = -m_yBottom;

    return max;
}




//-----------------------------------------------------------------------------
// Name: Scale()
// Desc:
//-----------------------------------------------------------------------------
void XC::Scale( FLOAT scale )
{
    int i;
    D3DXVECTOR2* ppts = m_pts;

    if( ppts == NULL )
        return;

    for( i = 0; i < m_numPts; i++, ppts++ )
    {
        ppts->x *= scale;
        ppts->y *= scale;
    }

    m_xLeft   *= scale;
    m_xRight  *= scale;
    m_yBottom *= scale;
    m_yTop    *= scale;
}




//-----------------------------------------------------------------------------
// Name: XC()
// Desc: A copy of a cross section
//-----------------------------------------------------------------------------
XC::XC( XC* xc )
{
    m_numPts = xc->m_numPts;

    if( m_pts != NULL )
        memcpy( m_pts, xc->m_pts, m_numPts * sizeof(D3DXVECTOR2) );

    m_xLeft   = xc->m_xLeft;
    m_xRight  = xc->m_xRight;
    m_yBottom = xc->m_yBottom;
    m_yTop    = xc->m_yTop;
}




//-----------------------------------------------------------------------------
// Name: XC()
// Desc:
//-----------------------------------------------------------------------------
XC::XC( int nPts )
{
    m_numPts = nPts;
}




//-----------------------------------------------------------------------------
// Name: ELLIPTICAL_XC()
// Desc:
//-----------------------------------------------------------------------------
ELLIPTICAL_XC::ELLIPTICAL_XC( FLOAT r1, FLOAT r2 ) : XC( XC_NUM_PTS )
{
    SetControlPoints( r1, r2 );
    CalcBoundingBox();
}




//-----------------------------------------------------------------------------
// Name: RANDOM4ARC_XC()
// Desc:
//-----------------------------------------------------------------------------
RANDOM4ARC_XC::RANDOM4ARC_XC( FLOAT radius ) : XC( XC_NUM_PTS )
{
    SetControlPoints( radius );
    CalcBoundingBox();
}
