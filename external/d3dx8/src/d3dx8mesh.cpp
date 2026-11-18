//-----------------------------------------------------------------------------
// File: d3dx8mesh.cpp
//
// Desc: ID3DXBuffer and a simple ID3DXMesh for the D3DX 8 subset used by the
//       Direct3D 8 screen savers (clean-room implementation, see
//       ../README.md).  Only what the savers need is implemented; the other
//       methods return E_NOTIMPL.
//-----------------------------------------------------------------------------
#include <windows.h>
#include <new>
#include <string.h>
#include "d3dx8.h"


//===========================================================================
// ID3DXBuffer
//===========================================================================
class CD3DXBuffer : public ID3DXBuffer
{
    LONG  m_cRef;
    DWORD m_cbSize;
    BYTE* m_pData;

public:
    CD3DXBuffer() : m_cRef(1), m_cbSize(0), m_pData(NULL) {}
    virtual ~CD3DXBuffer() { delete [] m_pData; }

    HRESULT Init( DWORD cbSize )
    {
        m_pData = new (std::nothrow) BYTE[cbSize ? cbSize : 1];
        if( m_pData == NULL )
            return E_OUTOFMEMORY;
        m_cbSize = cbSize;
        return S_OK;
    }

    STDMETHOD(QueryInterface)( REFIID riid, LPVOID* ppv )
    {
        if( IsEqualIID( riid, IID_IUnknown ) || IsEqualIID( riid, IID_ID3DXBuffer ) )
        {
            AddRef();
            *ppv = this;
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement( &m_cRef ); }
    STDMETHOD_(ULONG, Release)()
    {
        ULONG cRef = InterlockedDecrement( &m_cRef );
        if( cRef == 0 )
            delete this;
        return cRef;
    }
    STDMETHOD_(LPVOID, GetBufferPointer)() { return m_pData; }
    STDMETHOD_(DWORD, GetBufferSize)() { return m_cbSize; }
};

HRESULT WINAPI D3DXCreateBuffer( DWORD NumBytes, LPD3DXBUFFER *ppBuffer )
{
    if( ppBuffer == NULL )
        return D3DERR_INVALIDCALL;

    CD3DXBuffer* pBuffer = new (std::nothrow) CD3DXBuffer;
    if( pBuffer == NULL )
        return E_OUTOFMEMORY;

    HRESULT hr = pBuffer->Init( NumBytes );
    if( FAILED( hr ) )
    {
        pBuffer->Release();
        return hr;
    }

    *ppBuffer = pBuffer;
    return S_OK;
}


//===========================================================================
// Vertex sizes
//===========================================================================
static UINT FVFVertexSize( DWORD dwFVF )
{
    UINT size = 0;

    switch( dwFVF & D3DFVF_POSITION_MASK )
    {
        case D3DFVF_XYZ:    size = 3 * sizeof(FLOAT); break;
        case D3DFVF_XYZRHW: size = 4 * sizeof(FLOAT); break;
        case D3DFVF_XYZB1:  size = 4 * sizeof(FLOAT); break;
        case D3DFVF_XYZB2:  size = 5 * sizeof(FLOAT); break;
        case D3DFVF_XYZB3:  size = 6 * sizeof(FLOAT); break;
        case D3DFVF_XYZB4:  size = 7 * sizeof(FLOAT); break;
        case D3DFVF_XYZB5:  size = 8 * sizeof(FLOAT); break;
    }
    if( dwFVF & D3DFVF_NORMAL )   size += 3 * sizeof(FLOAT);
    if( dwFVF & D3DFVF_PSIZE )    size += sizeof(FLOAT);
    if( dwFVF & D3DFVF_DIFFUSE )  size += sizeof(DWORD);
    if( dwFVF & D3DFVF_SPECULAR ) size += sizeof(DWORD);

    UINT nTex = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for( UINT i = 0; i < nTex; i++ )
    {
        switch( (dwFVF >> (16 + i * 2)) & 3 )
        {
            case D3DFVF_TEXTUREFORMAT1: size += 1 * sizeof(FLOAT); break;
            case D3DFVF_TEXTUREFORMAT2: size += 2 * sizeof(FLOAT); break;
            case D3DFVF_TEXTUREFORMAT3: size += 3 * sizeof(FLOAT); break;
            case D3DFVF_TEXTUREFORMAT4: size += 4 * sizeof(FLOAT); break;
        }
    }
    return size;
}

// Offsets of the parts of a vertex (-1: not present)
struct FVFLAYOUT
{
    int  position;          // size: positionSize
    UINT positionSize;
    int  normal;
    int  psize;
    int  diffuse;
    int  specular;
    int  tex[8];            // size: texSize[i]
    UINT texSize[8];
};

static void FVFGetLayout( DWORD dwFVF, FVFLAYOUT* p )
{
    UINT offset;

    p->position = 0;
    p->positionSize = FVFVertexSize( dwFVF & D3DFVF_POSITION_MASK );
    offset = p->positionSize;
    p->normal = -1;
    p->psize = -1;
    p->diffuse = -1;
    p->specular = -1;
    if( dwFVF & D3DFVF_NORMAL )   { p->normal = offset; offset += 3 * sizeof(FLOAT); }
    if( dwFVF & D3DFVF_PSIZE )    { p->psize = offset; offset += sizeof(FLOAT); }
    if( dwFVF & D3DFVF_DIFFUSE )  { p->diffuse = offset; offset += sizeof(DWORD); }
    if( dwFVF & D3DFVF_SPECULAR ) { p->specular = offset; offset += sizeof(DWORD); }

    UINT nTex = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for( UINT i = 0; i < 8; i++ )
    {
        p->tex[i] = -1;
        p->texSize[i] = 0;
        if( i < nTex )
        {
            static const UINT s_size[4] = { 2, 3, 4, 1 };   // D3DFVF_TEXTUREFORMAT2, 3, 4, 1
            p->tex[i] = offset;
            p->texSize[i] = s_size[(dwFVF >> (16 + i * 2)) & 3] * sizeof(FLOAT);
            offset += p->texSize[i];
        }
    }
}


//===========================================================================
// ID3DXMesh
//===========================================================================
class CD3DXMesh : public ID3DXMesh
{
    LONG                    m_cRef;
    DWORD                   m_dwOptions;
    DWORD                   m_dwFVF;
    UINT                    m_uStride;
    DWORD                   m_dwNumFaces;
    DWORD                   m_dwNumVertices;
    LPDIRECT3DDEVICE8       m_pDevice;
    LPDIRECT3DVERTEXBUFFER8 m_pVB;
    LPDIRECT3DINDEXBUFFER8  m_pIB;
    DWORD*                  m_pAttribs;

public:
    CD3DXMesh() : m_cRef(1), m_dwOptions(0), m_dwFVF(0), m_uStride(0), m_dwNumFaces(0),
                  m_dwNumVertices(0), m_pDevice(NULL), m_pVB(NULL), m_pIB(NULL), m_pAttribs(NULL) {}

