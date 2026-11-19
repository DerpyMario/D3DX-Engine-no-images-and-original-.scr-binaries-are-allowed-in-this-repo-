//-----------------------------------------------------------------------------
// File: eval.cpp
//
// Desc: EVAL: the Bezier patch control nets of the flexible pipes.
//
//       The OpenGL screen saver drew the flexible pipes with the OpenGL
//       evaluators (glMap2f / glEvalMesh2).  This Direct3D port only kept
//       the computation of the control points: nothing is drawn from them.
//
//       Reconstructed from eval.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\eval.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

#define DEG_TO_RAD( a )     ( (a) * PI / 180.0f )




//-----------------------------------------------------------------------------
// Name: EVAL()
// Desc:
//-----------------------------------------------------------------------------
EVAL::EVAL( BOOL bTexture )
{
    m_bTexture = bTexture;

    // Control points
    int size = MAX_XC_PTS * MAX_VORDER * sizeof(D3DXVECTOR3);
    m_pts = (D3DXVECTOR3*) LocalAlloc( LMEM_FIXED, size );

    // Texture control points
    if( m_bTexture )
    {
        size = MAX_USECTIONS * TEX_ORDER * TEX_ORDER * sizeof(TEX_POINT2D);
        m_texPts = (TEX_POINT2D*) LocalAlloc( LMEM_FIXED, size );
    }
}




//-----------------------------------------------------------------------------
// Name: ~EVAL()
// Desc:
//-----------------------------------------------------------------------------
EVAL::~EVAL()
{
    LocalFree( m_pts );
    if( m_bTexture )
        LocalFree( m_texPts );
}




//-----------------------------------------------------------------------------
// Name: SetTextureControlPoints()
// Desc: The texture control points of the sections: s runs along the pipe,
//       t around it
//-----------------------------------------------------------------------------
void EVAL::SetTextureControlPoints( FLOAT s_start, FLOAT s_end, FLOAT t_start, FLOAT t_end )
{
    int i;
    TEX_POINT2D* ptexPts = m_texPts;
    FLOAT t_delta = (t_end - t_start) / m_numSections;
    FLOAT t = t_start;

    for( i = 0; i < m_numSections; i++, ptexPts += 4 )
    {
        ptexPts[0].t = ptexPts[2].t = t;
        t += t_delta;
        ptexPts[1].t = ptexPts[3].t = t;
        ptexPts[0].s = ptexPts[1].s = s_start;
        ptexPts[2].s = ptexPts[3].s = s_end;
    }
}




//-----------------------------------------------------------------------------
// Name: SetVertexCtrlPtsXCTranslate()
// Desc: The control net of two cross sections length apart along z: the
//       first one at z = 0, the second one at z = length.  The first point
//       of each set is replicated at its end.
//-----------------------------------------------------------------------------
void EVAL::SetVertexCtrlPtsXCTranslate( D3DXVECTOR3* pts, FLOAT length,
                                        XC* xcStart, XC* xcEnd )
{
    int i;
    D3DXVECTOR2 *ptsStart, *ptsEnd;
    D3DXVECTOR3 *pts1, *pts2;
    int numPts = xcStart->m_numPts;

    numPts++;   // the replicated point

    ptsStart = xcStart->m_pts;
    ptsEnd   = xcEnd->m_pts;
    pts1     = pts;
    pts2     = pts + numPts;

    for( i = 0; i < (numPts-1); i++, pts1++, pts2++ )
    {
        // x and y of each cross section
        pts1->x = ptsStart->x;
        pts1->y = ptsStart->y;
        ptsStart++;
        pts2->x = ptsEnd->x;
        pts2->y = ptsEnd->y;
        ptsEnd++;

        // and their z
        pts1->z = 0.0f;
        pts2->z = length;
    }

    // Replicate the first point of each set
    *pts1 = *pts;
    *pts2 = *(pts + numPts);
}




//-----------------------------------------------------------------------------
// Name: ExtrudePointSetDir()
// Desc: Moves the points back from dir by their arc control values
//-----------------------------------------------------------------------------
static void ExtrudePointSetDir( D3DXVECTOR3* inPts, int numPts, FLOAT* acPts, int dir,
                                D3DXVECTOR3* outPts )
{
    int i;
    FLOAT sign;
    int offset;

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
        case PLUS_Z:
            offset = 2;
            sign = -1.0f;
            break;
        case MINUS_Z:
            offset = 2;
            sign = 1.0f;
            break;
    }

    for( i = 0; i < numPts; i++, inPts++, outPts++, acPts++ )
    {
        *outPts = *inPts;
        ((FLOAT*) outPts)[offset] = ((FLOAT*) inPts)[offset] + (sign * (*acPts));
    }
}




