//-----------------------------------------------------------------------------
// File: fontextr.cpp
//
// Desc: Shape3D.lib - extrusion of glyph contours.
//
//       Receives the tessellated face of a glyph from the GLU tessellator,
//       builds the side walls from the contour loops (flat shaded quads for
//       straight runs, smooth shaded quad strips for curves) and adds the
//       front and the back face.
//
//       Reconstructed from ssText3d.scr 6.0.6001.18000 (amd64).  The function
//       names are the ones of the symbol file, except for extr_Init,
//       extr_Finish, extr_PolyFinish, extr_DrawPolygons, CalculateFaceNormals
//       and ConsolidatePrims, which were inlined into their callers and are
//       named by this reconstruction.
//
//       Original object:
//           shell\osshell\control\scrnsave\shape3d\objfre\amd64\fontextr.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <new>
#include "fontoutl.h"

namespace Shape3D
{

#define SHAPE3D_PI          3.14159265358979323846
#define ZERO_EPS            1e-08f

#define FACEBUF_INCREMENT   1000        // floats

static BOOL     DrawSidePolygons( EXTRContext* ec, LOOP_LIST* pLoopList );
static void     DrawPrims( EXTRContext* ec, LOOP* pLoop, BOOL bReverse );
static void     DrawQuads( EXTRContext* ec, PRIM* pPrim, BOOL bReverse );
static void     DrawFacePolygons( EXTRContext* ec, FLOAT z );
static GLenum   LoopOrientation( LOOP_LIST* pLoopList );
static BOOL     CalculateVertexNormals( LOOP* pLoop );
static BOOL     AppendToFaceBuf( EXTRContext* ec, FLOAT value );
static void     Normalize2d( POINT2D* p );


//-----------------------------------------------------------------------------
// Name: extr_Init()
// Desc: Creates the extrusion context.
//-----------------------------------------------------------------------------
EXTRContext* extr_Init( FLOAT extrusion )
{
    EXTRContext* ec = (EXTRContext*)calloc( sizeof(EXTRContext), 1 );
    if( ec == NULL )
        return NULL;

    ec->bFacePolys = TRUE;
    ec->zExtrusion = -extrusion;

    ec->pFaceContext = new( std::nothrow ) CFaceContext;
    if( ec->pFaceContext == NULL )
    {
        free( ec );
        return NULL;
    }

    return ec;
}


//-----------------------------------------------------------------------------
// Name: extr_Finish()
//-----------------------------------------------------------------------------
void extr_Finish( EXTRContext* ec )
{
    if( ec->pFaceContext != NULL )
        delete ec->pFaceContext;

    free( ec );
}


//-----------------------------------------------------------------------------
// Name: extr_PolyInit()
// Desc: Gets ready for the next glyph.  Returns zero on failure.
//-----------------------------------------------------------------------------
int extr_PolyInit( EXTRContext* ec )
{
    if( !ec->bFacePolys )
        return 1;

    ec->FaceBuf = NULL;
    ec->FaceBuf = (FLOAT*)malloc( FACEBUF_INCREMENT * sizeof(FLOAT) );
    if( ec->FaceBuf == NULL )
        return 0;

    ec->FaceBufIndex = 0;
    ec->FaceBufSize = FACEBUF_INCREMENT;

    // the first entry counts the primitives
    if( !AppendToFaceBuf( ec, 0.0f ) )
        return 0;

    if( FAILED( ec->pFaceContext->Init() ) )
        return 0;

    ec->hr = S_OK;
    return 1;
}


//-----------------------------------------------------------------------------
// Name: extr_PolyFinish()
//-----------------------------------------------------------------------------
void extr_PolyFinish( EXTRContext* ec )
{
    if( ec->bFacePolys && ec->FaceBuf != NULL )
    {
        free( ec->FaceBuf );
        ec->FaceBuf = NULL;
    }
}


//-----------------------------------------------------------------------------
// Name: extr_glBegin()
// Desc: GLU_TESS_BEGIN_DATA callback: a new primitive of the face starts.
//-----------------------------------------------------------------------------
void CALLBACK extr_glBegin( GLenum primType, void* data )
{
    EXTRContext* ec = ( (OFContext*)data )->ec;

    // one more primitive
    ec->FaceBuf[0] += 1.0f;

    // its vertex count follows the type
    ec->FaceVertexCountIndex = ec->FaceBufIndex + 1;

    if( !AppendToFaceBuf( ec, (FLOAT)primType ) ||
        !AppendToFaceBuf( ec, 0.0f ) )
    {
        ec->hr = E_OUTOFMEMORY;
    }
}


//-----------------------------------------------------------------------------
// Name: extr_glVertex()
// Desc: GLU_TESS_VERTEX_DATA callback.  The vertex data is a POINT2D.
//-----------------------------------------------------------------------------
void CALLBACK extr_glVertex( GLfloat* v, void* data )
{
    EXTRContext* ec = ( (OFContext*)data )->ec;

    if( !AppendToFaceBuf( ec, v[0] ) ||
        !AppendToFaceBuf( ec, v[1] ) )
    {
        ec->hr = E_OUTOFMEMORY;
    }

    ec->FaceBuf[ec->FaceVertexCountIndex] += 1.0f;
}


//-----------------------------------------------------------------------------
// Name: extr_glEnd()
// Desc: GLU_TESS_END callback
//-----------------------------------------------------------------------------
void CALLBACK extr_glEnd()
{
}


//-----------------------------------------------------------------------------
// Name: extr_DrawPolygons()
// Desc: Emits the triangles of the glyph whose face the tessellator has just
//       delivered: the side walls, then the front and the back face.
//-----------------------------------------------------------------------------
HRESULT extr_DrawPolygons( EXTRContext* ec, LOOP_LIST* pLoopList )
{
    if( FAILED( ec->hr ) )
        return ec->hr;

    if( DrawSidePolygons( ec, pLoopList ) )
    {
        DrawFacePolygons( ec, 0.0f );
        DrawFacePolygons( ec, ec->zExtrusion );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CalculateFaceNormals()
// Desc: One normal for every segment of the loop, perpendicular to it and
//       pointing out of the glyph.  They are not normalized here.
//-----------------------------------------------------------------------------
static inline void CalculateFaceNormals( LOOP* pLoop, GLenum orientation )
{
    static POINT2D  vec;
    POINT3D*        pNorm = pLoop->FNormBuf;
    PRIM*           pPrim = pLoop->PrimBuf;

    for( DWORD nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrim++ )
    {
        POINT2D* pVert = pPrim->pVert;

        pPrim->pFNorm = pNorm;

        for( DWORD nSegments = pPrim->nVerts - 1; nSegments != 0; nSegments--, pVert++, pNorm++ )
        {
            vec.x = pVert[1].x - pVert[0].x;
            vec.y = pVert[1].y - pVert[0].y;

            if( orientation == GL_CW )
            {
                pNorm->x = -vec.y;
                pNorm->y = vec.x;
            }
            else
            {
                pNorm->x = vec.y;
                pNorm->y = -vec.x;
            }
            pNorm->z = 0.0f;
        }
    }
}


//-----------------------------------------------------------------------------
// Name: ConsolidatePrims()
// Desc: Merges neighbouring primitives that can be drawn as one: two
//       polylines, or two curves that meet at less than 90 degrees.
//-----------------------------------------------------------------------------
static inline void ConsolidatePrims( LOOP* pLoop )
{
    DWORD nErased = 0;

    if( pLoop->nPrims <= 1 )
        return;

    PRIM* pPrim = pLoop->PrimBuf;

    for( DWORD nPairs = pLoop->nPrims - 1; nPairs != 0; nPairs--, pPrim++ )
    {
        PRIM* pNextPrim = pPrim + 1;
        BOOL  bJoin = FALSE;

        if( pPrim->primType == PRIM_LINE )
        {
            if( pNextPrim->primType == PRIM_LINE )
                bJoin = TRUE;
        }
        else if( pNextPrim->primType != PRIM_LINE )
        {
            double angle = CalcAngle( (POINT2D*)&pPrim->pFNorm[pPrim->nVerts - 2],
                                      (POINT2D*)pNextPrim->pFNorm );
            if( fabs( angle ) < SHAPE3D_PI / 2.0 )
                bJoin = TRUE;
        }

        if( bJoin )
        {
            // the next primitive swallows this one
            pNextPrim->nVerts += pPrim->nVerts - 1;
            pNextPrim->pVert = pPrim->pVert;
            pNextPrim->pFNorm = pPrim->pFNorm;
            pPrim->nVerts = 0;
            nErased++;
        }
    }

    if( nErased != 0 )
    {
        // close the gaps
        DWORD nPrims = pLoop->nPrims;
        PRIM* pSrc = pLoop->PrimBuf;
        DWORD nSkipped = 0;

        pLoop->nPrims = nPrims - nErased;

        for( ; nPrims != 0; nPrims--, pSrc++ )
        {
            if( pSrc->nVerts == 0 )
                nSkipped++;
            else
                memmove( pSrc - nSkipped, pSrc, sizeof(PRIM) );
        }
    }
}


//-----------------------------------------------------------------------------
// Name: DrawSidePolygons()
// Desc: Builds the walls between the front and the back face.
//-----------------------------------------------------------------------------
static BOOL DrawSidePolygons( EXTRContext* ec, LOOP_LIST* pLoopList )
{
    DWORD nLoops = pLoopList->nLoops;
    if( nLoops == 0 )
        return TRUE;

    // The outermost contour tells which way the glyph is wound.
    GLenum orientation = LoopOrientation( pLoopList );

    LOOP* pLoop = pLoopList->LoopBuf;

    for( ; nLoops != 0; nLoops--, pLoop++ )
    {
        ULONGLONG cb = (ULONGLONG)pLoop->nVerts * sizeof(POINT3D);
        if( cb > 0xffffffff )
            return FALSE;

        pLoop->FNormBuf = (POINT3D*)malloc( (size_t)cb );
        if( pLoop->FNormBuf != NULL )
            CalculateFaceNormals( pLoop, orientation );

        ConsolidatePrims( pLoop );

        if( !CalculateVertexNormals( pLoop ) )
        {
            if( pLoop->FNormBuf != NULL )
                free( pLoop->FNormBuf );
            if( pLoop->VNormBuf != NULL )
                free( pLoop->VNormBuf );
            return FALSE;
        }

        DrawPrims( ec, pLoop, orientation == GL_CCW );

        if( pLoop->FNormBuf != NULL )
            free( pLoop->FNormBuf );
        if( pLoop->VNormBuf != NULL )
            free( pLoop->VNormBuf );
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: DrawPrims()
// Desc: Emits the wall of one contour.
//-----------------------------------------------------------------------------
static void DrawPrims( EXTRContext* ec, LOOP* pLoop, BOOL bReverse )
{
    PRIM* pPrim = pLoop->PrimBuf;

    for( DWORD nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrim++ )
    {
        if( pPrim->primType == PRIM_LINE )
        {
            DrawQuads( ec, pPrim, bReverse );
        }
        else if( pPrim->primType == PRIM_CURVE )
        {
            // a quad strip with a normal per vertex
            POINT2D*     pVert = pPrim->pVert;
            POINT3D*     pNorm = pPrim->pVNorm;
            DWORD        nVerts = pPrim->nVerts;
            UINT         cStripVerts = nVerts * 2;
            D3DXVECTOR3* pStripVerts = new( std::nothrow ) D3DXVECTOR3[cStripVerts];
            D3DXVECTOR3* pStripNorms = new( std::nothrow ) D3DXVECTOR3[cStripVerts];

            if( pStripVerts != NULL && pStripNorms != NULL )
            {
                D3DXVECTOR3* pv = pStripVerts;
                D3DXVECTOR3* pn = pStripNorms;

                for( ; nVerts != 0; nVerts--, pVert++, pNorm++, pv += 2, pn += 2 )
                {
                    pn[0].x = pNorm->x;
                    pn[0].y = pNorm->y;
                    pn[0].z = pNorm->z;
                    pv[0].x = pVert->x;
                    pv[0].y = pVert->y;
                    pv[0].z = 0.0f;

                    pn[1].x = pNorm->x;
                    pn[1].y = pNorm->y;
                    pn[1].z = pNorm->z;
                    pv[1].x = pVert->x;
                    pv[1].y = pVert->y;
                    pv[1].z = ec->zExtrusion;
                }

                ec->pFaceContext->AddQuadStrip( pStripVerts, pStripNorms, cStripVerts, bReverse );
            }

            delete[] pStripVerts;
            delete[] pStripNorms;
        }
    }
}


//-----------------------------------------------------------------------------
// Name: DrawQuads()
// Desc: Emits one flat shaded quad for every segment of a polyline.
//-----------------------------------------------------------------------------
static void DrawQuads( EXTRContext* ec, PRIM* pPrim, BOOL bReverse )
{
    POINT2D* pVert = pPrim->pVert;
    POINT3D* pNorm = pPrim->pFNorm;

    for( DWORD nQuads = pPrim->nVerts - 1; nQuads != 0; nQuads--, pVert++, pNorm++ )
    {
        D3DXVECTOR3 verts[4];
        D3DXVECTOR3 norms[4];

        Normalize2d( (POINT2D*)pNorm );

        verts[0] = D3DXVECTOR3( pVert[0].x, pVert[0].y, 0.0f );
        verts[1] = D3DXVECTOR3( pVert[0].x, pVert[0].y, ec->zExtrusion );
        verts[2] = D3DXVECTOR3( pVert[1].x, pVert[1].y, ec->zExtrusion );
        verts[3] = D3DXVECTOR3( pVert[1].x, pVert[1].y, 0.0f );

        norms[0] = D3DXVECTOR3( pNorm->x, pNorm->y, pNorm->z );
        norms[1] = norms[0];
        norms[2] = norms[0];
        norms[3] = norms[0];

        ec->pFaceContext->AddQuad( verts, norms, bReverse );
    }
}


//-----------------------------------------------------------------------------
// Name: DrawFacePolygons()
// Desc: Emits the tessellated face at depth z: the front face at z = 0, or
//       the back face (facing the other way) at the extrusion depth.
//-----------------------------------------------------------------------------
static void DrawFacePolygons( EXTRContext* ec, FLOAT z )
{
    FLOAT*          pBuf = ec->FaceBuf;
    BOOL            bBack = ( z != 0.0f );
    CFaceContext*   pFaceContext = ec->pFaceContext;

    pFaceContext->m_vNormal = D3DXVECTOR3( 0.0f, 0.0f, bBack ? -1.0f : 1.0f );
    pFaceContext->m_fZ = z;

    DWORD nPrims = (DWORD)*pBuf++;

    for( ; nPrims != 0; nPrims-- )
    {
        DWORD        primType = (DWORD)*pBuf++;
        DWORD        nVerts = (DWORD)*pBuf++;
        D3DXVECTOR2* pVerts = (D3DXVECTOR2*)pBuf;

        switch( primType )
        {
            case GL_TRIANGLES:
                ec->pFaceContext->AddTriangles( pVerts, nVerts, !bBack );
                break;

            case GL_TRIANGLE_STRIP:
                ec->pFaceContext->AddTriangleStrip( pVerts, nVerts, !bBack );
                break;

            case GL_TRIANGLE_FAN:
                ec->pFaceContext->AddTriangleFan( pVerts, nVerts, !bBack );
                break;
        }

        pBuf += nVerts * 2;
    }
}


//-----------------------------------------------------------------------------
// Name: LoopOrientation()
// Desc: Returns the winding (GL_CW or GL_CCW) of the outermost contour, which
//       is taken to be the widest one.
//-----------------------------------------------------------------------------
static GLenum LoopOrientation( LOOP_LIST* pLoopList )
{
    DWORD   nLoops = pLoopList->nLoops;
    double  angle = 0.0;

    if( nLoops == 0 )
        return GL_CW;

    LOOP* pMaxLoop = pLoopList->LoopBuf;

    if( nLoops != 1 )
    {
        FLOAT maxExtent = 0.0f;
        LOOP* pLoop = pLoopList->LoopBuf;

        for( ; nLoops != 0; nLoops--, pLoop++ )
        {
            POINT2D* p = pLoop->VertBuf;
            FLOAT    xMin = p->x;
            FLOAT    xMax = p->x;

            for( DWORD nVerts = pLoop->nVerts; nVerts != 0; nVerts--, p++ )
            {
                if( p->x < xMin )
                    xMin = p->x;
                else if( p->x > xMax )
                    xMax = p->x;
            }

            if( xMax - xMin > maxExtent )
            {
                pMaxLoop = pLoop;
                maxExtent = xMax - xMin;
            }
        }
    }

    DWORD nVerts = pMaxLoop->nVerts;
    if( nVerts > 2 )
    {
        // Sum up the turns along the contour.  The last vertex repeats the
        // first, so the segment before the first one ends at vertex n - 2.
        POINT2D* p = pMaxLoop->VertBuf;
        POINT2D  v1, v2;

        v1.x = p[0].x - p[nVerts - 2].x;
        v1.y = p[0].y - p[nVerts - 2].y;

        for( nVerts--; nVerts != 0; nVerts--, p++ )
        {
            v2.x = p[1].x - p[0].x;
            v2.y = p[1].y - p[0].y;

            angle += CalcAngle( &v1, &v2 );

            v1 = v2;
        }

        if( fabs( angle ) >= 1e-05 && angle > 0.0 )
            return GL_CCW;
    }

    return GL_CW;
}


//-----------------------------------------------------------------------------
// Name: CalcAngle()
// Desc: Signed angle from v1 to v2, in (-pi, pi].
//-----------------------------------------------------------------------------
double CalcAngle( POINT2D* v1, POINT2D* v2 )
{
    double a1 = 0.0;
    double a2 = 0.0;

    if( v1->y != 0.0f || v1->x != 0.0f )
        a1 = atan2f( v1->y, v1->x );

    if( v2->y != 0.0f || v2->x != 0.0f )
        a2 = atan2f( v2->y, v2->x );

    double angle = a2 - a1;

    if( angle < -SHAPE3D_PI )
        angle += 2.0 * SHAPE3D_PI;
    else if( angle > SHAPE3D_PI )
        angle -= 2.0 * SHAPE3D_PI;

    return angle;
}


//-----------------------------------------------------------------------------
// Name: CalculateVertexNormals()
// Desc: Vertex normals for the curves of a loop, for smooth shading.  Inside
//       a curve a vertex gets the sum of the two adjoining face normals;
//       where two primitives meet it depends on their types and on the angle
//       between them whether the edge stays sharp.
//-----------------------------------------------------------------------------
static BOOL CalculateVertexNormals( LOOP* pLoop )
{
    DWORD nPrims;
    DWORD nCurveVerts = 0;
    PRIM* pPrim;

    pPrim = pLoop->PrimBuf;
    for( nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrim++ )
    {
        if( pPrim->primType == PRIM_CURVE )
            nCurveVerts += pPrim->nVerts;
    }

    if( nCurveVerts == 0 )
        return TRUE;

    POINT3D* pVNorm = (POINT3D*)malloc( (size_t)nCurveVerts * sizeof(POINT3D) );
    pLoop->VNormBuf = pVNorm;
    if( pVNorm == NULL )
        return FALSE;

    // within the primitives
    pPrim = pLoop->PrimBuf;
    for( nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrim++ )
    {
        if( pPrim->primType == PRIM_LINE )
            continue;

        POINT3D* pFNorm = pPrim->pFNorm;

        pPrim->pVNorm = pVNorm;

        // the first vertex has only one face
        *pVNorm = *pFNorm;
        Normalize2d( (POINT2D*)pVNorm );
        pVNorm++;

        for( DWORD nInner = pPrim->nVerts - 2; nInner != 0; nInner--, pFNorm++, pVNorm++ )
        {
            pVNorm->x = pFNorm[0].x + pFNorm[1].x;
            pVNorm->y = pFNorm[1].y + pFNorm[0].y;
            pVNorm->z = pFNorm[1].z + pFNorm[0].z;
            Normalize2d( (POINT2D*)pVNorm );
        }

        // and so has the last one
        *pVNorm = *pFNorm;
        Normalize2d( (POINT2D*)pVNorm );
        pVNorm++;
    }

    // where the primitives meet (the loop is closed: the first one follows
    // the last one)
    PRIM* pPrevPrim = &pLoop->PrimBuf[pLoop->nPrims - 1];
    pPrim = pLoop->PrimBuf;

    for( nPrims = pLoop->nPrims; nPrims != 0; nPrims--, pPrevPrim = pPrim, pPrim++ )
    {
        enum { LINE_LINE, LINE_CURVE, CURVE_LINE, CURVE_CURVE } transition;

        if( pPrevPrim->primType == PRIM_LINE )
            transition = ( pPrim->primType != PRIM_LINE ) ? LINE_CURVE : LINE_LINE;
        else
            transition = ( pPrim->primType != PRIM_LINE ) ? CURVE_CURVE : CURVE_LINE;

        POINT3D* pPrevNorm = &pPrevPrim->pFNorm[pPrevPrim->nVerts - 2];    // last face of the previous primitive
        POINT3D* pNorm = pPrim->pFNorm;                                     // first face of this one

        double angle = fabs( CalcAngle( (POINT2D*)pPrevNorm, (POINT2D*)pNorm ) );

        switch( transition )
        {
            case LINE_CURVE:
                // the curve continues the line smoothly: start it with the line's normal
                if( angle < SHAPE3D_PI / 4.0 )
                {
                    pPrim->pVNorm[0] = *pPrevNorm;
                    Normalize2d( (POINT2D*)&pPrim->pVNorm[0] );
                }
                break;

            case CURVE_LINE:
                if( angle < SHAPE3D_PI / 4.0 )
                {
                    POINT3D* pLast = &pPrevPrim->pVNorm[pPrevPrim->nVerts - 1];
                    *pLast = *pNorm;
                    Normalize2d( (POINT2D*)pLast );
                }
                break;

            case CURVE_CURVE:
                // both curves share the averaged normal
                if( angle < SHAPE3D_PI / 2.0 )
                {
                    pPrim->pVNorm[0].x = pNorm->x + pPrevNorm->x;
                    pPrim->pVNorm[0].y = pPrevNorm->y + pNorm->y;
                    pPrim->pVNorm[0].z = pPrevNorm->z + pNorm->z;
                    Normalize2d( (POINT2D*)&pPrim->pVNorm[0] );

                    pPrevPrim->pVNorm[pPrevPrim->nVerts - 1] = pPrim->pVNorm[0];
                }
                break;

            default:
                break;
        }
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: AppendToFaceBuf()
//-----------------------------------------------------------------------------
static BOOL AppendToFaceBuf( EXTRContext* ec, FLOAT value )
{
    if( ec->FaceBufIndex >= ec->FaceBufSize )
    {
        DWORD newSize = ec->FaceBufSize + FACEBUF_INCREMENT;

        if( newSize < ec->FaceBufSize ||
            (ULONGLONG)newSize * sizeof(FLOAT) > 0xffffffff )
        {
            return FALSE;
        }

        FLOAT* pNew = (FLOAT*)realloc( ec->FaceBuf, (size_t)newSize * sizeof(FLOAT) );
        if( pNew == NULL )
            return FALSE;

        ec->FaceBuf = pNew;
        ec->FaceBufSize = newSize;
    }

    ec->FaceBuf[ec->FaceBufIndex] = value;
    ec->FaceBufIndex++;

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: Normalize2d()
// Desc: Normalizes x and y of a vector (the z of a side normal is zero).
//-----------------------------------------------------------------------------
static void Normalize2d( POINT2D* p )
{
    FLOAT len = p->x * p->x + p->y * p->y;

    if( len > ZERO_EPS )
        len = 1.0f / sqrtf( len );
    else
        len = 1.0f;

    p->x *= len;
    p->y *= len;
}

} // namespace Shape3D