    virtual ~CD3DXMesh()
    {
        if( m_pVB ) m_pVB->Release();
        if( m_pIB ) m_pIB->Release();
        if( m_pDevice ) m_pDevice->Release();
        delete [] m_pAttribs;
    }

    HRESULT Init( DWORD NumFaces, DWORD NumVertices, DWORD Options, DWORD FVF, LPDIRECT3DDEVICE8 pDevice )
    {
        HRESULT hr;
        D3DPOOL pool;
        DWORD usage;

        if( NumFaces == 0 || NumVertices == 0 || pDevice == NULL )
            return D3DERR_INVALIDCALL;
        if( !(Options & D3DXMESH_32BIT) && NumVertices > 0xffff )
            return D3DERR_INVALIDCALL;

        m_dwOptions = Options;
        m_dwFVF = FVF;
        m_uStride = FVFVertexSize( FVF );
        m_dwNumFaces = NumFaces;
        m_dwNumVertices = NumVertices;
        m_pDevice = pDevice;
        m_pDevice->AddRef();

        // Vertex buffer
        pool = (Options & D3DXMESH_VB_SYSTEMMEM) ? D3DPOOL_SYSTEMMEM :
               (Options & D3DXMESH_VB_MANAGED) ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
        usage = 0;
        if( Options & D3DXMESH_VB_WRITEONLY )           usage |= D3DUSAGE_WRITEONLY;
        if( Options & D3DXMESH_VB_DYNAMIC )             usage |= D3DUSAGE_DYNAMIC;
        if( Options & D3DXMESH_VB_SOFTWAREPROCESSING )  usage |= D3DUSAGE_SOFTWAREPROCESSING;
        if( Options & D3DXMESH_DONOTCLIP )              usage |= D3DUSAGE_DONOTCLIP;
        if( Options & D3DXMESH_POINTS )                 usage |= D3DUSAGE_POINTS;
        if( Options & D3DXMESH_RTPATCHES )              usage |= D3DUSAGE_RTPATCHES;
        if( Options & D3DXMESH_NPATCHES )               usage |= D3DUSAGE_NPATCHES;
        hr = m_pDevice->CreateVertexBuffer( m_uStride * NumVertices, usage, FVF, pool, &m_pVB );
        if( FAILED( hr ) )
            return hr;

        // Index buffer
        pool = (Options & D3DXMESH_IB_SYSTEMMEM) ? D3DPOOL_SYSTEMMEM :
               (Options & D3DXMESH_IB_MANAGED) ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
        usage = 0;
        if( Options & D3DXMESH_IB_WRITEONLY )           usage |= D3DUSAGE_WRITEONLY;
        if( Options & D3DXMESH_IB_DYNAMIC )             usage |= D3DUSAGE_DYNAMIC;
        if( Options & D3DXMESH_IB_SOFTWAREPROCESSING )  usage |= D3DUSAGE_SOFTWAREPROCESSING;
        hr = m_pDevice->CreateIndexBuffer( NumFaces * 3 * ((Options & D3DXMESH_32BIT) ? 4 : 2), usage,
            (Options & D3DXMESH_32BIT) ? D3DFMT_INDEX32 : D3DFMT_INDEX16, pool, &m_pIB );
        if( FAILED( hr ) )
            return hr;

        // Attributes (all faces belong to subset 0 at first)
        m_pAttribs = new (std::nothrow) DWORD[NumFaces];
        if( m_pAttribs == NULL )
            return E_OUTOFMEMORY;
        ZeroMemory( m_pAttribs, NumFaces * sizeof(DWORD) );

        return S_OK;
    }