//-----------------------------------------------------------------------------
// Name: RotatePointSet()
// Desc: Rotates the points by angle (degrees) about the axis of a turn
//       towards dir (a direction relative to the cross section) of the
//       given radius
//
//       (The matrices are multiplied in the order of the OpenGL version,
//       which is the reverse order with the row vectors of D3DX.)
//-----------------------------------------------------------------------------
static void RotatePointSet( D3DXVECTOR3* inPts, int numPts, FLOAT angle, int dir,
                            FLOAT radius, D3DXVECTOR3* outPts )
{
    D3DXMATRIX matrix1, matrix2, matrix3;
    D3DXVECTOR3 rot( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 anchor( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR4 v;

    // Rotations are clockwise with the axis pointing at you
    angle = DEG_TO_RAD( -angle );

    // The rotation and the anchor point
    //   dir   rot
    //   +x    90 y
    //   -x   -90 y
    //   +y   -90 x
    //   -y    90 x
    switch( dir )
    {
        case PLUS_X:
            rot.y = angle;
            anchor.x = radius;
            break;
        case MINUS_X:
            rot.y = -angle;
            anchor.x = -radius;
            break;
        case PLUS_Y:
            rot.x = -angle;
            anchor.y = radius;
            break;
        case MINUS_Y:
            rot.x = angle;
            anchor.y = -radius;
            break;
    }

    // Move the anchor point to the origin
    D3DXMatrixIdentity( &matrix1 );
    D3DXMatrixTranslation( &matrix1, -anchor.x, -anchor.y, -anchor.z );

    // Rotate
    D3DXMatrixIdentity( &matrix2 );
    D3DXMatrixRotationYawPitchRoll( &matrix2, rot.y, rot.x, rot.z );

    D3DXMatrixMultiply( &matrix3, &matrix2, &matrix1 );

    // Move back
    D3DXMatrixIdentity( &matrix2 );
    D3DXMatrixTranslation( &matrix2, anchor.x, anchor.y, anchor.z );

    D3DXMatrixMultiply( &matrix1, &matrix2, &matrix3 );

    // Transform the points
    for( int i = 0; i < numPts; i++, inPts++, outPts++ )
    {
        D3DXVec3Transform( &v, inPts, &matrix1 );
        outPts->x = v.x;
        outPts->y = v.y;
        outPts->z = v.z;
    }
}




//-----------------------------------------------------------------------------
// Name: ProcessXCPrimLinear()
// Desc: A straight piece: the extrusion of a cross section along z
//-----------------------------------------------------------------------------
void EVAL::ProcessXCPrimLinear( XC* xcStart, XC* xcEnd, FLOAT length )
{
    if( length <= 0.0f )
        return;     // nothing to do

    SetVertexCtrlPtsXCTranslate( m_pts, length, xcStart, xcEnd );
}




//-----------------------------------------------------------------------------
// Name: ProcessXCPrimBendSimple()
// Desc: A 90 degree bend towards dir (relative to the cross section) of the
//       given radius: 4 point sets, the cross section at z = 0, its rotation
//       at the end of the bend, and the two sets of arc control points in
//       between
//-----------------------------------------------------------------------------
void EVAL::ProcessXCPrimBendSimple( XC* xcCur, int dir, FLOAT radius )
{
    D3DXVECTOR3 *ptsSrc, *ptsDst;
    static FLOAT acPts[MAX_XC_PTS+1];
    int ptSetStride = xcCur->m_numPts + 1;     // with the replicated point

    // 1st point set: the cross section at z = 0
    xcCur->ConvertPtsZ( m_pts, 0.0f );

    // 4th point set: the 1st one rotated to the end of the bend
    ptsDst = m_pts + 3*ptSetStride;
    RotatePointSet( m_pts, ptSetStride, 90.0f, dir, radius, ptsDst );

    // The arc control values of the points
    xcCur->CalcArcACValues90( dir, radius, acPts );

    // 2nd point set: the 1st one moved forward along +z
    ptsSrc = m_pts;
    ptsDst = m_pts + ptSetStride;
    ExtrudePointSetDir( ptsSrc, ptSetStride, acPts, MINUS_Z, ptsDst );

    // 3rd point set: the 4th one moved back from dir
    ptsSrc = m_pts + 3*ptSetStride;
    ptsDst = m_pts + 2*ptSetStride;
    ExtrudePointSetDir( ptsSrc, ptSetStride, acPts, dir, ptsDst );
}




//-----------------------------------------------------------------------------
// Name: ProcessXCPrimSingularity()
// Desc: A cap: joins the cross section (at z = 0) to a point (a tiny copy of
//       the cross section) length away along z: at -z when opening the
//       pipe, at +z when closing it
//-----------------------------------------------------------------------------
void EVAL::ProcessXCPrimSingularity( XC* xcCur, FLOAT length, BOOL bOpening )
{
    D3DXVECTOR3 *ptsSing, *ptsXC;
    static FLOAT acPts[MAX_XC_PTS+1];
    FLOAT zSing;                                // z of the singularity
    int ptSetStride = xcCur->m_numPts + 1;     // with the replicated point
    int i;
    XC xcSing( xcCur );

    // The singularity: an extremely scaled-down copy of the cross section
    // (no artifacts at the end, unless zoomed ultra-large)
    xcSing.Scale( 0.0005f );

    if( bOpening )
    {
        ptsSing = m_pts;
        ptsXC = m_pts + 3*ptSetStride;
    }
    else
    {
        ptsSing = m_pts + 3*ptSetStride;
        ptsXC = m_pts;
    }

    // The cross section at z = 0
    xcCur->ConvertPtsZ( ptsXC, 0.0f );

    // The singularity
    zSing = bOpening ? -length : length;
    xcSing.ConvertPtsZ( ptsSing, zSing );

    // The arc control values: the distances from the centre
    xcCur->CalcArcACValuesByDistance( acPts );

    // The point set next to the cross section: the cross section moved
    // towards the singularity
    if( bOpening )
        ExtrudePointSetDir( ptsXC, ptSetStride, acPts, PLUS_Z, ptsXC - ptSetStride );
    else
        ExtrudePointSetDir( ptsXC, ptSetStride, acPts, MINUS_Z, ptsXC + ptSetStride );

    // The point set next to the singularity: the cross section scaled by
    // the arc control factor at the z of the singularity
    if( bOpening )
        ptsSing += ptSetStride;
    else
        ptsSing -= ptSetStride;

    for( i = 0; i < ptSetStride; i++, ptsSing++, ptsXC++ )
    {
        ptsSing->x = EVAL_ARC_CTRL * ptsXC->x;
        ptsSing->y = EVAL_ARC_CTRL * ptsXC->y;
        ptsSing->z = zSing;
    }
}
