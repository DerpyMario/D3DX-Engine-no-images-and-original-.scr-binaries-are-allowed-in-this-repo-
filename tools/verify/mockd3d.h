//-----------------------------------------------------------------------------
// File: mockd3d.h
//
// Desc: Recording stand-ins for the Direct3D 9 objects the screen savers talk
//       to.  Every method call that reaches a mock object is appended to a
//       call log (interface, method, arguments, and for a few methods the
//       data an argument points to).  A test runs the original code and the
//       rebuilt code against two mock devices and then compares the logs: if
//       both issued exactly the same calls in the same order, the rebuilt
//       function drives Direct3D the way the original does.
//
//       The mocks are plain structures with hand-built vtables, so the same
//       object can be handed to code compiled by any compiler.  x64 only.
//-----------------------------------------------------------------------------
#ifndef _VERIFY_MOCKD3D_H
#define _VERIFY_MOCKD3D_H

#include <windows.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include "comslots.h"

#define MOCK_MAX_ARGS   9
#define MOCK_MAX_SLOTS  130

struct CallRecord
{
    const char*         pszInterface;
    const char*         pszMethod;
    int                 nArgs;
    unsigned long long  args[MOCK_MAX_ARGS];
    bool                bCompare[MOCK_MAX_ARGS];
    std::vector<BYTE>   payload;
};

class CallLog
{
public:
    std::vector<CallRecord> records;

    void Clear()    { records.clear(); }

    void Print( size_t nMax = 1000 ) const
    {
        for( size_t i = 0; i < records.size() && i < nMax; i++ )
            PrintRecord( i, records[i] );
    }

    static void PrintRecord( size_t i, const CallRecord& r )
    {
        printf( "    [%3u] %s::%s(", (unsigned)i, r.pszInterface, r.pszMethod );
        for( int a = 0; a < r.nArgs; a++ )
        {
            if( r.bCompare[a] )
                printf( "%s0x%llx", a ? ", " : "", r.args[a] );
            else
                printf( "%s?", a ? ", " : "" );
        }
        printf( ")" );
        if( !r.payload.empty() )
        {
            printf( " {" );
            for( size_t k = 0; k < r.payload.size() && k < 24; k++ )
                printf( "%02x", r.payload[k] );
            printf( r.payload.size() > 24 ? "...}" : "}" );
        }
        printf( "\n" );
    }

    // Compares two logs; prints the first few differences.  Payloads made of
    // floats may be compared with a tolerance.
    static bool Equal( const CallLog& a, const CallLog& b, float fFloatTolerance = 0.0f )
    {
        bool bEqual = true;
        int  nReported = 0;

        if( a.records.size() != b.records.size() )
        {
            printf( "    call count differs: original %u, rebuilt %u\n",
                    (unsigned)a.records.size(), (unsigned)b.records.size() );
            bEqual = false;
        }
        size_t n = a.records.size() < b.records.size() ? a.records.size() : b.records.size();
        for( size_t i = 0; i < n; i++ )
        {
            const CallRecord& ra = a.records[i];
            const CallRecord& rb = b.records[i];
            bool bSame = ( strcmp( ra.pszInterface, rb.pszInterface ) == 0 &&
                           strcmp( ra.pszMethod, rb.pszMethod ) == 0 && ra.nArgs == rb.nArgs );
            for( int k = 0; bSame && k < ra.nArgs; k++ )
            {
                if( ra.bCompare[k] && ra.args[k] != rb.args[k] )
                    bSame = false;
            }
            if( bSame && ra.payload.size() != rb.payload.size() )
                bSame = false;
            if( bSame && !ra.payload.empty() && memcmp( &ra.payload[0], &rb.payload[0], ra.payload.size() ) != 0 )
            {
                if( fFloatTolerance > 0.0f && ( ra.payload.size() % 4 ) == 0 )
                {
                    const float* fa = (const float*)&ra.payload[0];
                    const float* fb = (const float*)&rb.payload[0];
                    for( size_t k = 0; k < ra.payload.size() / 4; k++ )
                    {
                        float d = fa[k] - fb[k];
                        if( d < 0.0f ) d = -d;
                        if( !( d <= fFloatTolerance ) )
                            bSame = false;
                    }
                }
                else
                {
                    bSame = false;
                }
            }
            if( !bSame )
            {
                bEqual = false;
                if( nReported++ < 6 )
                {
                    printf( "    difference at call %u:\n      original:", (unsigned)i );
                    PrintRecord( i, ra );
                    printf( "      rebuilt: " );
                    PrintRecord( i, rb );
                }
            }
        }
        return bEqual;
    }
};