    // IUnknown
    STDMETHOD(QueryInterface)( REFIID riid, LPVOID* ppv )
    {
        if( IsEqualIID( riid, IID_IUnknown ) || IsEqualIID( riid, IID_ID3DXBaseMesh ) ||
            IsEqualIID( riid, IID_ID3DXMesh ) )
        {
            AddRef();
            *ppv = this;
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement( &m_cRef ); }
    STDMETHOD_(ULONG, Release)()
    {
        ULONG cRef = InterlockedDecrement( &m_cRef );
        if( cRef == 0 )
            delete this;
        return cRef;
    }

    // ID3DXBaseMesh
    STDMETHOD(DrawSubset)( DWORD AttribId )
    {
        HRESULT hr;

        m_pDevice->SetVertexShader( m_dwFVF );
        m_pDevice->SetStreamSource( 0, m_pVB, m_uStride );
        m_pDevice->SetIndices( m_pIB, 0 );

        // Draw each run of faces of the subset
        DWORD iFace = 0;
        while( iFace < m_dwNumFaces )
        {
            if( m_pAttribs[iFace] != AttribId )
            {
                iFace++;
                continue;
            }
            DWORD iStart = iFace;
            while( iFace < m_dwNumFaces && m_pAttribs[iFace] == AttribId )
                iFace++;
            hr = m_pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, m_dwNumVertices,
                                                  iStart * 3, iFace - iStart );
            if( FAILED( hr ) )
                return hr;
        }
        return S_OK;
    }
    STDMETHOD_(DWORD, GetNumFaces)() { return m_dwNumFaces; }
    STDMETHOD_(DWORD, GetNumVertices)() { return m_dwNumVertices; }
    STDMETHOD_(DWORD, GetFVF)() { return m_dwFVF; }
    STDMETHOD(GetDeclaration)( DWORD Declaration[MAX_FVF_DECL_SIZE] ) { return E_NOTIMPL; }
    STDMETHOD_(DWORD, GetOptions)() { return m_dwOptions; }
    STDMETHOD(GetDevice)( LPDIRECT3DDEVICE8* ppDevice )
    {
        if( ppDevice == NULL )
            return D3DERR_INVALIDCALL;
        m_pDevice->AddRef();
        *ppDevice = m_pDevice;
        return S_OK;
    }
    STDMETHOD(CloneMeshFVF)( DWORD Options, DWORD FVF, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh );
    STDMETHOD(CloneMesh)( DWORD Options, CONST DWORD *pDeclaration, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh )
    {
        return E_NOTIMPL;
    }
    STDMETHOD(GetVertexBuffer)( LPDIRECT3DVERTEXBUFFER8* ppVB )
    {
        if( ppVB == NULL )
            return D3DERR_INVALIDCALL;
        m_pVB->AddRef();
        *ppVB = m_pVB;
        return S_OK;
    }
    STDMETHOD(GetIndexBuffer)( LPDIRECT3DINDEXBUFFER8* ppIB )
    {
        if( ppIB == NULL )
            return D3DERR_INVALIDCALL;
        m_pIB->AddRef();
        *ppIB = m_pIB;
        return S_OK;
    }
    STDMETHOD(LockVertexBuffer)( DWORD Flags, BYTE** ppData ) { return m_pVB->Lock( 0, 0, ppData, Flags ); }
    STDMETHOD(UnlockVertexBuffer)() { return m_pVB->Unlock(); }
    STDMETHOD(LockIndexBuffer)( DWORD Flags, BYTE** ppData ) { return m_pIB->Lock( 0, 0, ppData, Flags ); }
    STDMETHOD(UnlockIndexBuffer)() { return m_pIB->Unlock(); }
    STDMETHOD(GetAttributeTable)( D3DXATTRIBUTERANGE *pAttribTable, DWORD* pAttribTableSize )
    {
        // (no attribute table until the mesh is optimized)
        if( pAttribTableSize )
            *pAttribTableSize = 0;
        return S_OK;
    }
    STDMETHOD(ConvertPointRepsToAdjacency)( CONST DWORD* pPRep, DWORD* pAdjacency ) { return E_NOTIMPL; }
    STDMETHOD(ConvertAdjacencyToPointReps)( CONST DWORD* pAdjacency, DWORD* pPRep ) { return E_NOTIMPL; }
    STDMETHOD(GenerateAdjacency)( FLOAT fEpsilon, DWORD* pAdjacency );

