//-----------------------------------------------------------------------------
// File: d3dx8text.cpp
//
// Desc: D3DXCreateText for the D3DX 8 subset used by the Direct3D 8 screen
//       savers (clean-room implementation, see ../README.md).
//
//       Like wglUseFontOutlines, which it is modelled on, the text is built in
//       units of the font's em square: the TrueType outlines of the glyphs
//       (GetGlyphOutline) are flattened to within Deviation, the front face
//       is tessellated with the GLU tessellator of glu32.dll, the back face is
//       a copy of it at z = Extrusion, and the sides join the two.  Vertices
//       are D3DFVF_XYZ | D3DFVF_NORMAL; the front of the text faces -z.
//-----------------------------------------------------------------------------
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <new>
#include <vector>
#include "d3dx8.h"

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

namespace
{

struct POINT2 { double x, y; };

struct TEXTVERTEX
{
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
};

typedef std::vector<POINT2> CONTOUR;

//-----------------------------------------------------------------------------
// Outlines
//-----------------------------------------------------------------------------
static double FixedToDouble( FIXED f )
{
    return (double)f.value + (double)f.fract / 65536.0;
}

// Flattens the quadratic B-spline segment p0 - p1 - p2
static void AddQuadratic( CONTOUR& c, POINT2 p0, POINT2 p1, POINT2 p2, double dev )
{
    // The distance of the control point from the chord bounds the deviation
    // of the curve (by half of it); subdivide accordingly.
    double dx = (p0.x + p2.x) * 0.5 - p1.x;
    double dy = (p0.y + p2.y) * 0.5 - p1.y;
    double d = sqrt( dx * dx + dy * dy ) * 0.5;
    int n = 1;
    while( d / ((double)n * n) > dev && n < 64 )
        n++;

    for( int i = 1; i <= n; i++ )
    {
        double t = (double)i / n;
        double u = 1.0 - t;
        POINT2 p;
        p.x = u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x;
        p.y = u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y;
        c.push_back( p );
    }
}

// Flattens the cubic Bezier segment p0 - p1 - p2 - p3
static void AddCubic( CONTOUR& c, POINT2 p0, POINT2 p1, POINT2 p2, POINT2 p3, double dev )
{
    double dx1 = p0.x - 2 * p1.x + p2.x, dy1 = p0.y - 2 * p1.y + p2.y;
    double dx2 = p1.x - 2 * p2.x + p3.x, dy2 = p1.y - 2 * p2.y + p3.y;
    double d1 = dx1 * dx1 + dy1 * dy1, d2 = dx2 * dx2 + dy2 * dy2;
    double d = 0.75 * sqrt( d1 > d2 ? d1 : d2 );
    int n = 1;
    while( d / ((double)n * n) > dev && n < 64 )
        n++;

    for( int i = 1; i <= n; i++ )
    {
        double t = (double)i / n;
        double u = 1.0 - t;
        POINT2 p;
        p.x = u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x;
        p.y = u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y;
        c.push_back( p );
    }
}

// The contours of one glyph, in em units, moved by (xOrigin, 0)
static BOOL GetGlyphContours( HDC hdc, UINT ch, double scale, double dev, double xOrigin,
                              std::vector<CONTOUR>& contours, GLYPHMETRICS* pgm )
{
    static const MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
    DWORD cb = GetGlyphOutlineW( hdc, ch, GGO_NATIVE, pgm, 0, NULL, &mat );
    if( cb == GDI_ERROR )
        return FALSE;
    if( cb == 0 )
        return TRUE;     // (white space)

    std::vector<BYTE> buf( cb );
    if( GetGlyphOutlineW( hdc, ch, GGO_NATIVE, pgm, cb, &buf[0], &mat ) == GDI_ERROR )
        return FALSE;

    BYTE* p = &buf[0];
    BYTE* pEnd = p + cb;
    while( p < pEnd )
    {
        TTPOLYGONHEADER* ph = (TTPOLYGONHEADER*)p;
        BYTE* pc = p + sizeof(TTPOLYGONHEADER);
        BYTE* pcEnd = p + ph->cb;
        CONTOUR c;
        POINT2 start, cur;

        start.x = FixedToDouble( ph->pfxStart.x ) * scale + xOrigin;
        start.y = FixedToDouble( ph->pfxStart.y ) * scale;
        c.push_back( start );
        cur = start;

        while( pc < pcEnd )
        {
            TTPOLYCURVE* pcurve = (TTPOLYCURVE*)pc;
            std::vector<POINT2> pts( pcurve->cpfx );
            for( WORD i = 0; i < pcurve->cpfx; i++ )
            {
                pts[i].x = FixedToDouble( pcurve->apfx[i].x ) * scale + xOrigin;
                pts[i].y = FixedToDouble( pcurve->apfx[i].y ) * scale;
            }

            if( pcurve->wType == TT_PRIM_LINE )
            {
                for( WORD i = 0; i < pcurve->cpfx; i++ )
                    c.push_back( pts[i] );
                cur = pts[pcurve->cpfx - 1];
            }
            else if( pcurve->wType == TT_PRIM_QSPLINE )
            {
                // Implied on-curve points halfway between off-curve points
                for( WORD i = 0; i + 1 < pcurve->cpfx; i++ )
                {
                    POINT2 end;
                    if( i + 2 < pcurve->cpfx )
                    {
                        end.x = (pts[i].x + pts[i + 1].x) * 0.5;
                        end.y = (pts[i].y + pts[i + 1].y) * 0.5;
                    }
                    else
                    {
                        end = pts[i + 1];
                    }
                    AddQuadratic( c, cur, pts[i], end, dev );
                    cur = end;
                }
            }
            else if( pcurve->wType == TT_PRIM_CSPLINE )
            {
                for( WORD i = 0; i + 2 < pcurve->cpfx; i += 3 )
                {
                    AddCubic( c, cur, pts[i], pts[i + 1], pts[i + 2], dev );
                    cur = pts[i + 2];
                }
            }

            pc += sizeof(TTPOLYCURVE) + (pcurve->cpfx - 1) * sizeof(POINTFX);
        }

        // Closed: drop the duplicate of the start point and repeated points
        CONTOUR clean;
        for( size_t i = 0; i < c.size(); i++ )
        {
            if( !clean.empty() && fabs( clean.back().x - c[i].x ) < 1e-9 && fabs( clean.back().y - c[i].y ) < 1e-9 )
                continue;
            clean.push_back( c[i] );
        }
        while( clean.size() > 1 && fabs( clean.back().x - clean[0].x ) < 1e-9 && fabs( clean.back().y - clean[0].y ) < 1e-9 )
            clean.pop_back();
        if( clean.size() >= 3 )
            contours.push_back( clean );

        p += ph->cb;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Tessellation of the front face
//-----------------------------------------------------------------------------
struct TESSDATA
{
    std::vector<POINT2>*   pVerts;     // all points (contour points, then new ones)
    std::vector<DWORD>*    pTris;      // triangle list (indices into pVerts)
    std::vector<double*>   newVerts;   // storage for the points made by GLU_TESS_COMBINE
};

static void CALLBACK TessBegin( GLenum type, void* pData ) {}
static void CALLBACK TessEdgeFlag( GLboolean flag, void* pData ) {}   // forces GL_TRIANGLES
static void CALLBACK TessEnd( void* pData ) {}

static void CALLBACK TessVertex( void* pVertex, void* pData )
{
    TESSDATA* p = (TESSDATA*)pData;
    p->pTris->push_back( (DWORD)(DWORD_PTR)pVertex );
}

static void CALLBACK TessCombine( GLdouble coords[3], void* vertexData[4], GLfloat weight[4],
                                  void** outData, void* pData )
{
    TESSDATA* p = (TESSDATA*)pData;
    POINT2 pt;
    pt.x = coords[0];
    pt.y = coords[1];
    p->pVerts->push_back( pt );
    *outData = (void*)(DWORD_PTR)(p->pVerts->size() - 1);
}

static BOOL TessellateFace( const std::vector<CONTOUR>& contours, std::vector<POINT2>& verts,
                            std::vector<DWORD>& tris )
{
    GLUtesselator* pTess = gluNewTess();
    if( pTess == NULL )
        return FALSE;

    TESSDATA data;
    data.pVerts = &verts;
    data.pTris = &tris;

    gluTessCallback( pTess, GLU_TESS_BEGIN_DATA, (void (CALLBACK*)())TessBegin );
    gluTessCallback( pTess, GLU_TESS_EDGE_FLAG_DATA, (void (CALLBACK*)())TessEdgeFlag );
    gluTessCallback( pTess, GLU_TESS_VERTEX_DATA, (void (CALLBACK*)())TessVertex );
    gluTessCallback( pTess, GLU_TESS_END_DATA, (void (CALLBACK*)())TessEnd );
    gluTessCallback( pTess, GLU_TESS_COMBINE_DATA, (void (CALLBACK*)())TessCombine );
    gluTessProperty( pTess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO );
    gluTessNormal( pTess, 0.0, 0.0, 1.0 );

    // GLU keeps pointers to the coordinates until the polygon ends
    std::vector<GLdouble> coords( verts.size() * 3 );
    for( size_t i = 0; i < verts.size(); i++ )
    {
        coords[i * 3 + 0] = verts[i].x;
        coords[i * 3 + 1] = verts[i].y;
        coords[i * 3 + 2] = 0.0;
    }

    gluTessBeginPolygon( pTess, &data );
    size_t base = 0;
    for( size_t c = 0; c < contours.size(); c++ )
    {
        gluTessBeginContour( pTess );
        for( size_t i = 0; i < contours[c].size(); i++ )
            gluTessVertex( pTess, &coords[(base + i) * 3], (void*)(DWORD_PTR)(base + i) );
        gluTessEndContour( pTess );
        base += contours[c].size();
    }
    gluTessEndPolygon( pTess );

    gluDeleteTess( pTess );
    return TRUE;
}


//-----------------------------------------------------------------------------
// Mesh building
//-----------------------------------------------------------------------------
static void AddTriangle( std::vector<TEXTVERTEX>& v, std::vector<DWORD>& idx,
                         DWORD a, DWORD b, DWORD c, const D3DXVECTOR3& nFront )
{
    // Clockwise as seen from the side the face points to (cull mode CCW)
    D3DXVECTOR3 e1 = v[b].p - v[a].p;
    D3DXVECTOR3 e2 = v[c].p - v[a].p;
    D3DXVECTOR3 n;
    D3DXVec3Cross( &n, &e1, &e2 );
    if( D3DXVec3Dot( &n, &nFront ) < 0.0f )
    {
        DWORD t = b;
        b = c;
        c = t;
    }
    idx.push_back( a );
    idx.push_back( b );
    idx.push_back( c );
}

static TEXTVERTEX MakeVertex( double x, double y, FLOAT z, const D3DXVECTOR3& n )
{
    TEXTVERTEX v;
    v.p = D3DXVECTOR3( (FLOAT)x, (FLOAT)y, z );
    v.n = n;
    return v;
}

static HRESULT CreateText( LPDIRECT3DDEVICE8 pDevice, HDC hDC, LPCWSTR pText, FLOAT Deviation,
                           FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                           LPGLYPHMETRICSFLOAT pGlyphMetrics )
{
    HRESULT hr;
    OUTLINETEXTMETRICW otm;
    LOGFONTW lf;
    HFONT hFont, hFontEm, hFontOld;

    if( pDevice == NULL || hDC == NULL || pText == NULL || ppMesh == NULL )
        return D3DERR_INVALIDCALL;
    *ppMesh = NULL;
    if( ppAdjacency )
        *ppAdjacency = NULL;

    // Work with the font at the size of its em square: one design unit per
    // logical unit
    hFont = (HFONT)GetCurrentObject( hDC, OBJ_FONT );
    if( hFont == NULL || GetObjectW( hFont, sizeof(lf), &lf ) == 0 )
        return E_FAIL;
    otm.otmSize = sizeof(otm);
    if( GetOutlineTextMetricsW( hDC, sizeof(otm), &otm ) == 0 )
        return E_FAIL;      // not a TrueType font
    lf.lfHeight = -(LONG)otm.otmEMSquare;
    lf.lfWidth = 0;
    hFontEm = CreateFontIndirectW( &lf );
    if( hFontEm == NULL )
        return E_FAIL;
    hFontOld = (HFONT)SelectObject( hDC, hFontEm );

    double scale = 1.0 / (double)otm.otmEMSquare;
    double dev = (Deviation > 0.0f) ? Deviation : scale;    // 0: one design unit

    std::vector<TEXTVERTEX> verts;
    std::vector<DWORD> indices;
    double xOrigin = 0.0;
    hr = S_OK;

    for( int iChar = 0; pText[iChar] != L'\0'; iChar++ )
    {
        std::vector<CONTOUR> contours;
        GLYPHMETRICS gm;

        if( !GetGlyphContours( hDC, pText[iChar], scale, dev, xOrigin, contours, &gm ) )
        {
            hr = E_FAIL;
            break;
        }

        if( pGlyphMetrics )
        {
            pGlyphMetrics[iChar].gmfBlackBoxX = (FLOAT)(gm.gmBlackBoxX * scale);
            pGlyphMetrics[iChar].gmfBlackBoxY = (FLOAT)(gm.gmBlackBoxY * scale);
            pGlyphMetrics[iChar].gmfptGlyphOrigin.x = (FLOAT)(gm.gmptGlyphOrigin.x * scale);
            pGlyphMetrics[iChar].gmfptGlyphOrigin.y = (FLOAT)(gm.gmptGlyphOrigin.y * scale);
            pGlyphMetrics[iChar].gmfCellIncX = (FLOAT)(gm.gmCellIncX * scale);
            pGlyphMetrics[iChar].gmfCellIncY = (FLOAT)(gm.gmCellIncY * scale);
        }

        xOrigin += gm.gmCellIncX * scale;

        if( contours.empty() )
            continue;

        // Front and back faces
        std::vector<POINT2> pts;
        std::vector<DWORD> tris;
        for( size_t c = 0; c < contours.size(); c++ )
            pts.insert( pts.end(), contours[c].begin(), contours[c].end() );
        if( !TessellateFace( contours, pts, tris ) )
        {
            hr = E_FAIL;
            break;
        }

        const D3DXVECTOR3 nFront( 0.0f, 0.0f, -1.0f );
        const D3DXVECTOR3 nBack( 0.0f, 0.0f, 1.0f );
        DWORD baseFront = (DWORD)verts.size();
        for( size_t i = 0; i < pts.size(); i++ )
            verts.push_back( MakeVertex( pts[i].x, pts[i].y, 0.0f, nFront ) );
        DWORD baseBack = (DWORD)verts.size();
        for( size_t i = 0; i < pts.size(); i++ )
            verts.push_back( MakeVertex( pts[i].x, pts[i].y, Extrusion, nBack ) );
        for( size_t t = 0; t + 2 < tris.size(); t += 3 )
        {
            AddTriangle( verts, indices, baseFront + tris[t], baseFront + tris[t + 1], baseFront + tris[t + 2], nFront );
            AddTriangle( verts, indices, baseBack + tris[t], baseBack + tris[t + 1], baseBack + tris[t + 2], nBack );
        }

        // Sides: smooth normals where the outline bends by less than about
        // 30 degrees, sharp edges elsewhere
        for( size_t c = 0; c < contours.size(); c++ )
        {
            const CONTOUR& con = contours[c];
            size_t n = con.size();
            std::vector<D3DXVECTOR3> edgeNormals( n );

            for( size_t i = 0; i < n; i++ )
            {
                const POINT2& a = con[i];
                const POINT2& b = con[(i + 1) % n];
                // Outward: TrueType outer contours are clockwise, holes
                // counter-clockwise; the material is to the right
                D3DXVECTOR3 nrm( (FLOAT)-(b.y - a.y), (FLOAT)(b.x - a.x), 0.0f );
                D3DXVec3Normalize( &edgeNormals[i], &nrm );
            }

            for( size_t i = 0; i < n; i++ )
            {
                size_t iPrev = (i + n - 1) % n;
                size_t iNext = (i + 1) % n;
                const POINT2& a = con[i];
                const POINT2& b = con[iNext];
                const D3DXVECTOR3& ne = edgeNormals[i];

                // Normals at both ends of the edge
                D3DXVECTOR3 na = ne, nb = ne;
                if( D3DXVec3Dot( &edgeNormals[iPrev], &ne ) > 0.866f )
                {
                    D3DXVECTOR3 s = edgeNormals[iPrev] + ne;
                    D3DXVec3Normalize( &na, &s );
                }
                if( D3DXVec3Dot( &edgeNormals[iNext], &ne ) > 0.866f )
                {
                    D3DXVECTOR3 s = edgeNormals[iNext] + ne;
                    D3DXVec3Normalize( &nb, &s );
                }

                DWORD v0 = (DWORD)verts.size();
                verts.push_back( MakeVertex( a.x, a.y, 0.0f, na ) );
                verts.push_back( MakeVertex( b.x, b.y, 0.0f, nb ) );
                verts.push_back( MakeVertex( a.x, a.y, Extrusion, na ) );
                verts.push_back( MakeVertex( b.x, b.y, Extrusion, nb ) );
                AddTriangle( verts, indices, v0, v0 + 1, v0 + 2, ne );
                AddTriangle( verts, indices, v0 + 1, v0 + 3, v0 + 2, ne );
            }
        }
    }

    SelectObject( hDC, hFontOld );
    DeleteObject( hFontEm );

    if( FAILED( hr ) )
        return hr;
    if( indices.empty() )
        return E_FAIL;      // (nothing visible: D3DX fails too)

    // The mesh
    DWORD dwNumFaces = (DWORD)(indices.size() / 3);
    DWORD dwNumVerts = (DWORD)verts.size();
    DWORD dwOptions = D3DXMESH_MANAGED;
    if( dwNumVerts > 0xffff )
        dwOptions |= D3DXMESH_32BIT;

    LPD3DXMESH pMesh;
    if( FAILED( hr = D3DXCreateMeshFVF( dwNumFaces, dwNumVerts, dwOptions,
                                        D3DFVF_XYZ | D3DFVF_NORMAL, pDevice, &pMesh ) ) )
        return hr;

    BYTE* pData;
    if( SUCCEEDED( hr = pMesh->LockVertexBuffer( 0, &pData ) ) )
    {
        memcpy( pData, &verts[0], dwNumVerts * sizeof(TEXTVERTEX) );
        pMesh->UnlockVertexBuffer();
    }
    if( SUCCEEDED( hr ) && SUCCEEDED( hr = pMesh->LockIndexBuffer( 0, &pData ) ) )
    {
        for( size_t i = 0; i < indices.size(); i++ )
        {
            if( dwOptions & D3DXMESH_32BIT )
                ((DWORD*)pData)[i] = indices[i];
            else
                ((WORD*)pData)[i] = (WORD)indices[i];
        }
        pMesh->UnlockIndexBuffer();
    }

    if( SUCCEEDED( hr ) && ppAdjacency )
    {
        if( SUCCEEDED( hr = D3DXCreateBuffer( dwNumFaces * 3 * sizeof(DWORD), ppAdjacency ) ) )
            hr = pMesh->GenerateAdjacency( 0.0f, (DWORD*)(*ppAdjacency)->GetBufferPointer() );
    }

    if( FAILED( hr ) )
    {
        if( ppAdjacency && *ppAdjacency )
        {
            (*ppAdjacency)->Release();
            *ppAdjacency = NULL;
        }
        pMesh->Release();
        return hr;
    }

    *ppMesh = pMesh;
    return S_OK;
}

} // namespace


HRESULT WINAPI D3DXCreateTextW( LPDIRECT3DDEVICE8 pDevice, HDC hDC, LPCWSTR pText, FLOAT Deviation,
                                FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                                LPGLYPHMETRICSFLOAT pGlyphMetrics )
{
    return CreateText( pDevice, hDC, pText, Deviation, Extrusion, ppMesh, ppAdjacency, pGlyphMetrics );
}

HRESULT WINAPI D3DXCreateTextA( LPDIRECT3DDEVICE8 pDevice, HDC hDC, LPCSTR pText, FLOAT Deviation,
                                FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                                LPGLYPHMETRICSFLOAT pGlyphMetrics )
{
    if( pText == NULL )
        return D3DERR_INVALIDCALL;

    int cch = MultiByteToWideChar( CP_ACP, 0, pText, -1, NULL, 0 );
    if( cch <= 0 )
        return E_FAIL;
    std::vector<WCHAR> wsz( cch );
    MultiByteToWideChar( CP_ACP, 0, pText, -1, &wsz[0], cch );

    return CreateText( pDevice, hDC, &wsz[0], Deviation, Extrusion, ppMesh, ppAdjacency, pGlyphMetrics );
}
