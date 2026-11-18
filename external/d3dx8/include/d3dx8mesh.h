//-----------------------------------------------------------------------------
// File: d3dx8mesh.h
//
// Desc: Meshes of the D3DX 8 utility library, as far as the Direct3D 8
//       screen savers use them: the ID3DXMesh interface and D3DXCreateText
//       (clean-room replacement, see ../README.md).
//-----------------------------------------------------------------------------
#ifndef __D3DX8MESH_H__
#define __D3DX8MESH_H__

#include <d3d8.h>
#include "d3dx8core.h"

// {A69BA991-1F7D-11d3-B929-00C04F68DC23}
DEFINE_GUID( IID_ID3DXBaseMesh,
0xa69ba991, 0x1f7d, 0x11d3, 0xb9, 0x29, 0x0, 0xc0, 0x4f, 0x68, 0xdc, 0x23);

// {9D84AC46-6B90-49a9-A721-085C7A3E3DAE}
DEFINE_GUID( IID_ID3DXMesh,
0x9d84ac46, 0x6b90, 0x49a9, 0xa7, 0x21, 0x8, 0x5c, 0x7a, 0x3e, 0x3d, 0xae);

#ifndef MAX_FVF_DECL_SIZE
#define MAX_FVF_DECL_SIZE 20
#endif

// Options of the meshes
enum _D3DXMESH {
    D3DXMESH_32BIT                  = 0x001,
    D3DXMESH_DONOTCLIP              = 0x002,
    D3DXMESH_POINTS                 = 0x004,
    D3DXMESH_RTPATCHES              = 0x008,
    D3DXMESH_NPATCHES               = 0x4000,
    D3DXMESH_VB_SYSTEMMEM           = 0x010,
    D3DXMESH_VB_MANAGED             = 0x020,
    D3DXMESH_VB_WRITEONLY           = 0x040,
    D3DXMESH_VB_DYNAMIC             = 0x080,
    D3DXMESH_VB_SOFTWAREPROCESSING  = 0x8000,
    D3DXMESH_IB_SYSTEMMEM           = 0x100,
    D3DXMESH_IB_MANAGED             = 0x200,
    D3DXMESH_IB_WRITEONLY           = 0x400,
    D3DXMESH_IB_DYNAMIC             = 0x800,
    D3DXMESH_IB_SOFTWAREPROCESSING  = 0x10000,
    D3DXMESH_VB_SHARE               = 0x1000,
    D3DXMESH_USEHWONLY              = 0x2000,
    D3DXMESH_SYSTEMMEM              = 0x110,
    D3DXMESH_MANAGED                = 0x220,
    D3DXMESH_WRITEONLY              = 0x440,
    D3DXMESH_DYNAMIC                = 0x880,
    D3DXMESH_SOFTWAREPROCESSING     = 0x18000,
};

typedef struct _D3DXATTRIBUTERANGE
{
    DWORD AttribId;
    DWORD FaceStart;
    DWORD FaceCount;
    DWORD VertexStart;
    DWORD VertexCount;
} D3DXATTRIBUTERANGE;

typedef D3DXATTRIBUTERANGE* LPD3DXATTRIBUTERANGE;

typedef interface ID3DXBaseMesh ID3DXBaseMesh;
typedef interface ID3DXBaseMesh *LPD3DXBASEMESH;
typedef interface ID3DXMesh ID3DXMesh;
typedef interface ID3DXMesh *LPD3DXMESH;

#undef INTERFACE
#define INTERFACE ID3DXBaseMesh

DECLARE_INTERFACE_(ID3DXBaseMesh, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DXBaseMesh
    STDMETHOD(DrawSubset)(THIS_ DWORD AttribId) PURE;
    STDMETHOD_(DWORD, GetNumFaces)(THIS) PURE;
    STDMETHOD_(DWORD, GetNumVertices)(THIS) PURE;
    STDMETHOD_(DWORD, GetFVF)(THIS) PURE;
    STDMETHOD(GetDeclaration)(THIS_ DWORD Declaration[MAX_FVF_DECL_SIZE]) PURE;
    STDMETHOD_(DWORD, GetOptions)(THIS) PURE;
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE8* ppDevice) PURE;
    STDMETHOD(CloneMeshFVF)(THIS_ DWORD Options, DWORD FVF, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh) PURE;
    STDMETHOD(CloneMesh)(THIS_ DWORD Options, CONST DWORD *pDeclaration, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh) PURE;
    STDMETHOD(GetVertexBuffer)(THIS_ LPDIRECT3DVERTEXBUFFER8* ppVB) PURE;
    STDMETHOD(GetIndexBuffer)(THIS_ LPDIRECT3DINDEXBUFFER8* ppIB) PURE;
    STDMETHOD(LockVertexBuffer)(THIS_ DWORD Flags, BYTE** ppData) PURE;
    STDMETHOD(UnlockVertexBuffer)(THIS) PURE;
    STDMETHOD(LockIndexBuffer)(THIS_ DWORD Flags, BYTE** ppData) PURE;
    STDMETHOD(UnlockIndexBuffer)(THIS) PURE;
    STDMETHOD(GetAttributeTable)(THIS_ D3DXATTRIBUTERANGE *pAttribTable, DWORD* pAttribTableSize) PURE;
    STDMETHOD(ConvertPointRepsToAdjacency)(THIS_ CONST DWORD* pPRep, DWORD* pAdjacency) PURE;
    STDMETHOD(ConvertAdjacencyToPointReps)(THIS_ CONST DWORD* pAdjacency, DWORD* pPRep) PURE;
    STDMETHOD(GenerateAdjacency)(THIS_ FLOAT fEpsilon, DWORD* pAdjacency) PURE;
};