//-----------------------------------------------------------------------------
// Name: struct MockObject
// Desc: Base of every mock COM object: a vtable pointer followed by our data.
//-----------------------------------------------------------------------------
struct MockObject;
typedef ULONG_PTR ( *MockHandler )( MockObject* pThis, int iSlot, ULONG_PTR* pArgs, CallRecord* pRecord );

struct MockObject
{
    void**          lpVtbl;         // must be first
    const char*     pszInterface;
    const ComSlot*  pSlots;
    int             nSlots;
    CallLog*        pLog;
    MockHandler     pfnHandler;
    LONG            cRef;
    bool            bRecord;

    // Data used by the concrete mocks below
    std::vector<BYTE> buffer;       // vertex data / pixels
    UINT            uWidth;
    UINT            uHeight;
    UINT            uPitch;
    D3DFORMAT       format;
    MockObject*     pChild;         // texture -> surface of level 0
    D3DCAPS9        caps;           // device
    D3DVIEWPORT9    viewport;       // device
    std::vector<MockObject*> created;   // device: everything it handed out
};

static inline ULONG_PTR MockDispatch( MockObject* pThis, int iSlot, ULONG_PTR* pArgs )
{
    CallRecord rec;
    rec.pszInterface = pThis->pszInterface;
    rec.pszMethod = ( iSlot < pThis->nSlots ) ? pThis->pSlots[iSlot].pszName : "?";
    const char* pszKinds = ( iSlot < pThis->nSlots ) ? pThis->pSlots[iSlot].pszArgs : "";
    rec.nArgs = (int)strlen( pszKinds );
    for( int i = 0; i < rec.nArgs && i < MOCK_MAX_ARGS; i++ )
    {
        rec.bCompare[i] = true;
        switch( pszKinds[i] )
        {
            case 'D': rec.args[i] = pArgs[i] & 0xffffffffull; break;
            case 'Q': rec.args[i] = pArgs[i]; break;
            case 'P': rec.args[i] = ( pArgs[i] != 0 ); break;         // only NULL / non-NULL is comparable
            case 'F':
                // The first three arguments after "this" travel in XMM
                // registers, which these thunks do not capture.
                rec.args[i] = pArgs[i] & 0xffffffffull;
                rec.bCompare[i] = ( i >= 3 );
                break;
            default:  rec.args[i] = 0; rec.bCompare[i] = false; break;
        }
    }

    ULONG_PTR result = 0;       // S_OK
    if( pThis->pfnHandler != NULL )
        result = pThis->pfnHandler( pThis, iSlot, pArgs, &rec );

    if( pThis->bRecord && pThis->pLog != NULL )
        pThis->pLog->records.push_back( rec );
    return result;
}

template <int N>
static ULONG_PTR MockThunk( MockObject* pThis, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, ULONG_PTR a4,
                            ULONG_PTR a5, ULONG_PTR a6, ULONG_PTR a7, ULONG_PTR a8, ULONG_PTR a9 )
{
    ULONG_PTR args[MOCK_MAX_ARGS] = { a1, a2, a3, a4, a5, a6, a7, a8, a9 };
    return MockDispatch( pThis, N, args );
}

template <int N> struct MockVtblFiller
{
    static void Fill( void** pVtbl )
    {
        pVtbl[N] = (void*)&MockThunk<N>;
        MockVtblFiller<N - 1>::Fill( pVtbl );
    }
};
template <> struct MockVtblFiller<-1>
{
    static void Fill( void** ) { }
};

