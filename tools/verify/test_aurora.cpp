//-----------------------------------------------------------------------------
// File: test_aurora.cpp
//
// Desc: Differential test: reconstructed Aurora / Helpers sources against the
//       original Aurora.scr (6.0.6000.16386, amd64, link time 0x4549BB49).
//
//           test_aurora.exe <path to original Aurora.scr>
//
//       Both sides draw their random numbers from the same msvcrt rand(), so
//       after an identical srand() they see identical sequences.
//-----------------------------------------------------------------------------
#include <new>
#include "origimage.h"
#include "mockd3d.h"

#include "point.h"
#include "noise.h"
#include "matrix44.h"
#include "image.h"
#include "targa.h"
#include "texture.h"
#include "camera.h"
#include "arc.h"
#include "dxutil9.h"

// Aurora.scr 6.0.6000.16386 (amd64)
#define AURORA_TIMESTAMP                0x4549bb49
#define RVA_ArcClass_ctor               0x15808
#define RVA_ArcClass_InitDevice         0x15adc
#define RVA_ArcClass_Update             0x15f04
#define RVA_ArcClass_Render             0x16650
#define RVA_Camera_Update               0x17acc
#define RVA_CreateTextureFromTGA        0x243e8
#define RVA_Noise                       0x24824
#define RVA_Matrix44f_Identity          0x24bd8
#define RVA_TargaReader_ctor            0x24c14
#define RVA_TargaReader_GetRGBA         0x24d40
#define RVA_Image_IsPNG                 0x24ed8
#define RVA_Image_GetWidthAndHeightPNG  0x24f58
#define RVA_Image_GetImageType          0x2500c
#define RVA_Image_GetWidthAndHeight     0x25084

typedef float   ( *PFN_Noise )( const Point3f* );
typedef void    ( *PFN_Matrix44f_Identity )( Matrix44f* );
typedef void*   ( *PFN_TargaReader_ctor )( void*, const BYTE* );
typedef bool    ( *PFN_TargaReader_GetRGBA )( void*, BYTE*, BYTE*, BYTE*, BYTE* );
typedef bool    ( *PFN_Image_IsPNG )( const BYTE* );
typedef bool    ( *PFN_Image_GetWH )( const BYTE*, UINT*, UINT* );
typedef int     ( *PFN_Image_GetImageType )( const BYTE* );
typedef HRESULT ( *PFN_CreateTextureFromTGA )( void*, const BYTE*, UINT, UINT, DWORD, D3DFORMAT, void** );
typedef void    ( *PFN_Camera_Update )( Camera* );
typedef void*   ( *PFN_ArcClass_ctor )( void* );
typedef void    ( *PFN_ArcClass_InitDevice )( void*, void*, int, float );
typedef void    ( *PFN_ArcClass_Update )( void*, Camera*, float, float, float );
typedef void    ( *PFN_ArcClass_Render )( void*, void*, int );

static float Rand( float lo, float hi )
{
    return lo + ( hi - lo ) * ( (float)rand() / 32768.0f );
}

