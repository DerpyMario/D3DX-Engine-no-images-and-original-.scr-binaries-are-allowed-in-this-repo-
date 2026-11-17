//-----------------------------------------------------------------------------
// File: fontoutl.cpp
//
// Desc: Shape3D.lib - 3D text meshes from TrueType outlines.
//
//       For every character the native outline of the glyph is fetched with
//       GetGlyphOutline( GGO_NATIVE ), its quadratic splines are flattened
//       into closed contour loops, the loops are tessellated with the GLU
//       tessellator and extruded (fontextr.cpp), and the triangles of all
//       glyphs are finally copied into one ID3DXMesh.
//
//       Reconstructed from ssText3d.scr 6.0.6001.18000 (amd64).  The function
//       names are the ones of the symbol file (which also names the static
//       functions).  InitLoopList, MakeLinesFromGlyph, CalcVertPtrs,
//       GetFixedPoint and the CFaceContext members other than ResizeFaces,
//       ResizeVertices, SetupPrim and AddQuadStrip were inlined into their
//       callers and are named by this reconstruction.
//
//       Original object:
//           shell\osshell\control\scrnsave\shape3d\objfre\amd64\fontoutl.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <new>
#include "fontoutl.h"
#include "shape3d.h"

using namespace Shape3D;

namespace Shape3D
{

#define SHAPE3D_PI                  3.14159265358979323846

// Three points are taken to be on a line if the direction changes by less
// than half a degree.
#define COLINEAR_THRESHOLD_ANGLE    ( SHAPE3D_PI / 360.0 )

#define MAX_CHORDAL_DEVIATION       0.035f

#define LOOP_BUF_INITIAL            10
#define LOOP_BUF_INCREMENT          50
#define PRIM_BUF_INCREMENT          50
#define VERT_BUF_INITIAL            50
#define VERT_BUF_INCREMENT          100

#define FACE_CONTEXT_INITIAL        100         // faces and vertices

static HRESULT      MakeDisplayListFromGlyph( OFContext* ofc );
static void CALLBACK TessCombine( GLdouble coords[3], void* vertex_data[4], GLfloat weight[4],
                                  void** outData, void* data );
static void CALLBACK TessError( GLenum error, void* data );
static BOOL         MakeLinesFromTTPolygon( OFContext* ofc, LOOP_LIST* pLoopList, UCHAR** pp );
static BOOL         MakeLinesFromTTLine( OFContext* ofc, LOOP* pLoop, PRIM* pPrim, UCHAR** pp, WORD pointCount );
static BOOL         MakeLinesFromTTQSpline( OFContext* ofc, LOOP* pLoop, PRIM* pPrim, UCHAR** pp, WORD pointCount );
static BOOL         MakeLinesFromArc( OFContext* ofc, LOOP* pLoop, PRIM* pPrim, POINT2D p0, POINT2D p1,
                                      POINT2D p2, FLOAT chordalDeviationSquared );
static void         CheckRedundantVertices( LOOP* pLoop );
static BOOL         PointsColinear( POINT2D* p1, POINT2D* p2, POINT2D* p3 );
static HRESULT      CreateOFContext( HDC hdc, FLOAT chordalDeviation, FLOAT extrusion, INT format,
                                     OFContext** ppOFContext );
static BOOL         ScaleFont( HDC hdc, OFContext* ofc );
static void         DestroyOFContext( HDC hdc, OFContext* ofc );
static LOOP*        NewLoop( LOOP_LIST* pLoopList, POINT2D* pFirstPoint );
static PRIM*        NewPrim( LOOP* pLoop, DWORD primType );
static void         FreeLoopList( LOOP_LIST* pLoopList );
static BOOL         AppendToVertBuf( LOOP* pLoop, PRIM* pPrim, POINT2D* p );
static HRESULT      GenerateMesh( LPDIRECT3DDEVICE9 pDevice, CFaceContext** ppFaceContexts, UINT cFaceContexts,
                                  LPD3DXMESH* ppMesh );

// identity transform for GetGlyphOutline()
static const MAT2 s_matIdentity = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };

// which corner goes into which slot of a face, for the two windings
static const UINT s_rgOrder[3]         = { 0, 1, 2 };
static const UINT s_rgOrderReversed[3] = { 0, 2, 1 };


//-----------------------------------------------------------------------------
// Name: InitLoopList()
//-----------------------------------------------------------------------------
static inline LOOP_LIST* InitLoopList()
{
    LOOP_LIST* pLoopList = (LOOP_LIST*)malloc( sizeof(LOOP_LIST) );
    if( pLoopList == NULL )
        return NULL;

    LOOP* pLoops = (LOOP*)malloc( LOOP_BUF_INITIAL * sizeof(LOOP) );
    if( pLoops == NULL )
    {
        free( pLoopList );
        return NULL;
    }

    pLoopList->LoopBuf = pLoops;
    pLoopList->nLoops = 0;
    pLoopList->LoopBufSize = LOOP_BUF_INITIAL;

    return pLoopList;
}