static inline void** MockVtbl()
{
    static void* s_Vtbl[MOCK_MAX_SLOTS];
    static bool s_bFilled = false;
    if( !s_bFilled )
    {
        MockVtblFiller<MOCK_MAX_SLOTS - 1>::Fill( s_Vtbl );
        s_bFilled = true;
    }
    return s_Vtbl;
}

#define MOCK_SLOTS(x)   g_Slots_##x, (int)( sizeof(g_Slots_##x) / sizeof(g_Slots_##x[0]) )

static inline MockObject* MockCreate( const char* pszInterface, const ComSlot* pSlots, int nSlots,
                                      CallLog* pLog, MockHandler pfnHandler )
{
    MockObject* p = new MockObject;
    p->lpVtbl = MockVtbl();
    p->pszInterface = pszInterface;
    p->pSlots = pSlots;
    p->nSlots = nSlots;
    p->pLog = pLog;
    p->pfnHandler = pfnHandler;
    p->cRef = 1;
    p->bRecord = true;
    p->uWidth = p->uHeight = p->uPitch = 0;
    p->format = D3DFMT_UNKNOWN;
    p->pChild = NULL;
    memset( &p->caps, 0, sizeof(p->caps) );
    memset( &p->viewport, 0, sizeof(p->viewport) );
    return p;
}

static inline void MockPayload( CallRecord* pRecord, const void* pData, size_t cb )
{
    if( pData != NULL && cb != 0 )
        pRecord->payload.insert( pRecord->payload.end(), (const BYTE*)pData, (const BYTE*)pData + cb );
}

// FNV-1a, to keep pixel / vertex payloads short
static inline void MockPayloadHash( CallRecord* pRecord, const void* pData, size_t cb )
{
    unsigned long long h = 1469598103934665603ull;
    for( size_t i = 0; i < cb; i++ )
    {
        h ^= ( (const BYTE*)pData )[i];
        h *= 1099511628211ull;
    }
    MockPayload( pRecord, &h, sizeof(h) );
}

//-----------------------------------------------------------------------------
// IUnknown part, shared by all mocks
//-----------------------------------------------------------------------------
static inline bool MockUnknown( MockObject* pThis, int iSlot, ULONG_PTR* pResult )
{
    switch( iSlot )
    {
        case 0: *pResult = (ULONG_PTR)E_NOINTERFACE; return true;
        case 1: *pResult = (ULONG_PTR)++pThis->cRef; return true;
        case 2: *pResult = (ULONG_PTR)--pThis->cRef; return true;      // never actually freed
    }
    return false;
}