int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        printf( "usage: test_aurora <original Aurora.scr>\n" );
        return 2;
    }

    OrigImage orig;
    if( !orig.Load( argv[1], AURORA_TIMESTAMP ) )
        return 2;

    char msg[256];

    //-------------------------------------------------------------------------
    // Noise()
    //-------------------------------------------------------------------------
    {
        PFN_Noise pfnNoise = orig.At<PFN_Noise>( RVA_Noise );
        const int N = 200000;
        int nDifferent = 0;
        float fMaxDiff = 0.0f;
        srand( 1 );
        for( int i = 0; i < N; i++ )
        {
            Point3f p( Rand( -300.0f, 300.0f ), Rand( -300.0f, 300.0f ), Rand( -300.0f, 300.0f ) );
            if( i % 16 == 0 )
            {
                // exact lattice points and negative integers
                p.x = (float)(int)p.x;
                p.y = (float)(int)p.y;
            }
            float a = pfnNoise( &p );
            float b = Noise( p );
            if( memcmp( &a, &b, 4 ) != 0 )
                nDifferent++;
            if( fabsf( a - b ) > fMaxDiff )
                fMaxDiff = fabsf( a - b );
        }
        sprintf( msg, "Noise(): %d points, %d not bit-identical, max difference %g", N, nDifferent, fMaxDiff );
        Check( fMaxDiff < 1e-5f, msg );
    }

    //-------------------------------------------------------------------------
    // Matrix44f::Identity()
    //-------------------------------------------------------------------------
    {
        Matrix44f a, b;
        memset( &a, 0x55, sizeof(a) );
        memset( &b, 0x55, sizeof(b) );
        orig.At<PFN_Matrix44f_Identity>( RVA_Matrix44f_Identity )( &a );
        b.Identity();
        Check( memcmp( &a, &b, sizeof(a) ) == 0, "Matrix44f::Identity()" );
    }

    //-------------------------------------------------------------------------
    // The arc texture, straight out of the original's resources
    //-------------------------------------------------------------------------
    const BYTE* pTga = NULL;
    DWORD cbTga = 0;
    {
        HMODULE hData = LoadLibraryExA( argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE );
        HRSRC hRes = hData ? FindResourceW( hData, MAKEINTRESOURCEW( 102 ), L"BMP" ) : NULL;
        if( hRes != NULL )
        {
            cbTga = SizeofResource( hData, hRes );
            pTga = (const BYTE*)LockResource( LoadResource( hData, hRes ) );
        }
        Check( pTga != NULL && cbTga > 18, "found resource BMP/102 in the original" );
        if( pTga == NULL )
            return Summary( "test_aurora" );
    }

    //-------------------------------------------------------------------------
    // Image::*
    //-------------------------------------------------------------------------
    {
        static const BYTE png[32] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R',
                                      0, 0, 0x02, 0x80, 0, 0, 0x01, 0xe0, 8, 6, 0, 0, 0, 0, 0, 0 };
        static const BYTE bmp[32] = { 'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
                                      0x20, 0x03, 0, 0, 0x58, 0x02, 0, 0, 1, 0, 24, 0, 0, 0 };
        static const BYTE junk[32] = { 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 };
        const BYTE* samples[4] = { png, bmp, junk, pTga };
        const char* names[4] = { "png", "bmp", "junk", "tga" };
        bool bAllOk = true;
        for( int i = 0; i < 4; i++ )
        {
            UINT w1 = 0, h1 = 0, w2 = 0, h2 = 0;
            int  t1 = orig.At<PFN_Image_GetImageType>( RVA_Image_GetImageType )( samples[i] );
            int  t2 = (int)Image::GetImageType( samples[i] );
            bool r1 = orig.At<PFN_Image_GetWH>( RVA_Image_GetWidthAndHeight )( samples[i], &w1, &h1 );
            bool r2 = Image::GetWidthAndHeight( samples[i], w2, h2 );
            bool p1 = orig.At<PFN_Image_IsPNG>( RVA_Image_IsPNG )( samples[i] );
            bool p2 = Image::IsPNG( samples[i] );
            UINT pw1 = 0, ph1 = 0, pw2 = 0, ph2 = 0;
            bool q1 = orig.At<PFN_Image_GetWH>( RVA_Image_GetWidthAndHeightPNG )( samples[i], &pw1, &ph1 );
            bool q2 = Image::GetWidthAndHeightPNG( samples[i], pw2, ph2 );
            bool bOk = ( t1 == t2 && r1 == r2 && w1 == w2 && h1 == h2 && p1 == p2 &&
                         q1 == q2 && pw1 == pw2 && ph1 == ph2 );
            if( !bOk )
                printf( "    %s: type %d/%d size %ux%u / %ux%u\n", names[i], t1, t2, w1, h1, w2, h2 );
            bAllOk = bAllOk && bOk;
        }
        Check( bAllOk, "Image::GetImageType / GetWidthAndHeight / IsPNG / GetWidthAndHeightPNG" );
    }

    //-------------------------------------------------------------------------
    // Targa::TargaReader
    //-------------------------------------------------------------------------
    {
        BYTE rawOrig[0x48];
        BYTE rawMine[0x48];
        memset( rawOrig, 0, sizeof(rawOrig) );
        memset( rawMine, 0, sizeof(rawMine) );
        orig.At<PFN_TargaReader_ctor>( RVA_TargaReader_ctor )( rawOrig, pTga );
        Targa::TargaReader& mine = *new( rawMine ) Targa::TargaReader( pTga );
        Check( memcmp( rawOrig, rawMine, 0x48 ) == 0, "TargaReader::TargaReader(): identical object state" );

        UINT w = pTga[12] + pTga[13] * 256;
        UINT h = pTga[14] + pTga[15] * 256;
        size_t nPixels = (size_t)w * h;
        size_t nBad = 0;
        PFN_TargaReader_GetRGBA pfnGet = orig.At<PFN_TargaReader_GetRGBA>( RVA_TargaReader_GetRGBA );
        for( size_t i = 0; i < nPixels; i++ )
        {
            BYTE a[4] = { 1, 2, 3, 4 }, b[4] = { 5, 6, 7, 8 };
            bool r1 = pfnGet( rawOrig, &a[0], &a[1], &a[2], &a[3] );
            bool r2 = mine.GetRGBA( b[0], b[1], b[2], b[3] );
            if( r1 != r2 || memcmp( a, b, 4 ) != 0 )
                nBad++;
        }
        sprintf( msg, "TargaReader::GetRGBA(): %ux%u pixels decoded, %u mismatches", w, h, (unsigned)nBad );
        Check( nBad == 0 && memcmp( rawOrig, rawMine, 0x48 ) == 0, msg );
    }

    //-------------------------------------------------------------------------
    // Texture::CreateTextureFromTGAFileInMemory()
    //-------------------------------------------------------------------------
    {
        CallLog logOrig, logMine;
        MockObject* pDevOrig = MockCreateDevice( &logOrig );
        MockObject* pDevMine = MockCreateDevice( &logMine );
        void* pTexOrig = NULL;
        IDirect3DTexture9* pTexMine = NULL;
        HRESULT hr1 = orig.At<PFN_CreateTextureFromTGA>( RVA_CreateTextureFromTGA )(
                          pDevOrig, pTga, cbTga, 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, &pTexOrig );
        HRESULT hr2 = Texture::CreateTextureFromTGAFileInMemory(
                          (IDirect3DDevice9*)pDevMine, pTga, cbTga, 0, D3DUSAGE_AUTOGENMIPMAP,
                          D3DFMT_A8R8G8B8, &pTexMine );
        sprintf( msg, "CreateTextureFromTGAFileInMemory(): %u Direct3D calls, same calls and same pixels",
                 (unsigned)logOrig.records.size() );
        Check( hr1 == hr2 && CallLog::Equal( logOrig, logMine ), msg );

        // ... and for something that is not a Targa file
        static const BYTE junk[64] = { 9, 9, 9 };
        hr1 = orig.At<PFN_CreateTextureFromTGA>( RVA_CreateTextureFromTGA )(
                  pDevOrig, junk, sizeof(junk), 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, &pTexOrig );
        hr2 = Texture::CreateTextureFromTGAFileInMemory(
                  (IDirect3DDevice9*)pDevMine, junk, sizeof(junk), 0, D3DUSAGE_AUTOGENMIPMAP,
                  D3DFMT_A8R8G8B8, &pTexMine );
        Check( hr1 == hr2 && hr1 == E_INVALIDARG, "CreateTextureFromTGAFileInMemory(): rejects non-Targa data" );
    }

    //-------------------------------------------------------------------------
    // Camera::Update()
    //-------------------------------------------------------------------------
    Camera camOrig, camMine;
    {
        const int N = 500;
        static float resOrig[N][6], resMine[N][6];
        PFN_Camera_Update pfnUpdate = orig.At<PFN_Camera_Update>( RVA_Camera_Update );

        srand( 7 );
        for( int i = 0; i < N; i++ )
        {
            pfnUpdate( &camOrig );
            memcpy( &resOrig[i][0], &camOrig.m_eye, 12 );
            memcpy( &resOrig[i][3], &camOrig.m_direction, 12 );
        }
        srand( 7 );
        for( int i = 0; i < N; i++ )
        {
            camMine.Update();
            memcpy( &resMine[i][0], &camMine.m_eye, 12 );
            memcpy( &resMine[i][3], &camMine.m_direction, 12 );
        }
        size_t cDifferent;
        float fMax = MaxFloatDiff( &resOrig[0][0], &resMine[0][0], N * 6, &cDifferent );
        sprintf( msg, "Camera::Update(): %d frames, %u of %u floats not bit-identical, max difference %g",
                 N, (unsigned)cDifferent, N * 6, fMax );
        Check( fMax < 1e-5f, msg );
    }

    //-------------------------------------------------------------------------
    // ArcClass
    //-------------------------------------------------------------------------
    {
        static BYTE rawOrig[sizeof(ArcClass)];
        static BYTE rawMine[sizeof(ArcClass)];
        memset( rawOrig, 0, sizeof(rawOrig) );
        memset( rawMine, 0, sizeof(rawMine) );

        srand( 11 );
        orig.At<PFN_ArcClass_ctor>( RVA_ArcClass_ctor )( rawOrig );
        srand( 11 );
        ArcClass* pMine = new( rawMine ) ArcClass;
        Check( memcmp( rawOrig, rawMine, sizeof(ArcClass) ) == 0, "ArcClass::ArcClass(): identical object state" );

        CallLog logOrig, logMine;
        MockObject* pDevOrig = MockCreateDevice( &logOrig );
        MockObject* pDevMine = MockCreateDevice( &logMine );

        srand( 12 );
        orig.At<PFN_ArcClass_InitDevice>( RVA_ArcClass_InitDevice )( rawOrig, pDevOrig, 3, 0.075f );
        srand( 12 );
        pMine->InitDevice( (IDirect3DDevice9*)pDevMine, 3, 0.075f );

        ArcClass* pOrig = (ArcClass*)rawOrig;
        bool bSameVerts = memcmp( pOrig->m_Vertices, pMine->m_Vertices, sizeof(pMine->m_Vertices) ) == 0 &&
                          memcmp( pOrig->m_BaseVertices, pMine->m_BaseVertices, sizeof(pMine->m_BaseVertices) ) == 0;
        Check( bSameVerts && CallLog::Equal( logOrig, logMine ),
               "ArcClass::InitDevice(): same Direct3D calls, identical vertices and colour" );

        // Animate both for a while with the same camera
        const int nFrames = 300;
        size_t cDifferent = 0;
        float fMaxDiff = 0.0f;
        size_t nAlphaDiff = 0;
        int nMaxAlphaDelta = 0;
        srand( 13 );
        Camera cam;
        for( int f = 0; f < nFrames; f++ )
        {
            cam.Update();
            Camera c1 = cam, c2 = cam;
            unsigned int seed = 1000 + f;
            srand( seed );
            orig.At<PFN_ArcClass_Update>( RVA_ArcClass_Update )( rawOrig, &c1, 0.0002f * ( 1 + f % 7 ), 0.075f, 2.0f );
            srand( seed );
            pMine->Update( c2, 0.0002f * ( 1 + f % 7 ), 0.075f, 2.0f );

            for( int v = 0; v < ARC_NUM_VERTICES; v++ )
            {
                size_t c;
                float d = MaxFloatDiff( &pOrig->m_Vertices[v].pos.x, &pMine->m_Vertices[v].pos.x, 3, &c );
                cDifferent += c;
                if( d > fMaxDiff )
                    fMaxDiff = d;
                int a1 = (int)( pOrig->m_Vertices[v].color >> 24 );
                int a2 = (int)( pMine->m_Vertices[v].color >> 24 );
                if( a1 != a2 )
                {
                    nAlphaDiff++;
                    if( abs( a1 - a2 ) > nMaxAlphaDelta )
                        nMaxAlphaDelta = abs( a1 - a2 );
                }
            }
            // keep the two in lock-step so that rounding differences cannot accumulate
            memcpy( rawMine, rawOrig, sizeof(ArcClass) );
            pMine->m_pVB[3] = (IDirect3DVertexBuffer9*)pDevMine->created[0];
        }
        sprintf( msg, "ArcClass::Update(): %d frames, %u position floats not bit-identical, max difference %g; "
                      "%u alpha bytes differ (max by %d)",
                 nFrames, (unsigned)cDifferent, fMaxDiff, (unsigned)nAlphaDiff, nMaxAlphaDelta );
        Check( fMaxDiff < 1e-4f && nMaxAlphaDelta <= 1, msg );

        // Push the noise position out of bounds to exercise the bounce code
        {
            bool bSame = true;
            for( int k = 0; k < 6; k++ )
            {
                ArcClass* arcs[2] = { pOrig, pMine };
                for( int s = 0; s < 2; s++ )
                {
                    arcs[s]->m_noisePos = Point3f( k & 1 ? 25.0f : -25.0f, k & 2 ? 25.0f : -25.0f, k & 4 ? 25.0f : -25.0f );
                    arcs[s]->m_noiseVelocity = Point3f( k & 1 ? 0.5f : -0.5f, k & 2 ? -0.5f : 0.5f, k & 4 ? 0.5f : -0.5f );
                    arcs[s]->m_fMinNoiseSpeed = 0.25f;
                    arcs[s]->m_fMaxNoiseSpeed = 0.75f;
                }
                Camera c1 = cam, c2 = cam;
                srand( 77 + k );
                orig.At<PFN_ArcClass_Update>( RVA_ArcClass_Update )( rawOrig, &c1, 0.001f, 0.075f, 2.0f );
                srand( 77 + k );
                pMine->Update( c2, 0.001f, 0.075f, 2.0f );
                if( memcmp( &pOrig->m_noiseVelocity, &pMine->m_noiseVelocity, 12 ) != 0 ||
                    memcmp( &pOrig->m_noisePos, &pMine->m_noisePos, 12 ) != 0 )
                    bSame = false;
            }
            Check( bSame, "ArcClass::Update(): noise position bounces off its bounds identically" );
        }

        // Render
        logOrig.Clear();
        logMine.Clear();
        memcpy( rawMine, rawOrig, sizeof(ArcClass) );
        pMine->m_pVB[3] = (IDirect3DVertexBuffer9*)pDevMine->created[0];
        orig.At<PFN_ArcClass_Render>( RVA_ArcClass_Render )( rawOrig, pDevOrig, 3 );
        pMine->Render( (IDirect3DDevice9*)pDevMine, 3 );
        sprintf( msg, "ArcClass::Render(): %u Direct3D calls, identical sequence and arguments",
                 (unsigned)logOrig.records.size() );
        Check( CallLog::Equal( logOrig, logMine ) &&
               memcmp( &pDevOrig->created[0]->buffer[0], &pDevMine->created[0]->buffer[0], sizeof(pMine->m_Vertices) ) == 0,
               msg );
    }

    return Summary( "test_aurora" );
}