//-----------------------------------------------------------------------------
// Name: MakeLinesFromGlyph()
// Desc: Converts the outline of the current glyph into contour loops.
//-----------------------------------------------------------------------------
static inline HRESULT MakeLinesFromGlyph( OFContext* ofc, LOOP_LIST** ppLoopList )
{
    LOOP_LIST* pLoopList = InitLoopList();
    if( pLoopList == NULL )
    {
        *ppLoopList = NULL;
        return E_OUTOFMEMORY;
    }

    UCHAR* p = ofc->glyphBuf;

    while( p < ofc->glyphBuf + ofc->glyphSize )
    {
        if( !MakeLinesFromTTPolygon( ofc, pLoopList, &p ) )
        {
            FreeLoopList( pLoopList );
            *ppLoopList = NULL;
            return E_FAIL;
        }
    }

    *ppLoopList = pLoopList;
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: MakeDisplayListFromGlyph()
// Desc: Turns the glyph in ofc->glyphBuf into triangles, which end up in the
//       face context of the extrusion context.
//-----------------------------------------------------------------------------
static HRESULT MakeDisplayListFromGlyph( OFContext* ofc )
{
    LOOP_LIST*  pLoopList;
    HRESULT     hr;

    hr = MakeLinesFromGlyph( ofc, &pLoopList );

    if( SUCCEEDED( hr ) )
    {
        DWORD nLoops;
        LOOP* pLoop;

        pLoop = pLoopList->LoopBuf;
        for( nLoops = pLoopList->nLoops; nLoops != 0; nLoops--, pLoop++ )
            CheckRedundantVertices( pLoop );

        // nothing in the combine buffers is needed any longer
        ofc->pCurCombineBuf = &ofc->combineBuf;
        ofc->combineBuf.nVerts = 0;
        ofc->combineBuf.pNext = NULL;

        if( ofc->ec == NULL || extr_PolyInit( ofc->ec ) )
        {
            GLdouble v[3];

            v[2] = 0.0;

            ofc->hr = S_OK;

            gluTessBeginPolygon( ofc->tess, ofc );

            pLoop = pLoopList->LoopBuf;
            for( nLoops = pLoopList->nLoops; nLoops != 0; nLoops--, pLoop++ )
            {
                gluTessBeginContour( ofc->tess );

                // the last vertex repeats the first one
                POINT2D* pVert = pLoop->VertBuf;
                for( DWORD nVerts = pLoop->nVerts - 1; nVerts != 0; nVerts--, pVert++ )
                {
                    v[0] = pVert->x;
                    v[1] = pVert->y;
                    gluTessVertex( ofc->tess, v, pVert );
                }

                gluTessEndContour( ofc->tess );
            }

            gluTessEndPolygon( ofc->tess );

            if( FAILED( ofc->hr ) )
                hr = ofc->hr;
            else if( ofc->ec != NULL )
                hr = extr_DrawPolygons( ofc->ec, pLoopList );
        }

        // free the combine buffers that had to be added
        CombineBuf* pBuf = ofc->combineBuf.pNext;
        while( pBuf != NULL )
        {
            CombineBuf* pNext = pBuf->pNext;
            free( pBuf );
            pBuf = pNext;
        }
    }

    if( pLoopList != NULL )
        FreeLoopList( pLoopList );

    if( ofc->ec != NULL )
        extr_PolyFinish( ofc->ec );

    return hr;
}


//-----------------------------------------------------------------------------
// Name: TessCombine()
// Desc: GLU_TESS_COMBINE_DATA callback: the tessellator needs a new vertex
//       where two contours cross.
//-----------------------------------------------------------------------------
static void CALLBACK TessCombine( GLdouble coords[3], void* vertex_data[4], GLfloat weight[4],
                                  void** outData, void* data )
{
    OFContext*  ofc = (OFContext*)data;
    CombineBuf* pBuf = ofc->pCurCombineBuf;

    (void)vertex_data;
    (void)weight;

    if( pBuf->nVerts < 0 )
        return;

    if( pBuf->nVerts >= COMBINE_BUF_VERTS )
    {
        // this block is full, chain another one
        CombineBuf* pNewBuf = (CombineBuf*)malloc( sizeof(CombineBuf) );
        if( pNewBuf == NULL )
            return;

        pNewBuf->pNext = NULL;
        pBuf->pNext = pNewBuf;
        ofc->pCurCombineBuf = pNewBuf;
        pNewBuf->nVerts = 0;
        pBuf = pNewBuf;
    }

    POINT2D* p = &pBuf->verts[pBuf->nVerts];
    p->x = (FLOAT)coords[0];
    p->y = (FLOAT)coords[1];
    pBuf->nVerts++;

    *outData = p;
}


//-----------------------------------------------------------------------------
// Name: TessError()
// Desc: GLU_TESS_ERROR_DATA callback
//-----------------------------------------------------------------------------
static void CALLBACK TessError( GLenum error, void* data )
{
    OFContext* ofc = (OFContext*)data;

    if( error == GLU_TESS_COORD_TOO_LARGE || error == GLU_TESS_NEED_COMBINE_CALLBACK )
        ofc->hr = E_OUTOFMEMORY;
}


//-----------------------------------------------------------------------------
// Name: GetFixedPoint()
// Desc: Reads a POINTFX of the glyph data, scales it and moves on.
//-----------------------------------------------------------------------------
static inline FLOAT FixedToFloat( const FIXED& f )
{
    return (FLOAT)f.fract * ( 1.0f / 65536.0f ) + (FLOAT)f.value;
}

static inline void GetFixedPoint( POINT2D* p, FLOAT scale, UCHAR** pp )
{
    POINTFX* pfx = (POINTFX*)*pp;

    *pp += sizeof(POINTFX);

    p->x = FixedToFloat( pfx->x ) * scale;
    p->y = FixedToFloat( pfx->y ) * scale;
}


//-----------------------------------------------------------------------------
// Name: CalcVertPtrs()
// Desc: The vertex buffer of a loop moves while it grows, so the primitives
//       only know the index of their first vertex until the loop is complete.
//-----------------------------------------------------------------------------
static inline void CalcVertPtrs( LOOP* pLoop )
{
    PRIM* pPrim = pLoop->PrimBuf;

    for( DWORD nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrim++ )
        pPrim->pVert = pLoop->VertBuf + pPrim->vertIndex;
}


//-----------------------------------------------------------------------------
// Name: MakeLinesFromTTPolygon()
// Desc: Converts one contour (a TTPOLYGONHEADER and its TTPOLYCURVE records)
//       into a loop.  *pp is advanced to the next contour.
//-----------------------------------------------------------------------------
static BOOL MakeLinesFromTTPolygon( OFContext* ofc, LOOP_LIST* pLoopList, UCHAR** pp )
{
    TTPOLYGONHEADER* pHeader = (TTPOLYGONHEADER*)*pp;
    DWORD            polySize = pHeader->cb;
    DWORD            dwType = pHeader->dwType;
    POINT2D          p0;
    LOOP*            pLoop;
    PRIM*            pPrim;

    *pp += sizeof(DWORD) + sizeof(DWORD);

    if( dwType != TT_POLYGON_TYPE )
        return FALSE;

    // the contour starts here ...
    GetFixedPoint( &p0, ofc->scale, pp );

    pLoop = NewLoop( pLoopList, &p0 );
    if( pLoop == NULL )
        return FALSE;

    for( ;; )
    {
        if( *pp >= (UCHAR*)pHeader + polySize )
        {
            // ... and has to end here as well
            POINT2D* pLast = &pLoop->VertBuf[pLoop->nVerts - 1];

            if( pLast->x != p0.x || pLast->y != p0.y )
            {
                pPrim = NewPrim( pLoop, TT_PRIM_LINE );
                if( pPrim == NULL )
                    return FALSE;

                if( !AppendToVertBuf( pLoop, pPrim, &p0 ) )
                    return FALSE;
            }

            CalcVertPtrs( pLoop );
            return TRUE;
        }

        TTPOLYCURVE* pCurve = (TTPOLYCURVE*)*pp;
        WORD         wType = pCurve->wType;
        WORD         pointCount = pCurve->cpfx;
        BOOL         bOk;

        *pp += sizeof(WORD) + sizeof(WORD);

        pPrim = NewPrim( pLoop, wType );
        if( pPrim == NULL )
            return FALSE;

        switch( wType )
        {
            case TT_PRIM_LINE:
                bOk = MakeLinesFromTTLine( ofc, pLoop, pPrim, pp, pointCount );
                break;

            case TT_PRIM_QSPLINE:
                bOk = MakeLinesFromTTQSpline( ofc, pLoop, pPrim, pp, pointCount );
                break;

            default:
                bOk = FALSE;
                break;
        }

        if( !bOk )
            return FALSE;
    }
}


//-----------------------------------------------------------------------------
// Name: MakeLinesFromTTLine()
// Desc: A polyline record: the points are taken as they are.
//-----------------------------------------------------------------------------
static BOOL MakeLinesFromTTLine( OFContext* ofc, LOOP* pLoop, PRIM* pPrim, UCHAR** pp, WORD pointCount )
{
    POINT2D p;

    while( pointCount-- )
    {
        GetFixedPoint( &p, ofc->scale, pp );

        if( !AppendToVertBuf( pLoop, pPrim, &p ) )
            return FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: MakeLinesFromTTQSpline()
// Desc: A record of quadratic B-splines.  The points are control points; the
//       curves in between join at the midpoints of consecutive control
//       points, and only the last point of the record lies on the outline.
//-----------------------------------------------------------------------------
static BOOL MakeLinesFromTTQSpline( OFContext* ofc, LOOP* pLoop, PRIM* pPrim, UCHAR** pp, WORD pointCount )
{
    POINT2D p0, p1, p2;
    INT     nCurves = (INT)pointCount - 1;

    // the curve starts at the last point so far
    p0 = pLoop->VertBuf[pLoop->nVerts - 1];

    for( WORD i = 0; (INT)i < nCurves; i++ )
    {
        // the control point
        GetFixedPoint( &p1, ofc->scale, pp );

        if( (INT)i == nCurves - 1 )
        {
            // the last curve ends on the last point
            GetFixedPoint( &p2, ofc->scale, pp );
        }
        else
        {
            // the others end halfway to the next control point (which stays
            // where it is, for the next round)
            POINTFX* pfx = (POINTFX*)*pp;

            p2.x = ( FixedToFloat( pfx->x ) * ofc->scale + p1.x ) * 0.5f;
            p2.y = ( FixedToFloat( pfx->y ) * ofc->scale + p1.y ) * 0.5f;
        }

        if( !MakeLinesFromArc( ofc, pLoop, pPrim, p0, p1, p2,
                               ofc->chordalDeviation * ofc->chordalDeviation ) )
        {
            return FALSE;
        }

        p0 = p2;
    }

    // the end point
    return AppendToVertBuf( pLoop, pPrim, &p0 ) != FALSE;
}


//-----------------------------------------------------------------------------
// Name: MakeLinesFromArc()
// Desc: Flattens the quadratic Bezier curve p0-p1-p2 by subdivision, until
//       the midpoint of the curve is close enough to the control point.  The
//       vertices emitted are the control points of the final pieces; the end
//       point is left to the caller.
//-----------------------------------------------------------------------------
static BOOL MakeLinesFromArc( OFContext* ofc, LOOP* pLoop, PRIM* pPrim, POINT2D p0, POINT2D p1,
                              POINT2D p2, FLOAT chordalDeviationSquared )
{
    POINT2D p01, p12, midPoint;
    FLOAT   dx, dy;

    // de Casteljau at t = 1/2
    p01.x = ( p1.x + p0.x ) * 0.5f;
    p01.y = ( p1.y + p0.y ) * 0.5f;
    p12.x = ( p2.x + p1.x ) * 0.5f;
    p12.y = ( p2.y + p1.y ) * 0.5f;
    midPoint.x = ( p12.x + p01.x ) * 0.5f;
    midPoint.y = ( p12.y + p01.y ) * 0.5f;

    dx = midPoint.x - p1.x;
    dy = midPoint.y - p1.y;

    if( dy * dy + dx * dx > chordalDeviationSquared )
    {
        if( !MakeLinesFromArc( ofc, pLoop, pPrim, p0, p01, midPoint, chordalDeviationSquared ) )
            return FALSE;

        if( !MakeLinesFromArc( ofc, pLoop, pPrim, midPoint, p12, p2, chordalDeviationSquared ) )
            return FALSE;
    }
    else
    {
        if( !AppendToVertBuf( pLoop, pPrim, &p1 ) )
            return FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: CheckRedundantVertices()
// Desc: Where a curve runs straight on into its neighbour (or the other way
//       round), the vertex they share is not needed.  Removes those vertices
//       and then the primitives that have no segment left.
//-----------------------------------------------------------------------------
static void CheckRedundantVertices( LOOP* pLoop )
{
    DWORD nPrims = pLoop->nPrims;
    DWORD nVerts;
    DWORD nEliminated = 0;
    BOOL  bEliminate = FALSE;
    PRIM* pPrim;
    PRIM* pNextPrim;

    if( nPrims < 2 )
        return;

    // every primitive against its successor ...
    pPrim = pLoop->PrimBuf;

    for( nPrims--; nPrims != 0; nPrims--, pPrim = pNextPrim )
    {
        pNextPrim = pPrim + 1;
        nVerts = pPrim->nVerts;
        bEliminate = FALSE;

        if( nVerts > 1 &&
            ( pPrim->primType == PRIM_CURVE || pNextPrim->primType == PRIM_CURVE ) )
        {
            POINT2D* p = &pPrim->pVert[nVerts - 2];

            if( PointsColinear( p, p + 1, &pNextPrim->pVert[1] ) )
            {
                // drop the last vertex of this primitive
                nVerts--;
                bEliminate = TRUE;
                pPrim->nVerts = nVerts;
            }
        }

        if( nEliminated != 0 )
        {
            // move the vertices down over the ones dropped so far
            POINT2D* pNewVert = pPrim->pVert - nEliminated;
            memmove( pNewVert + 1, pPrim->pVert + 1, (size_t)( nVerts - 1 ) * sizeof(POINT2D) );
            pPrim->pVert = pNewVert;
        }

        if( bEliminate )
            nEliminated++;
    }

    // ... and the last one against the first
    PRIM* pFirstPrim = pLoop->PrimBuf;
    BOOL  bEliminateLast = FALSE;

    nVerts = pPrim->nVerts;

    if( nVerts > 1 &&
        ( pPrim->primType == PRIM_CURVE || pFirstPrim->primType == PRIM_CURVE ) )
    {
        POINT2D* p = &pPrim->pVert[nVerts - 2];
        POINT2D* pLast = p + 1;

        // if this primitive has just one segment and its first vertex went
        // away a moment ago, the vertex before it takes its place
        if( nVerts == 2 && bEliminate )
            p--;

        if( PointsColinear( p, pLast, &pFirstPrim->pVert[1] ) )
        {
            nVerts--;
            bEliminateLast = TRUE;
            pPrim->nVerts = nVerts;

            // the loop now starts (and ends) one vertex earlier
            pFirstPrim->pVert[0] = *p;
        }
    }

    if( nEliminated != 0 )
    {
        POINT2D* pNewVert = pPrim->pVert - nEliminated;
        memmove( pNewVert + 1, pPrim->pVert + 1, (size_t)( nVerts - 1 ) * sizeof(POINT2D) );
        pPrim->pVert = pNewVert;
    }

    if( bEliminateLast )
        nEliminated++;

    pLoop->nVerts -= nEliminated;

    // primitives with a single vertex have no segment any more
    DWORD nEmpty = 0;

    pPrim = pLoop->PrimBuf;
    for( nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrim++ )
    {
        if( pPrim->nVerts == 1 )
            nEmpty++;
        else
            memmove( pPrim - nEmpty, pPrim, sizeof(PRIM) );
    }

    pLoop->nPrims -= nEmpty;
}


//-----------------------------------------------------------------------------
// Name: PointsColinear()
//-----------------------------------------------------------------------------
static BOOL PointsColinear( POINT2D* p1, POINT2D* p2, POINT2D* p3 )
{
    POINT2D v1, v2;

    // coincident points are as good as colinear ones
    if( ( p1->x == p2->x && p1->y == p2->y ) ||
        ( p2->x == p3->x && p2->y == p3->y ) )
    {
        return TRUE;
    }

    v1.x = p2->x - p1->x;
    v1.y = p2->y - p1->y;
    v2.x = p3->x - p2->x;
    v2.y = p3->y - p2->y;

    if( fabs( CalcAngle( &v1, &v2 ) ) < COLINEAR_THRESHOLD_ANGLE )
        return TRUE;

    return FALSE;
}


//-----------------------------------------------------------------------------
// Name: CreateOFContext()
//-----------------------------------------------------------------------------
static HRESULT CreateOFContext( HDC hdc, FLOAT chordalDeviation, FLOAT extrusion, INT format,
                                OFContext** ppOFContext )
{
    HRESULT    hr = S_OK;
    OFContext* ofc = (OFContext*)calloc( sizeof(OFContext), 1 );

    if( ofc == NULL )
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        ofc->chordalDeviation = chordalDeviation;
        ofc->format = format;

        if( !ScaleFont( hdc, ofc ) )
        {
            hr = E_FAIL;
        }
        else if( ( ofc->ec = extr_Init( extrusion ) ) == NULL )
        {
            hr = E_FAIL;
        }
        else
        {
            GLUtesselator* tess;

            ofc->tess = NULL;

            tess = gluNewTess();
            if( tess == NULL )
            {
                hr = E_OUTOFMEMORY;
            }
            else
            {
                gluTessCallback( tess, GLU_TESS_BEGIN_DATA,   (void (CALLBACK*)())extr_glBegin );
                gluTessCallback( tess, GLU_TESS_END,          (void (CALLBACK*)())extr_glEnd );
                gluTessCallback( tess, GLU_TESS_VERTEX_DATA,  (void (CALLBACK*)())extr_glVertex );
                gluTessCallback( tess, GLU_TESS_ERROR_DATA,   (void (CALLBACK*)())TessError );
                gluTessCallback( tess, GLU_TESS_COMBINE_DATA, (void (CALLBACK*)())TessCombine );

                gluTessNormal( tess, 0.0, 0.0, 1.0 );
                gluTessProperty( tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO );

                ofc->tess = tess;

                *ppOFContext = ofc;
                ofc = NULL;
            }
        }
    }

    if( ofc != NULL )
        DestroyOFContext( hdc, ofc );

    return hr;
}


//-----------------------------------------------------------------------------
// Name: ScaleFont()
// Desc: Replaces the font in the DC by one whose height is the em square, so
//       that the outlines come in font design units, without hinting.
//-----------------------------------------------------------------------------
static BOOL ScaleFont( HDC hdc, OFContext* ofc )
{
    OUTLINETEXTMETRIC otm;
    LOGFONT           lf;
    HFONT             hfont;

    if( GetOutlineTextMetrics( hdc, sizeof(otm), &otm ) == 0 )
        return FALSE;

    GetObject( GetCurrentObject( hdc, OBJ_FONT ), sizeof(lf), &lf );

    lf.lfHeight = otm.otmEMSquare;
    lf.lfWidth = 0;

    hfont = CreateFontIndirect( &lf );
    ofc->hfontOld = (HFONT)SelectObject( hdc, hfont );

    ofc->scale = 1.0f / (FLOAT)otm.otmEMSquare;

    if( ofc->chordalDeviation == 0.0f )
        ofc->chordalDeviation = 1.0f / (FLOAT)otm.otmEMSquare;
    else if( ofc->chordalDeviation > MAX_CHORDAL_DEVIATION )
        ofc->chordalDeviation = MAX_CHORDAL_DEVIATION;

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: DestroyOFContext()
//-----------------------------------------------------------------------------
static void DestroyOFContext( HDC hdc, OFContext* ofc )
{
    if( ofc->ec != NULL )
        extr_Finish( ofc->ec );

    // give the caller's font back
    if( ofc->hfontOld != NULL )
    {
        HFONT hfont = (HFONT)SelectObject( hdc, ofc->hfontOld );
        if( hfont != NULL )
            DeleteObject( hfont );
    }

    if( ofc->format == OF_FORMAT_POLYGONS && ofc->tess != NULL )
        gluDeleteTess( ofc->tess );

    free( ofc );
}


//-----------------------------------------------------------------------------
// Name: NewLoop()
// Desc: Starts a new loop at the given point.
//-----------------------------------------------------------------------------
static LOOP* NewLoop( LOOP_LIST* pLoopList, POINT2D* pFirstPoint )
{
    LOOP*    pLoop;
    PRIM*    pPrims;
    POINT2D* pVerts;

    if( pLoopList->nLoops >= pLoopList->LoopBufSize )
    {
        pLoopList->LoopBufSize += LOOP_BUF_INCREMENT;

        pLoop = (LOOP*)realloc( pLoopList->LoopBuf, (size_t)pLoopList->LoopBufSize * sizeof(LOOP) );
        if( pLoop == NULL )
            return NULL;

        pLoopList->LoopBuf = pLoop;
    }

    pLoop = &pLoopList->LoopBuf[pLoopList->nLoops];

    pPrims = (PRIM*)malloc( PRIM_BUF_INCREMENT * sizeof(PRIM) );
    if( pPrims == NULL )
        return NULL;

    pLoop->nPrims = 0;
    pLoop->PrimBuf = pPrims;
    pLoop->PrimBufSize = PRIM_BUF_INCREMENT;

    pVerts = (POINT2D*)malloc( VERT_BUF_INITIAL * sizeof(POINT2D) );
    if( pVerts == NULL )
    {
        free( pPrims );
        return NULL;
    }

    pLoop->nVerts = 0;
    pLoop->VertBuf = pVerts;
    pLoop->VertBufSize = VERT_BUF_INITIAL;

    pVerts[0].x = pFirstPoint->x;
    pVerts[0].y = pFirstPoint->y;
    pLoop->nVerts++;

    pLoop->FNormBuf = NULL;
    pLoop->VNormBuf = NULL;

    pLoopList->nLoops++;

    return pLoop;
}


//-----------------------------------------------------------------------------
// Name: NewPrim()
// Desc: Starts a new primitive at the last vertex of the loop.
//-----------------------------------------------------------------------------
static PRIM* NewPrim( LOOP* pLoop, DWORD primType )
{
    PRIM* pPrim;

    if( pLoop->nPrims >= pLoop->PrimBufSize )
    {
        pLoop->PrimBufSize += PRIM_BUF_INCREMENT;

        pPrim = (PRIM*)realloc( pLoop->PrimBuf, (size_t)pLoop->PrimBufSize * sizeof(PRIM) );
        if( pPrim == NULL )
            return NULL;

        pLoop->PrimBuf = pPrim;
    }

    pPrim = &pLoop->PrimBuf[pLoop->nPrims];

    pPrim->nVerts = 1;
    pPrim->primType = ( primType == TT_PRIM_LINE ) ? PRIM_LINE : PRIM_CURVE;
    pPrim->pFNorm = NULL;
    pPrim->pVNorm = NULL;
    pPrim->vertIndex = pLoop->nVerts - 1;

    pLoop->nPrims++;

    return pPrim;
}


//-----------------------------------------------------------------------------
// Name: FreeLoopList()
//-----------------------------------------------------------------------------
static void FreeLoopList( LOOP_LIST* pLoopList )
{
    if( pLoopList == NULL )
        return;

    if( pLoopList->LoopBuf != NULL )
    {
        LOOP* pLoop = pLoopList->LoopBuf;

        for( DWORD nLoops = pLoopList->nLoops; nLoops != 0; nLoops--, pLoop++ )
        {
            if( pLoop->PrimBuf != NULL )
                free( pLoop->PrimBuf );
            if( pLoop->VertBuf != NULL )
                free( pLoop->VertBuf );
        }

        free( pLoopList->LoopBuf );
    }

    free( pLoopList );
}


//-----------------------------------------------------------------------------
// Name: AppendToVertBuf()
// Desc: Adds a vertex to the loop, and to its current primitive.
//-----------------------------------------------------------------------------
static BOOL AppendToVertBuf( LOOP* pLoop, PRIM* pPrim, POINT2D* p )
{
    if( pLoop->nVerts >= pLoop->VertBufSize )
    {
        pLoop->VertBufSize += VERT_BUF_INCREMENT;

        POINT2D* pVerts = (POINT2D*)realloc( pLoop->VertBuf, (size_t)pLoop->VertBufSize * sizeof(POINT2D) );
        if( pVerts == NULL )
            return FALSE;

        pLoop->VertBuf = pVerts;
    }

    pLoop->VertBuf[pLoop->nVerts] = *p;
    pLoop->nVerts++;
    pPrim->nVerts++;

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: class CFaceContext
//-----------------------------------------------------------------------------
CFaceContext::CFaceContext()
{
    m_pVertices = NULL;
    m_cVertices = 0;
    m_cVerticesMax = 0;
    m_pFaces = NULL;
    m_cFaces = 0;
    m_cFacesMax = 0;
    m_vNormal.x = 0.0f;
    m_vNormal.y = 0.0f;
    m_vNormal.z = 0.0f;
    m_fZ = 0.0f;
}


CFaceContext::~CFaceContext()
{
    delete[] m_pVertices;
    delete[] m_pFaces;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::Init()
// Desc: Empties the context (the buffers are kept).
//-----------------------------------------------------------------------------
HRESULT CFaceContext::Init()
{
    m_cFaces = 0;
    m_cVertices = 0;

    if( m_cFacesMax == 0 )
    {
        m_cFacesMax = FACE_CONTEXT_INITIAL;
        m_pFaces = new( std::nothrow ) FACE[FACE_CONTEXT_INITIAL];
        if( m_pFaces == NULL )
            return E_OUTOFMEMORY;
    }

    if( m_cVerticesMax == 0 )
    {
        m_cVerticesMax = FACE_CONTEXT_INITIAL;
        m_pVertices = new( std::nothrow ) VERTEX[FACE_CONTEXT_INITIAL];
        if( m_pVertices == NULL )
            return E_OUTOFMEMORY;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::ResizeFaces()
//-----------------------------------------------------------------------------
HRESULT CFaceContext::ResizeFaces( UINT cFaces )
{
    m_cFacesMax = cFaces;

    FACE* pFaces = new( std::nothrow ) FACE[cFaces];
    if( pFaces == NULL )
        return E_OUTOFMEMORY;

    memcpy( pFaces, m_pFaces, (size_t)m_cFaces * sizeof(FACE) );
    delete[] m_pFaces;
    m_pFaces = pFaces;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::ResizeVertices()
//-----------------------------------------------------------------------------
HRESULT CFaceContext::ResizeVertices( UINT cVertices )
{
    m_cVerticesMax = cVertices;

    VERTEX* pVertices = new( std::nothrow ) VERTEX[cVertices];
    if( pVertices == NULL )
        return E_OUTOFMEMORY;

    memcpy( (void*)pVertices, (void*)m_pVertices, (size_t)m_cVertices * sizeof(VERTEX) );
    delete[] m_pVertices;
    m_pVertices = pVertices;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::SetupPrim()
// Desc: Makes room for a primitive of a face and adds its vertices, at the
//       current depth and with the current normal.
//-----------------------------------------------------------------------------
HRESULT CFaceContext::SetupPrim( D3DXVECTOR2* pVertices, UINT cVertices, UINT cFaces, UINT* piFirstVertex )
{
    HRESULT hr;

    if( m_cVertices + cVertices > m_cVerticesMax )
    {
        hr = ResizeVertices( m_cVertices + cVertices );
        if( FAILED( hr ) )
            return hr;
    }

    if( m_cFaces + cFaces > m_cFacesMax )
    {
        hr = ResizeFaces( m_cFaces + cFaces );
        if( FAILED( hr ) )
            return hr;
    }

    *piFirstVertex = m_cVertices;

    VERTEX* pVertex = &m_pVertices[m_cVertices];

    for( UINT i = cVertices; i != 0; i--, pVertex++, pVertices++ )
    {
        pVertex->position.x = pVertices->x;
        pVertex->position.y = pVertices->y;
        pVertex->position.z = m_fZ;
        pVertex->normal.x = m_vNormal.x;
        pVertex->normal.y = m_vNormal.y;
        pVertex->normal.z = m_vNormal.z;
    }

    m_cVertices += cVertices;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::AddQuad()
// Desc: Adds a quad (two triangles) with its own four vertices.
//-----------------------------------------------------------------------------
HRESULT CFaceContext::AddQuad( D3DXVECTOR3* pVertices, D3DXVECTOR3* pNormals, BOOL bReverse )
{
    const UINT* pOrder = bReverse ? s_rgOrderReversed : s_rgOrder;
    HRESULT     hr;

    if( m_cVertices + 4 > m_cVerticesMax )
    {
        hr = ResizeVertices( m_cVertices + 4 );
        if( FAILED( hr ) )
            return hr;
    }

    if( m_cFaces + 2 > m_cFacesMax )
    {
        hr = ResizeFaces( m_cFaces + 2 );
        if( FAILED( hr ) )
            return hr;
    }

    VERTEX* pVertex = &m_pVertices[m_cVertices];

    for( UINT i = 0; i < 4; i++, pVertex++ )
    {
        pVertex->position.x = pVertices[i].x;
        pVertex->position.y = pVertices[i].y;
        pVertex->position.z = pVertices[i].z;
        pVertex->normal.x = pNormals[i].x;
        pVertex->normal.y = pNormals[i].y;
        pVertex->normal.z = pNormals[i].z;
    }

    m_pFaces[m_cFaces].v[pOrder[0]] = (WORD)m_cVertices;
    m_pFaces[m_cFaces].v[pOrder[1]] = (WORD)( m_cVertices + 2 );
    m_pFaces[m_cFaces].v[pOrder[2]] = (WORD)( m_cVertices + 1 );
    m_cFaces++;

    m_pFaces[m_cFaces].v[pOrder[0]] = (WORD)m_cVertices;
    m_pFaces[m_cFaces].v[pOrder[1]] = (WORD)( m_cVertices + 3 );
    m_pFaces[m_cFaces].v[pOrder[2]] = (WORD)( m_cVertices + 2 );
    m_cFaces++;

    m_cVertices += 4;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::AddQuadStrip()
// Desc: Adds a strip of quads; the vertices come in pairs (front, back).
//-----------------------------------------------------------------------------
HRESULT CFaceContext::AddQuadStrip( D3DXVECTOR3* pVertices, D3DXVECTOR3* pNormals, UINT cVertices,
                                    BOOL bReverse )
{
    const UINT* pOrder = bReverse ? s_rgOrderReversed : s_rgOrder;
    HRESULT     hr;

    if( m_cVertices + cVertices > m_cVerticesMax )
    {
        hr = ResizeVertices( m_cVertices + cVertices );
        if( FAILED( hr ) )
            return hr;
    }

    if( m_cFaces + ( cVertices - 2 ) > m_cFacesMax )
    {
        hr = ResizeFaces( m_cFaces + ( cVertices - 2 ) );
        if( FAILED( hr ) )
            return hr;
    }

    UINT iVertex = m_cVertices;

    for( UINT i = 0; i < cVertices; i++, iVertex++ )
    {
        m_pVertices[iVertex].position.x = pVertices[i].x;
        m_pVertices[iVertex].position.y = pVertices[i].y;
        m_pVertices[iVertex].position.z = pVertices[i].z;
        m_pVertices[iVertex].normal.x = pNormals[i].x;
        m_pVertices[iVertex].normal.y = pNormals[i].y;
        m_pVertices[iVertex].normal.z = pNormals[i].z;
    }

    WORD wVertex = (WORD)m_cVertices;

    for( UINT cQuads = ( cVertices - 2 ) / 2; cQuads != 0; cQuads--, wVertex += 2 )
    {
        m_pFaces[m_cFaces].v[pOrder[0]] = wVertex;
        m_pFaces[m_cFaces].v[pOrder[1]] = wVertex + 3;
        m_pFaces[m_cFaces].v[pOrder[2]] = wVertex + 1;
        m_cFaces++;

        m_pFaces[m_cFaces].v[pOrder[0]] = wVertex;
        m_pFaces[m_cFaces].v[pOrder[1]] = wVertex + 2;
        m_pFaces[m_cFaces].v[pOrder[2]] = wVertex + 3;
        m_cFaces++;
    }

    m_cVertices += cVertices;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::AddTriangles()
// Desc: Adds a GL_TRIANGLES primitive of a face.
//-----------------------------------------------------------------------------
HRESULT CFaceContext::AddTriangles( D3DXVECTOR2* pVertices, UINT cVertices, BOOL bReverse )
{
    const UINT* pOrder = bReverse ? s_rgOrderReversed : s_rgOrder;
    UINT        cFaces = cVertices / 3;
    UINT        iVertex;
    HRESULT     hr;

    hr = SetupPrim( pVertices, cVertices, cFaces, &iVertex );
    if( FAILED( hr ) )
        return hr;

    for( ; cFaces != 0; cFaces--, iVertex += 3 )
    {
        FACE face;

        face.v[pOrder[0]] = (WORD)iVertex;
        face.v[pOrder[1]] = (WORD)( iVertex + 2 );
        face.v[pOrder[2]] = (WORD)( iVertex + 1 );

        m_pFaces[m_cFaces] = face;
        m_cFaces++;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::AddTriangleStrip()
// Desc: Adds a GL_TRIANGLE_STRIP primitive of a face.
//-----------------------------------------------------------------------------
HRESULT CFaceContext::AddTriangleStrip( D3DXVECTOR2* pVertices, UINT cVertices, BOOL bReverse )
{
    const UINT* pOrder = bReverse ? s_rgOrderReversed : s_rgOrder;
    UINT        cFaces = cVertices - 2;
    UINT        iVertex;
    HRESULT     hr;

    if( cVertices <= 2 )
        return S_OK;

    hr = SetupPrim( pVertices, cVertices, cFaces, &iVertex );
    if( FAILED( hr ) )
        return hr;

    for( UINT i = 0; i < cFaces; i++, iVertex++ )
    {
        FACE face;

        if( ( i & 1 ) == 0 )
        {
            face.v[pOrder[0]] = (WORD)iVertex;
            face.v[pOrder[1]] = (WORD)( iVertex + 2 );
            face.v[pOrder[2]] = (WORD)( iVertex + 1 );
        }
        else
        {
            face.v[pOrder[0]] = (WORD)( iVertex + 1 );
            face.v[pOrder[1]] = (WORD)( iVertex + 2 );
            face.v[pOrder[2]] = (WORD)iVertex;
        }

        m_pFaces[m_cFaces] = face;
        m_cFaces++;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CFaceContext::AddTriangleFan()
// Desc: Adds a GL_TRIANGLE_FAN primitive of a face.
//-----------------------------------------------------------------------------
HRESULT CFaceContext::AddTriangleFan( D3DXVECTOR2* pVertices, UINT cVertices, BOOL bReverse )
{
    const UINT* pOrder = bReverse ? s_rgOrderReversed : s_rgOrder;
    UINT        cFaces = cVertices - 2;
    UINT        iFirstVertex;
    UINT        iVertex;
    HRESULT     hr;

    if( cVertices <= 2 )
        return S_OK;

    hr = SetupPrim( pVertices, cVertices, cFaces, &iFirstVertex );
    if( FAILED( hr ) )
        return hr;

    iVertex = iFirstVertex;

    for( ; cFaces != 0; cFaces-- )
    {
        FACE face;

        iVertex++;

        face.v[pOrder[0]] = (WORD)iFirstVertex;
        face.v[pOrder[1]] = (WORD)( iVertex + 1 );
        face.v[pOrder[2]] = (WORD)iVertex;

        m_pFaces[m_cFaces] = face;
        m_cFaces++;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: GenerateMesh()
// Desc: Copies the triangles of all glyphs into one mesh.
//-----------------------------------------------------------------------------
static HRESULT GenerateMesh( LPDIRECT3DDEVICE9 pDevice, CFaceContext** ppFaceContexts, UINT cFaceContexts,
                             LPD3DXMESH* ppMesh )
{
    HRESULT                 hr;
    LPD3DXMESH              pMesh = NULL;
    LPDIRECT3DVERTEXBUFFER9 pVB = NULL;
    CFaceContext::VERTEX*   pVertices = NULL;
    WORD*                   pIndices = NULL;
    D3DVERTEXELEMENT9       decl[MAX_FVF_DECL_SIZE];
    UINT                    cFaces = 0;
    UINT                    cVertices = 0;
    UINT                    i;

    for( i = 0; i < cFaceContexts; i++ )
    {
        cFaces += ppFaceContexts[i]->m_cFaces;
        cVertices += ppFaceContexts[i]->m_cVertices;
    }

    hr = D3DXDeclaratorFromFVF( D3DFVF_XYZ | D3DFVF_NORMAL, decl );

    if( SUCCEEDED( hr ) )
        hr = D3DXCreateMesh( cFaces, cVertices, D3DXMESH_MANAGED, decl, pDevice, &pMesh );

    if( SUCCEEDED( hr ) )
        hr = pMesh->GetVertexBuffer( &pVB );

    if( SUCCEEDED( hr ) )
        hr = pVB->Lock( 0, 0, (void**)&pVertices, D3DLOCK_NOSYSLOCK );

    if( SUCCEEDED( hr ) )
        hr = pMesh->LockIndexBuffer( 0, (void**)&pIndices );

    if( SUCCEEDED( hr ) )
    {
        CFaceContext::VERTEX* pVertex = pVertices;
        WORD*                 pIndex = pIndices;
        UINT                  iBaseVertex = 0;

        for( i = 0; i < cFaceContexts; i++ )
        {
            CFaceContext* pFaceContext = ppFaceContexts[i];
            UINT          cGlyphFaces = pFaceContext->m_cFaces;
            UINT          cGlyphVertices = pFaceContext->m_cVertices;

            memcpy( pIndex, pFaceContext->m_pFaces, (size_t)cGlyphFaces * sizeof(CFaceContext::FACE) );
            memcpy( (void*)pVertex, (void*)pFaceContext->m_pVertices,
                    (size_t)cGlyphVertices * sizeof(CFaceContext::VERTEX) );

            // the indices of a glyph start at zero
            for( UINT iFace = 0; iFace < cGlyphFaces; iFace++ )
            {
                pIndex[iFace * 3 + 0] = (WORD)( pIndex[iFace * 3 + 0] + iBaseVertex );
                pIndex[iFace * 3 + 1] = (WORD)( pIndex[iFace * 3 + 1] + iBaseVertex );
                pIndex[iFace * 3 + 2] = (WORD)( pIndex[iFace * 3 + 2] + iBaseVertex );
            }

            iBaseVertex += cGlyphVertices;
            pIndex += cGlyphFaces * 3;
            pVertex += cGlyphVertices;
        }

        *ppMesh = pMesh;
        pMesh->AddRef();
    }

    if( pIndices != NULL )
        pMesh->UnlockIndexBuffer();

    if( pMesh != NULL )
        pMesh->Release();

    if( pVertices != NULL )
        pVB->Unlock();

    if( pVB != NULL )
        pVB->Release();

    return hr;
}

} // namespace Shape3D


//-----------------------------------------------------------------------------
// Name: D3DXCreateTextAW()
// Desc: The worker behind the ANSI and the Unicode entry point.  In
//       ssText3d.scr it is only ever called with bUnicode set, and the
//       optimiser specialised it for that; the ANSI branches below follow
//       from the Unicode ones.
//-----------------------------------------------------------------------------
static HRESULT D3DXCreateTextAW( LPDIRECT3DDEVICE9 pDevice, HDC hDC, const void* pText, FLOAT Deviation,
                                 FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                                 LPGLYPHMETRICSFLOAT pGlyphMetrics, BOOL bUnicode )
{
    HRESULT         hr;
    OFContext*      ofc = NULL;
    UCHAR*          pGlyphBuf = NULL;
    DWORD           cbGlyphBuf = 0;
    CFaceContext**  ppFaceContexts = NULL;
    UINT            cGlyphs = 0;
    FLOAT           fX = 0.0f;
    FLOAT           fY = 0.0f;
    GLYPHMETRICS    gm;
    UINT            i;

    if( pText == NULL ||
        ( bUnicode ? *(const WCHAR*)pText == L'\0' : *(const CHAR*)pText == '\0' ) )
    {
        return D3DERR_INVALIDCALL;
    }

    if( ppMesh == NULL || Deviation < 0.0f || Extrusion < 0.0f )
        return D3DERR_INVALIDCALL;

    *ppMesh = NULL;

    // Only the Unicode path is left in ssText3d.scr, and its callers never
    // ask for adjacency information: whatever the original did with a
    // non-NULL ppAdjacency was removed by the optimiser.
    if( ppAdjacency != NULL )
        *ppAdjacency = NULL;

    cGlyphs = bUnicode ? (UINT)wcslen( (const WCHAR*)pText ) : (UINT)strlen( (const CHAR*)pText );

    ppFaceContexts = new( std::nothrow ) CFaceContext*[cGlyphs];
    if( ppFaceContexts == NULL )
    {
        hr = E_OUTOFMEMORY;
        goto LCleanup;
    }
    memset( ppFaceContexts, 0, (size_t)cGlyphs * sizeof(CFaceContext*) );

    hr = CreateOFContext( hDC, Deviation, Extrusion, OF_FORMAT_POLYGONS, &ofc );
    if( FAILED( hr ) )
        goto LCleanup;

    for( i = 0; i < cGlyphs; i++ )
    {
        UINT  uChar = bUnicode ? ( (const WCHAR*)pText )[i] : (BYTE)( (const CHAR*)pText )[i];
        DWORD cbGlyph;
        DWORD dwResult;

        // how much data is it?
        cbGlyph = bUnicode ? GetGlyphOutlineW( hDC, uChar, GGO_NATIVE, &gm, 0, NULL, &s_matIdentity )
                           : GetGlyphOutlineA( hDC, uChar, GGO_NATIVE, &gm, 0, NULL, &s_matIdentity );
        if( cbGlyph == GDI_ERROR )
        {
            hr = E_OUTOFMEMORY;
            goto LCleanup;
        }

        if( cbGlyph > cbGlyphBuf )
        {
            if( pGlyphBuf != NULL )
                free( pGlyphBuf );

            pGlyphBuf = (UCHAR*)malloc( cbGlyph );
            cbGlyphBuf = cbGlyph;

            if( pGlyphBuf == NULL )
            {
                hr = E_OUTOFMEMORY;
                goto LCleanup;
            }
        }

        dwResult = bUnicode ? GetGlyphOutlineW( hDC, uChar, GGO_NATIVE, &gm, cbGlyphBuf, pGlyphBuf, &s_matIdentity )
                            : GetGlyphOutlineA( hDC, uChar, GGO_NATIVE, &gm, cbGlyphBuf, pGlyphBuf, &s_matIdentity );
        if( dwResult == GDI_ERROR )
        {
            hr = E_FAIL;
            goto LCleanup;
        }

        ofc->glyphBuf = pGlyphBuf;
        ofc->glyphSize = cbGlyph;

        hr = MakeDisplayListFromGlyph( ofc );
        if( FAILED( hr ) )
            goto LCleanup;

        // move the glyph to its place in the string ...
        CFaceContext* pFaceContext = ofc->ec->pFaceContext;

        for( UINT iVertex = 0; iVertex < pFaceContext->m_cVertices; iVertex++ )
        {
            pFaceContext->m_pVertices[iVertex].position.x += fX;
            pFaceContext->m_pVertices[iVertex].position.y += fY;
            pFaceContext->m_pVertices[iVertex].position.z += 0.0f;
        }

        // ... keep it, and get a fresh face context for the next one
        ppFaceContexts[i] = pFaceContext;

        ofc->ec->pFaceContext = new( std::nothrow ) CFaceContext;
        if( ofc->ec->pFaceContext == NULL )
        {
            hr = E_OUTOFMEMORY;
            goto LCleanup;
        }

        fX += ofc->scale * (FLOAT)gm.gmCellIncX;
        fY += (FLOAT)gm.gmCellIncY * ofc->scale;

        if( pGlyphMetrics != NULL )
        {
            pGlyphMetrics->gmfBlackBoxX = (FLOAT)gm.gmBlackBoxX * ofc->scale;
            pGlyphMetrics->gmfBlackBoxY = (FLOAT)gm.gmBlackBoxY * ofc->scale;
            pGlyphMetrics->gmfptGlyphOrigin.x = (FLOAT)gm.gmptGlyphOrigin.x * ofc->scale;
            pGlyphMetrics->gmfptGlyphOrigin.y = (FLOAT)gm.gmptGlyphOrigin.y * ofc->scale;
            pGlyphMetrics->gmfCellIncX = (FLOAT)gm.gmCellIncX * ofc->scale;
            pGlyphMetrics->gmfCellIncY = (FLOAT)gm.gmCellIncY * ofc->scale;
            pGlyphMetrics++;
        }
    }

    hr = GenerateMesh( pDevice, ppFaceContexts, cGlyphs, ppMesh );

LCleanup:
    if( pGlyphBuf != NULL )
        free( pGlyphBuf );

    if( ofc != NULL )
        DestroyOFContext( hDC, ofc );

    if( ppFaceContexts != NULL )
    {
        for( i = 0; i < cGlyphs; i++ )
        {
            if( ppFaceContexts[i] != NULL )
                delete ppFaceContexts[i];
        }
    }

    delete[] ppFaceContexts;

    return hr;
}


//-----------------------------------------------------------------------------
// Name: Shape3D::CreateTextW()
//-----------------------------------------------------------------------------
HRESULT Shape3D::CreateTextW( LPDIRECT3DDEVICE9 pDevice, HDC hDC, LPCWSTR pText, FLOAT Deviation,
                              FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                              LPGLYPHMETRICSFLOAT pGlyphMetrics )
{
    return D3DXCreateTextAW( pDevice, hDC, pText, Deviation, Extrusion, ppMesh, ppAdjacency,
                             pGlyphMetrics, TRUE );
}