//-----------------------------------------------------------------------------
// IDirect3DVertexBuffer9 / IDirect3DIndexBuffer9
//-----------------------------------------------------------------------------
static ULONG_PTR MockBufferHandler( MockObject* pThis, int iSlot, ULONG_PTR* pArgs, CallRecord* pRecord )
{
    ULONG_PTR result = 0;
    (void)pRecord;
    if( MockUnknown( pThis, iSlot, &result ) )
        return result;
    switch( iSlot )
    {
        case 11:    // Lock( OffsetToLock, SizeToLock, ppbData, Flags )
            *(void**)pArgs[2] = pThis->buffer.empty() ? NULL : &pThis->buffer[(UINT)pArgs[0]];
            break;
        case 12:    // Unlock()
            break;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// IDirect3DSurface9
//-----------------------------------------------------------------------------
static ULONG_PTR MockSurfaceHandler( MockObject* pThis, int iSlot, ULONG_PTR* pArgs, CallRecord* pRecord )
{
    ULONG_PTR result = 0;
    (void)pRecord;
    if( MockUnknown( pThis, iSlot, &result ) )
        return result;
    switch( iSlot )
    {
        case 12:    // GetDesc( pDesc )
        {
            D3DSURFACE_DESC* pDesc = (D3DSURFACE_DESC*)pArgs[0];
            memset( pDesc, 0, sizeof(*pDesc) );
            pDesc->Format = pThis->format;
            pDesc->Type = D3DRTYPE_SURFACE;
            pDesc->Width = pThis->uWidth;
            pDesc->Height = pThis->uHeight;
            break;
        }
        case 13:    // LockRect( pLockedRect, pRect, Flags )
        {
            D3DLOCKED_RECT* pLock = (D3DLOCKED_RECT*)pArgs[0];
            pLock->Pitch = (INT)pThis->uPitch;
            pLock->pBits = pThis->buffer.empty() ? NULL : &pThis->buffer[0];
            break;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
// IDirect3DTexture9
//-----------------------------------------------------------------------------
static ULONG_PTR MockTextureHandler( MockObject* pThis, int iSlot, ULONG_PTR* pArgs, CallRecord* pRecord )
{
    ULONG_PTR result = 0;
    (void)pRecord;
    if( MockUnknown( pThis, iSlot, &result ) )
        return result;
    switch( iSlot )
    {
        case 17:    // GetLevelDesc( Level, pDesc )
        {
            D3DSURFACE_DESC* pDesc = (D3DSURFACE_DESC*)pArgs[1];
            memset( pDesc, 0, sizeof(*pDesc) );
            pDesc->Format = pThis->pChild->format;
            pDesc->Type = D3DRTYPE_SURFACE;
            pDesc->Width = pThis->pChild->uWidth;
            pDesc->Height = pThis->pChild->uHeight;
            break;
        }
        case 18:    // GetSurfaceLevel( Level, ppSurfaceLevel )
            pThis->pChild->cRef++;
            *(MockObject**)pArgs[1] = pThis->pChild;
            break;
        case 19:    // LockRect( Level, pLockedRect, pRect, Flags )
        {
            D3DLOCKED_RECT* pLock = (D3DLOCKED_RECT*)pArgs[1];
            pLock->Pitch = (INT)pThis->pChild->uPitch;
            pLock->pBits = &pThis->pChild->buffer[0];
            break;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
// IDirect3DDevice9
//-----------------------------------------------------------------------------
static ULONG_PTR MockDeviceHandler( MockObject* pThis, int iSlot, ULONG_PTR* pArgs, CallRecord* pRecord )
{
    ULONG_PTR result = 0;
    if( MockUnknown( pThis, iSlot, &result ) )
        return result;
    switch( iSlot )
    {
        case 7:     // GetDeviceCaps( pCaps )
            *(D3DCAPS9*)pArgs[0] = pThis->caps;
            break;

        case 23:    // CreateTexture( Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle )
        {
            MockObject* pSurface = MockCreate( "IDirect3DSurface9", MOCK_SLOTS( IDirect3DSurface9 ),
                                               pThis->pLog, MockSurfaceHandler );
            pSurface->uWidth = (UINT)pArgs[0];
            pSurface->uHeight = (UINT)pArgs[1];
            pSurface->uPitch = pSurface->uWidth * 4;
            pSurface->format = (D3DFORMAT)(UINT)pArgs[4];
            pSurface->buffer.assign( (size_t)pSurface->uPitch * pSurface->uHeight + 64, 0xCD );
            MockObject* pTexture = MockCreate( "IDirect3DTexture9", MOCK_SLOTS( IDirect3DTexture9 ),
                                               pThis->pLog, MockTextureHandler );
            pTexture->pChild = pSurface;
            pThis->created.push_back( pTexture );
            *(MockObject**)pArgs[6] = pTexture;
            break;
        }

        case 26:    // CreateVertexBuffer( Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle )
        {
            MockObject* pVB = MockCreate( "IDirect3DVertexBuffer9", MOCK_SLOTS( IDirect3DVertexBuffer9 ),
                                          pThis->pLog, MockBufferHandler );
            pVB->buffer.assign( (size_t)(UINT)pArgs[0] + 64, 0xCD );
            pThis->created.push_back( pVB );
            *(MockObject**)pArgs[4] = pVB;
            break;
        }

        case 27:    // CreateIndexBuffer( Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle )
        {
            MockObject* pIB = MockCreate( "IDirect3DIndexBuffer9", MOCK_SLOTS( IDirect3DIndexBuffer9 ),
                                          pThis->pLog, MockBufferHandler );
            pIB->buffer.assign( (size_t)(UINT)pArgs[0] + 64, 0xCD );
            pThis->created.push_back( pIB );
            *(MockObject**)pArgs[4] = pIB;
            break;
        }

        case 30:    // UpdateSurface( pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint )
        {
            // Record what is being uploaded: this is how decoded images get compared
            MockObject* pSource = (MockObject*)pArgs[0];
            if( pSource != NULL && !pSource->buffer.empty() )
            {
                MockPayload( pRecord, &pSource->uWidth, sizeof(UINT) );
                MockPayload( pRecord, &pSource->uHeight, sizeof(UINT) );
                MockPayloadHash( pRecord, &pSource->buffer[0], (size_t)pSource->uPitch * pSource->uHeight );
            }
            break;
        }

        case 83:    // DrawPrimitiveUP( PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride )
        {
            UINT cPrimitives = (UINT)pArgs[1];
            UINT cVertices = 0;
            switch( (UINT)pArgs[0] )
            {
                case D3DPT_POINTLIST:     cVertices = cPrimitives; break;
                case D3DPT_LINELIST:      cVertices = cPrimitives * 2; break;
                case D3DPT_LINESTRIP:     cVertices = cPrimitives + 1; break;
                case D3DPT_TRIANGLELIST:  cVertices = cPrimitives * 3; break;
                case D3DPT_TRIANGLESTRIP:
                case D3DPT_TRIANGLEFAN:   cVertices = cPrimitives + 2; break;
            }
            MockPayload( pRecord, (void*)pArgs[2], (size_t)cVertices * (UINT)pArgs[3] );
            break;
        }

        case 44:    // SetTransform( State, pMatrix )
            MockPayload( pRecord, (void*)pArgs[1], sizeof(D3DMATRIX) );
            break;

        case 47:    // SetViewport( pViewport )
            MockPayload( pRecord, (void*)pArgs[0], sizeof(D3DVIEWPORT9) );
            pThis->viewport = *(D3DVIEWPORT9*)pArgs[0];
            break;

        case 48:    // GetViewport( pViewport )
            *(D3DVIEWPORT9*)pArgs[0] = pThis->viewport;
            break;

        case 49:    // SetMaterial( pMaterial )
            MockPayload( pRecord, (void*)pArgs[0], sizeof(D3DMATERIAL9) );
            break;

        case 51:    // SetLight( Index, pLight )
            MockPayload( pRecord, (void*)pArgs[1], sizeof(D3DLIGHT9) );
            break;
    }
    return 0;
}

// A texture that was not created through the mock device (to pre-load slots)
static inline MockObject* MockCreateTexture( CallLog* pLog, UINT uWidth, UINT uHeight )
{
    MockObject* pSurface = MockCreate( "IDirect3DSurface9", MOCK_SLOTS( IDirect3DSurface9 ), pLog, MockSurfaceHandler );
    pSurface->uWidth = uWidth;
    pSurface->uHeight = uHeight;
    pSurface->uPitch = uWidth * 4;
    pSurface->format = D3DFMT_A8R8G8B8;
    pSurface->buffer.assign( (size_t)pSurface->uPitch * uHeight + 64, 0xCD );
    MockObject* pTexture = MockCreate( "IDirect3DTexture9", MOCK_SLOTS( IDirect3DTexture9 ), pLog, MockTextureHandler );
    pTexture->pChild = pSurface;
    return pTexture;
}

static inline MockObject* MockCreateDevice( CallLog* pLog )
{
    MockObject* pDevice = MockCreate( "IDirect3DDevice9", MOCK_SLOTS( IDirect3DDevice9 ), pLog, MockDeviceHandler );
    pDevice->viewport.Width = 1280;
    pDevice->viewport.Height = 720;
    pDevice->viewport.MaxZ = 1.0f;
    return pDevice;
}

#endif // _VERIFY_MOCKD3D_H