    // ID3DXMesh
    STDMETHOD(LockAttributeBuffer)( DWORD Flags, DWORD** ppData )
    {
        if( ppData == NULL )
            return D3DERR_INVALIDCALL;
        *ppData = m_pAttribs;
        return S_OK;
    }
    STDMETHOD(UnlockAttributeBuffer)() { return S_OK; }
    STDMETHOD(Optimize)( DWORD Flags, CONST DWORD* pAdjacencyIn, DWORD* pAdjacencyOut,
                         DWORD* pFaceRemap, LPD3DXBUFFER *ppVertexRemap, LPD3DXMESH* ppOptMesh )
    {
        return E_NOTIMPL;
    }
    STDMETHOD(OptimizeInplace)( DWORD Flags, CONST DWORD* pAdjacencyIn, DWORD* pAdjacencyOut,
                                DWORD* pFaceRemap, LPD3DXBUFFER *ppVertexRemap )
    {
        return E_NOTIMPL;
    }

    // Reads the three vertex indices of a face
    void GetFace( const BYTE* pIndices, DWORD iFace, DWORD v[3] )
    {
        for( int i = 0; i < 3; i++ )
        {
            if( m_dwOptions & D3DXMESH_32BIT )
                v[i] = ((const DWORD*)pIndices)[iFace * 3 + i];
            else
                v[i] = ((const WORD*)pIndices)[iFace * 3 + i];
        }
    }
};


// Copies the vertices into a mesh with another vertex format; the parts that
// are missing in the source are zero (diffuse: opaque white)
HRESULT CD3DXMesh::CloneMeshFVF( DWORD Options, DWORD FVF, LPDIRECT3DDEVICE8 pD3DDevice, LPD3DXMESH* ppCloneMesh )
{
    HRESULT hr;
    CD3DXMesh* pMesh;
    BYTE *pSrc, *pDst;
    FVFLAYOUT src, dst;

    if( ppCloneMesh == NULL || pD3DDevice == NULL )
        return D3DERR_INVALIDCALL;

    pMesh = new (std::nothrow) CD3DXMesh;
    if( pMesh == NULL )
        return E_OUTOFMEMORY;
    if( FAILED( hr = pMesh->Init( m_dwNumFaces, m_dwNumVertices, Options, FVF, pD3DDevice ) ) )
    {
        pMesh->Release();
        return hr;
    }

    FVFGetLayout( m_dwFVF, &src );
    FVFGetLayout( FVF, &dst );

    // Vertices
    if( SUCCEEDED( hr = LockVertexBuffer( D3DLOCK_READONLY, &pSrc ) ) )
    {
        if( SUCCEEDED( hr = pMesh->LockVertexBuffer( 0, &pDst ) ) )
        {
            ZeroMemory( pDst, pMesh->m_uStride * m_dwNumVertices );
            for( DWORD i = 0; i < m_dwNumVertices; i++ )
            {
                BYTE* s = pSrc + i * m_uStride;
                BYTE* d = pDst + i * pMesh->m_uStride;

                memcpy( d + dst.position, s + src.position,
                        src.positionSize < dst.positionSize ? src.positionSize : dst.positionSize );
                if( dst.normal >= 0 && src.normal >= 0 )
                    memcpy( d + dst.normal, s + src.normal, 3 * sizeof(FLOAT) );
                if( dst.psize >= 0 && src.psize >= 0 )
                    memcpy( d + dst.psize, s + src.psize, sizeof(FLOAT) );
                if( dst.diffuse >= 0 )
                {
                    if( src.diffuse >= 0 )
                        memcpy( d + dst.diffuse, s + src.diffuse, sizeof(DWORD) );
                    else
                        *(DWORD*)(d + dst.diffuse) = 0xffffffff;
                }
                if( dst.specular >= 0 && src.specular >= 0 )
                    memcpy( d + dst.specular, s + src.specular, sizeof(DWORD) );
                for( int t = 0; t < 8; t++ )
                {
                    if( dst.tex[t] >= 0 && src.tex[t] >= 0 )
                        memcpy( d + dst.tex[t], s + src.tex[t],
                                src.texSize[t] < dst.texSize[t] ? src.texSize[t] : dst.texSize[t] );
                }
            }
            pMesh->UnlockVertexBuffer();
        }
        UnlockVertexBuffer();
    }

    // Indices
    if( SUCCEEDED( hr ) && SUCCEEDED( hr = LockIndexBuffer( D3DLOCK_READONLY, &pSrc ) ) )
    {
        if( SUCCEEDED( hr = pMesh->LockIndexBuffer( 0, &pDst ) ) )
        {
            for( DWORD i = 0; i < m_dwNumFaces; i++ )
            {
                DWORD v[3];
                GetFace( pSrc, i, v );
                for( int j = 0; j < 3; j++ )
                {
                    if( Options & D3DXMESH_32BIT )
                        ((DWORD*)pDst)[i * 3 + j] = v[j];
                    else
                        ((WORD*)pDst)[i * 3 + j] = (WORD)v[j];
                }
            }
            pMesh->UnlockIndexBuffer();
        }
        UnlockIndexBuffer();
    }

    if( FAILED( hr ) )
    {
        pMesh->Release();
        return hr;
    }

    memcpy( pMesh->m_pAttribs, m_pAttribs, m_dwNumFaces * sizeof(DWORD) );

    *ppCloneMesh = pMesh;
    return S_OK;
}


// Faces that share an edge (the same two vertex indices) are neighbours
HRESULT CD3DXMesh::GenerateAdjacency( FLOAT fEpsilon, DWORD* pAdjacency )
{
    HRESULT hr;
    BYTE* pIndices;

    if( pAdjacency == NULL )
        return D3DERR_INVALIDCALL;

    if( FAILED( hr = LockIndexBuffer( D3DLOCK_READONLY, &pIndices ) ) )
        return hr;

    for( DWORD i = 0; i < m_dwNumFaces * 3; i++ )
        pAdjacency[i] = 0xffffffff;

    for( DWORD f = 0; f < m_dwNumFaces; f++ )
    {
        DWORD a[3];
        GetFace( pIndices, f, a );
        for( int e = 0; e < 3; e++ )
        {
            if( pAdjacency[f * 3 + e] != 0xffffffff )
                continue;
            DWORD a0 = a[e], a1 = a[(e + 1) % 3];
            for( DWORD g = f + 1; g < m_dwNumFaces && pAdjacency[f * 3 + e] == 0xffffffff; g++ )
            {
                DWORD b[3];
                GetFace( pIndices, g, b );
                for( int k = 0; k < 3; k++ )
                {
                    if( b[k] == a1 && b[(k + 1) % 3] == a0 && pAdjacency[g * 3 + k] == 0xffffffff )
                    {
                        pAdjacency[f * 3 + e] = g;
                        pAdjacency[g * 3 + k] = f;
                        break;
                    }
                }
            }
        }
    }

    UnlockIndexBuffer();
    return S_OK;
}


HRESULT WINAPI D3DXCreateMeshFVF( DWORD NumFaces, DWORD NumVertices, DWORD Options, DWORD FVF,
                                  LPDIRECT3DDEVICE8 pD3D, LPD3DXMESH* ppMesh )
{
    HRESULT hr;

    if( ppMesh == NULL )
        return D3DERR_INVALIDCALL;

    CD3DXMesh* pMesh = new (std::nothrow) CD3DXMesh;
    if( pMesh == NULL )
        return E_OUTOFMEMORY;

    if( FAILED( hr = pMesh->Init( NumFaces, NumVertices, Options, FVF, pD3D ) ) )
    {
        pMesh->Release();
        return hr;
    }

    *ppMesh = pMesh;
    return S_OK;
}


// {932E6A7E-C68E-45dd-A7BF-53D19C86DB1F}
extern "C" const GUID IID_ID3DXBuffer =
    { 0x932e6a7e, 0xc68e, 0x45dd, { 0xa7, 0xbf, 0x53, 0xd1, 0x9c, 0x86, 0xdb, 0x1f } };
// {A69BA991-1F7D-11d3-B929-00C04F68DC23}
extern "C" const GUID IID_ID3DXBaseMesh =
    { 0xa69ba991, 0x1f7d, 0x11d3, { 0xb9, 0x29, 0x00, 0xc0, 0x4f, 0x68, 0xdc, 0x23 } };
// {9D84AC46-6B90-49a9-A721-085C7A3E3DAE}
extern "C" const GUID IID_ID3DXMesh =
    { 0x9d84ac46, 0x6b90, 0x49a9, { 0xa7, 0x21, 0x08, 0x5c, 0x7a, 0x3e, 0x3d, 0xae } };