#undef INTERFACE
#define INTERFACE ID3DXMesh

DECLARE_INTERFACE_(ID3DXMesh, ID3DXBaseMesh)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DXBaseMesh
    STDMETHOD(DrawSubset)(THIS_ DWORD AttribId) PURE;
    STDMETHOD_(DWORD, GetNumFaces)(THIS) PURE;
    STDMETHOD_(DWORD, GetNumVertices)(THIS) PURE;
    STDMETHOD_(DWORD, GetFVF)(THIS) PURE;
    STDMETHOD(GetDeclaration)(THIS_ DWORD Declaration[MAX_FVF_DECL_SIZE]) PURE;
    STDMETHOD_(DWORD, GetOptions)(THIS) PURE;
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE8* ppDevice) PURE;
    STDMETHOD(CloneMeshFVF)(THIS_ DWORD Options, DWORD FVF, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh) PURE;
    STDMETHOD(CloneMesh)(THIS_ DWORD Options, CONST DWORD *pDeclaration, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh) PURE;
    STDMETHOD(GetVertexBuffer)(THIS_ LPDIRECT3DVERTEXBUFFER8* ppVB) PURE;
    STDMETHOD(GetIndexBuffer)(THIS_ LPDIRECT3DINDEXBUFFER8* ppIB) PURE;
    STDMETHOD(LockVertexBuffer)(THIS_ DWORD Flags, BYTE** ppData) PURE;
    STDMETHOD(UnlockVertexBuffer)(THIS) PURE;
    STDMETHOD(LockIndexBuffer)(THIS_ DWORD Flags, BYTE** ppData) PURE;
    STDMETHOD(UnlockIndexBuffer)(THIS) PURE;
    STDMETHOD(GetAttributeTable)(THIS_ D3DXATTRIBUTERANGE *pAttribTable, DWORD* pAttribTableSize) PURE;
    STDMETHOD(ConvertPointRepsToAdjacency)(THIS_ CONST DWORD* pPRep, DWORD* pAdjacency) PURE;
    STDMETHOD(ConvertAdjacencyToPointReps)(THIS_ CONST DWORD* pAdjacency, DWORD* pPRep) PURE;
    STDMETHOD(GenerateAdjacency)(THIS_ FLOAT fEpsilon, DWORD* pAdjacency) PURE;

    // ID3DXMesh
    STDMETHOD(LockAttributeBuffer)(THIS_ DWORD Flags, DWORD** ppData) PURE;
    STDMETHOD(UnlockAttributeBuffer)(THIS) PURE;
    STDMETHOD(Optimize)(THIS_ DWORD Flags, CONST DWORD* pAdjacencyIn, DWORD* pAdjacencyOut,
                     DWORD* pFaceRemap, LPD3DXBUFFER *ppVertexRemap, LPD3DXMESH* ppOptMesh) PURE;
    STDMETHOD(OptimizeInplace)(THIS_ DWORD Flags, CONST DWORD* pAdjacencyIn, DWORD* pAdjacencyOut,
                     DWORD* pFaceRemap, LPD3DXBUFFER *ppVertexRemap) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DXCreateMeshFVF( DWORD NumFaces, DWORD NumVertices, DWORD Options, DWORD FVF,
                                  LPDIRECT3DDEVICE8 pD3D, LPD3DXMESH* ppMesh );

// A mesh of the outlines of the TrueType glyphs of pText (the font selected
// into hDC), extruded by Extrusion along -z (toward the viewer: the front
// face lies in the plane z = 0, the back face at z = Extrusion).  Deviation
// is the maximum chordal deviation of the curves (0: one design unit).
// Vertices are D3DFVF_XYZ | D3DFVF_NORMAL.
HRESULT WINAPI D3DXCreateTextA( LPDIRECT3DDEVICE8 pDevice, HDC hDC, LPCSTR pText, FLOAT Deviation,
                                FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                                LPGLYPHMETRICSFLOAT pGlyphMetrics );
HRESULT WINAPI D3DXCreateTextW( LPDIRECT3DDEVICE8 pDevice, HDC hDC, LPCWSTR pText, FLOAT Deviation,
                                FLOAT Extrusion, LPD3DXMESH* ppMesh, LPD3DXBUFFER* ppAdjacency,
                                LPGLYPHMETRICSFLOAT pGlyphMetrics );

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define D3DXCreateText D3DXCreateTextW
#else
#define D3DXCreateText D3DXCreateTextA
#endif

#endif // __D3DX8MESH_H__
