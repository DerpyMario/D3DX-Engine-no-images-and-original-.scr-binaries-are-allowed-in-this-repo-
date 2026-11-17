//-----------------------------------------------------------------------------
// File: fontoutl.h
//
// Desc: Internal declarations of Shape3D.lib, shared by fontoutl.cpp (glyph
//       outline -> contour loops -> mesh) and fontextr.cpp (extrusion).
//
//       The structure layouts were recovered from ssText3d.scr (amd64); the
//       names of the structures and of their members are reconstructions,
//       apart from EXTRContext, POINT2D and CFaceContext, whose names appear
//       in the symbol file.
//-----------------------------------------------------------------------------
#ifndef _SHAPE3D_FONTOUTL_H
#define _SHAPE3D_FONTOUTL_H

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <GL/gl.h>
#include <GL/glu.h>

namespace Shape3D
{

//-----------------------------------------------------------------------------
// Contours
//-----------------------------------------------------------------------------
struct POINT2D
{
    FLOAT x;
    FLOAT y;
};

struct POINT3D
{
    FLOAT x;
    FLOAT y;
    FLOAT z;
};

// Primitive types of a contour
#define PRIM_LINE       3           // a polyline; drawn as flat shaded quads
#define PRIM_CURVE      4           // a flattened curve; drawn as a smooth shaded quad strip

// A run of vertices of a contour.  Neighbouring primitives share a vertex:
// the first vertex of a primitive is the last one of its predecessor.
struct PRIM
{
    DWORD       primType;           // PRIM_LINE or PRIM_CURVE
    DWORD       nVerts;
    DWORD       vertIndex;          // index of the first vertex in LOOP::VertBuf
    POINT2D*    pVert;              // -> first vertex
    POINT3D*    pFNorm;             // -> face normals (one per segment)
    POINT3D*    pVNorm;             // -> vertex normals (curves only)
};

// One closed contour of a glyph.  The last vertex repeats the first.
struct LOOP
{
    PRIM*       PrimBuf;
    DWORD       nPrims;
    DWORD       PrimBufSize;
    POINT2D*    VertBuf;
    DWORD       nVerts;
    DWORD       VertBufSize;
    POINT3D*    FNormBuf;
    POINT3D*    VNormBuf;
};

// All contours of a glyph
struct LOOP_LIST
{
    LOOP*       LoopBuf;
    DWORD       nLoops;
    DWORD       LoopBufSize;
};

//-----------------------------------------------------------------------------
// Name: class CFaceContext
// Desc: Collects the triangles of one glyph.
//-----------------------------------------------------------------------------
class CFaceContext
{
public:
    struct VERTEX                   // D3DFVF_XYZ | D3DFVF_NORMAL
    {
        D3DXVECTOR3 position;
        D3DXVECTOR3 normal;
    };

    struct FACE
    {
        WORD        v[3];
    };

    VERTEX*     m_pVertices;
    UINT        m_cVertices;
    UINT        m_cVerticesMax;
    FACE*       m_pFaces;
    UINT        m_cFaces;
    UINT        m_cFacesMax;
    D3DXVECTOR3 m_vNormal;          // normal and depth given to the vertices
    FLOAT       m_fZ;               //   that SetupPrim() adds

public:
    CFaceContext();
    ~CFaceContext();

    HRESULT     Init();

    HRESULT     AddQuad( D3DXVECTOR3* pVertices, D3DXVECTOR3* pNormals, BOOL bReverse );
    HRESULT     AddQuadStrip( D3DXVECTOR3* pVertices, D3DXVECTOR3* pNormals, UINT cVertices, BOOL bReverse );

    HRESULT     AddTriangles( D3DXVECTOR2* pVertices, UINT cVertices, BOOL bReverse );
    HRESULT     AddTriangleStrip( D3DXVECTOR2* pVertices, UINT cVertices, BOOL bReverse );
    HRESULT     AddTriangleFan( D3DXVECTOR2* pVertices, UINT cVertices, BOOL bReverse );

private:
    HRESULT     ResizeFaces( UINT cFaces );
    HRESULT     ResizeVertices( UINT cVertices );
    HRESULT     SetupPrim( D3DXVECTOR2* pVertices, UINT cVertices, UINT cFaces, UINT* piFirstVertex );
};

//-----------------------------------------------------------------------------
// Name: struct EXTRContext
// Desc: State of the extrusion code
//-----------------------------------------------------------------------------
struct EXTRContext
{
    FLOAT           zExtrusion;             // z of the back face (negative)
    BOOL            bFacePolys;             // front and back faces are wanted

    // The tessellated front face, as delivered by the GLU tessellator:
    //   [number of primitives] { [GL primitive type] [number of vertices] { [x] [y] } }
    FLOAT*          FaceBuf;
    DWORD           FaceBufSize;            // in floats
    DWORD           FaceBufIndex;           // next free entry
    DWORD           FaceVertexCountIndex;   // vertex count entry of the primitive being received

    HRESULT         hr;                     // first error that happened inside a callback
    CFaceContext*   pFaceContext;           // receives the triangles
};

//-----------------------------------------------------------------------------
// Name: struct OFContext
// Desc: State of the outline font code
//-----------------------------------------------------------------------------
#define COMBINE_BUF_VERTS   50

// Storage for the vertices that the tessellator asks for when contours
// intersect.  The first block lives in the context, further ones are chained.
struct CombineBuf
{
    INT             nVerts;
    POINT2D         verts[COMBINE_BUF_VERTS];
    CombineBuf*     pNext;
};

#define OF_FORMAT_POLYGONS  1               // (WGL_FONT_POLYGONS)

struct OFContext
{
    HRESULT         hr;                     // error reported by the tessellator
    FLOAT           chordalDeviation;
    FLOAT           scale;                  // font design units -> em square
    INT             format;
    UCHAR*          glyphBuf;               // GGO_NATIVE data of the current glyph
    DWORD           glyphSize;
    HFONT           hfontOld;               // the caller's font
    GLUtesselator*  tess;
    CombineBuf      combineBuf;
    CombineBuf*     pCurCombineBuf;
    EXTRContext*    ec;
};

//-----------------------------------------------------------------------------
// fontextr.cpp
//-----------------------------------------------------------------------------
EXTRContext*    extr_Init( FLOAT extrusion );
void            extr_Finish( EXTRContext* ec );
int             extr_PolyInit( EXTRContext* ec );
void            extr_PolyFinish( EXTRContext* ec );
HRESULT         extr_DrawPolygons( EXTRContext* ec, LOOP_LIST* pLoopList );

void CALLBACK   extr_glBegin( GLenum primType, void* data );
void CALLBACK   extr_glVertex( GLfloat* v, void* data );
void CALLBACK   extr_glEnd();

double          CalcAngle( POINT2D* v1, POINT2D* v2 );

} // namespace Shape3D

#endif // _SHAPE3D_FONTOUTL_H
