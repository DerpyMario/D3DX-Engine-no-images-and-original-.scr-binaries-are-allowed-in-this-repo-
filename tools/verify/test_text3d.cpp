//-----------------------------------------------------------------------------
// File: test_text3d.cpp
//
// Desc: Differential test of the reconstructed 3D Text screen saver
//       (text3d.cpp + Shape3D.lib) against the original ssText3d.scr.
//
//           test_text3d <path to the original ssText3d.scr>
//
//       The addresses below belong to ssText3d.scr 6.0.6001.18000 (amd64),
//       link time stamp 0x47919797.
//
//       Checked:
//         * the layout of CTextScreensaver
//         * UpdateAngles()        - bit-identical angles for every rotation style
//         * FrameMove()           - the bouncing rectangle, frame by frame
//         * Render()              - identical Direct3D call sequence (mock device)
//         * ReadSettings()        - registry and legacy control.ini settings
//         * the text mesh         - the original worker (D3DXCreateTextAW) and
//                                   Shape3D::CreateTextW() must produce the
//                                   same vertices, indices and glyph metrics
//         * BuildTextMesh()       - mesh and text extent
//
//       The mesh tests need a Direct3D 9 device and a few TrueType fonts.
//       The test executable must contain the string resources of text3d.rc
//       (the build links them in) and, to get past the delay-loaded branding
//       DLL on systems that lack it, a winbrand.dll next to it.
//-----------------------------------------------------------------------------
#define STRSAFE_NO_DEPRECATE        // the test formats its messages with sprintf
#include <new>
#include "origimage.h"
#include "mockd3d.h"
#include <usp10.h>
#include <strsafe.h>
#include "dxutil9.h"

// the test has to look into the classes
#define protected public
#define private public
#include "d3dsaver9.h"
#include "text3d.h"
#undef protected
#undef private

#include "shape3d.h"
#include "resource.h"

#define TEXT3D_TIMESTAMP            0x47919797

#define RVA_ctor                    0x16258
#define RVA_FrameMove               0x164e4
#define RVA_Render                  0x16808
#define RVA_BuildTextMesh           0x176e4
#define RVA_UpdateAngles            0x17b78
#define RVA_D3DXCreateTextAW        0x296cc

#define VTBL_ReadSettings           3

typedef void*   ( *PFN_ctor )( void* );
typedef HRESULT ( *PFN_this_hr )( void* );
typedef void    ( *PFN_this_void )( void* );
typedef void    ( *PFN_UpdateAngles )( void*, DWORD );
typedef HRESULT ( *PFN_BuildTextMesh )( void*, const WCHAR* );
typedef HRESULT ( *PFN_D3DXCreateTextAW )( IDirect3DDevice9*, HDC, const WCHAR*, FLOAT, FLOAT, ID3DXMesh**,
                                           ID3DXBuffer**, GLYPHMETRICSFLOAT*, BOOL );

static const WCHAR* const g_pszRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Screensavers\\ssText3D";


//-----------------------------------------------------------------------------
// Name: Offset()
//-----------------------------------------------------------------------------
template <class T> static size_t Offset( CTextScreensaver* p, T* pMember )
{
    return (size_t)( (BYTE*)pMember - (BYTE*)p );
}


//-----------------------------------------------------------------------------
// Name: TestLayout()
//-----------------------------------------------------------------------------
static void TestLayout( CTextScreensaver* p )
{
    bool bOk =
        Offset( p, &p->m_bWindowed ) == 0x30 &&
        Offset( p, &p->m_Monitors ) == 0x358 &&
        Offset( p, &p->m_Monitors[0].rcScreen ) == 0x560 &&
        Offset( p, &p->m_RenderUnits[0].iMonitor ) == 0x17a4 &&
        Offset( p, &p->m_pd3dDevice ) == 0x2460 &&
        Offset( p, &p->m_rcRenderTotal ) == 0x2468 &&
        Offset( p, &p->m_rcRenderCurDevice ) == 0x2478 &&
        Offset( p, &p->m_strRegPath ) == 0x263c &&
        Offset( p, &p->m_DeviceObjectsArray ) == 0x2938 &&
        Offset( p, &p->m_pDeviceObjects ) == 0x2a28 &&
        Offset( p, &p->m_floatrect ) == 0x2a30 &&
        Offset( p, &p->m_hFont ) == 0x2a48 &&
        Offset( p, &p->m_fTextMinX ) == 0x2a50 &&
        Offset( p, &p->m_fTextOffsetX ) == 0x2a60 &&
        Offset( p, &p->m_fAngleX ) == 0x2a68 &&
        Offset( p, &p->m_dwLastTick ) == 0x2a74 &&
        Offset( p, &p->m_dwMeshUpdateCounter ) == 0x2a78 &&
        Offset( p, &p->m_szDisplayString ) == 0x2a7c &&
        Offset( p, &p->m_Font ) == 0x2aa8 &&
        Offset( p, &p->m_bUseTime ) == 0x2b04 &&
        Offset( p, &p->m_SurfType ) == 0x2b08 &&
        Offset( p, &p->m_bSpecular ) == 0x2b0c &&
        Offset( p, &p->m_dwRotationSpeed ) == 0x2b10 &&
        Offset( p, &p->m_dwSize ) == 0x2b14 &&
        Offset( p, &p->m_RotType ) == 0x2b18 &&
        Offset( p, &p->m_SurfaceColor ) == 0x2b1c &&
        Offset( p, &p->m_bUseCustomColor ) == 0x2b20 &&
        Offset( p, &p->m_bUseCustomTexture ) == 0x2b24 &&
        Offset( p, &p->m_bUseCustomEnvironment ) == 0x2b28 &&
        Offset( p, &p->m_szCustomTexture ) == 0x2b2c &&
        Offset( p, &p->m_szCustomEnvironment ) == 0x2d34 &&
        Offset( p, &p->m_dwMeshQuality ) == 0x2f3c &&
        Offset( p, &p->m_matWorld ) == 0x2f40 &&
        Offset( p, &p->m_matView ) == 0x2f80 &&
        sizeof(CTextScreensaver) == 0x2fc0;

    Check( bOk, "CTextScreensaver layout" );
}


//-----------------------------------------------------------------------------
// Name: TestUpdateAngles()
//-----------------------------------------------------------------------------
static void TestUpdateAngles( OrigImage& orig, CTextScreensaver* pOrig, CTextScreensaver* pMine )
{
    PFN_UpdateAngles pfn = orig.At<PFN_UpdateAngles>( RVA_UpdateAngles );
    const int N = 40000;
    int nDifferent = 0;
    unsigned int seed = 99;

    for( int i = 0; i < N; i++ )
    {
        seed = seed * 1103515245u + 12345u;
        DWORD dwElapsed = ( seed >> 16 ) % 120;
        seed = seed * 1103515245u + 12345u;
        DWORD dwSpeed = 1 + ( seed >> 16 ) % 20;

        // change the style every 500 calls, going through all of them (and
        // an invalid one) several times
        RotType rotType = (RotType)( ( i / 500 ) % 6 );

        pOrig->m_RotType = rotType;
        pOrig->m_dwRotationSpeed = dwSpeed;
        pMine->m_RotType = rotType;
        pMine->m_dwRotationSpeed = dwSpeed;

        pfn( pOrig, dwElapsed );
        pMine->UpdateAngles( dwElapsed );

        if( memcmp( &pOrig->m_fAngleX, &pMine->m_fAngleX, 3 * sizeof(FLOAT) ) != 0 )
        {
            if( nDifferent++ == 0 )
                printf( "    first difference at call %d (style %d): %g %g %g / %g %g %g\n", i, (int)rotType,
                        pOrig->m_fAngleX, pOrig->m_fAngleY, pOrig->m_fAngleZ,
                        pMine->m_fAngleX, pMine->m_fAngleY, pMine->m_fAngleZ );
        }
    }

    char msg[128];
    sprintf( msg, "UpdateAngles: %d calls, %d different", N, nDifferent );
    Check( nDifferent == 0, msg );
}


//-----------------------------------------------------------------------------
// Name: TestFrameMove()
//-----------------------------------------------------------------------------
static void TestFrameMove( OrigImage& orig, CTextScreensaver* pOrig, CTextScreensaver* pMine,
                           BOOL bWindowed, DWORD dwSize, RotType rotType, unsigned int seed )
{
    PFN_this_hr pfn = orig.At<PFN_this_hr>( RVA_FrameMove );
    const int N = 6000;
    CTextScreensaver* p[2] = { pOrig, pMine };
    FLOATRECT* pTrace[2];

    for( int k = 0; k < 2; k++ )
    {
        SetRect( &p[k]->m_rcRenderTotal, -1280, 0, 1920, 1200 );
        p[k]->m_bWindowed = bWindowed ? true : false;
        p[k]->m_RenderUnits[0].iMonitor = 1;
        SetRect( &p[k]->m_Monitors[1].rcScreen, 0, 0, 1920, 1200 );
        p[k]->m_dwSize = dwSize;
        p[k]->m_RotType = rotType;
        p[k]->m_dwRotationSpeed = 0;        // keeps the angles independent of the clock
        p[k]->m_bUseTime = FALSE;
        memset( &p[k]->m_floatrect, 0, sizeof(FLOATRECT) );

        pTrace[k] = new FLOATRECT[N];

        srand( seed );
        for( int i = 0; i < N; i++ )
        {
            if( k == 0 )
                pfn( pOrig );
            else
                pMine->FrameMove();
            pTrace[k][i] = p[k]->m_floatrect;
        }
    }

    int nFirstDiff = -1;
    int nBounces = 0;
    for( int i = 0; i < N; i++ )
    {
        if( nFirstDiff < 0 && memcmp( &pTrace[0][i], &pTrace[1][i], sizeof(FLOATRECT) ) != 0 )
            nFirstDiff = i;
        if( i > 0 && ( pTrace[0][i].xVel != pTrace[0][i - 1].xVel || pTrace[0][i].yVel != pTrace[0][i - 1].yVel ) )
            nBounces++;
    }

    char msg[200];
    sprintf( msg, "FrameMove (%s, size %u, style %d): %d frames, %d bounces, rect %g x %g, first difference %d",
             bWindowed ? "windowed" : "full screen", (unsigned)dwSize, (int)rotType, N, nBounces,
             pTrace[0][0].xSize, pTrace[0][0].ySize, nFirstDiff );
    Check( nFirstDiff < 0 && pTrace[0][0].xSize > 0.0f &&
           memcmp( &pOrig->m_fAngleX, &pMine->m_fAngleX, 3 * sizeof(FLOAT) ) == 0, msg );

    delete[] pTrace[0];
    delete[] pTrace[1];
}


//-----------------------------------------------------------------------------
// Name: TestRender()
// Desc: Render() and SetPerFrameStates() against recording mock objects.
//-----------------------------------------------------------------------------
static void TestRender( OrigImage& orig, CTextScreensaver* pOrig, CTextScreensaver* pMine,
                        SurfType surfType, BOOL bSpecular, BOOL bCustomColor, int iGeometry )
{
    PFN_this_hr pfn = orig.At<PFN_this_hr>( RVA_Render );
    CallLog log[2];
    CTextScreensaver* p[2] = { pOrig, pMine };
    HRESULT hr[2] = { E_FAIL, E_FAIL };

    for( int k = 0; k < 2; k++ )
    {
        MockObject* pDevice = MockCreateDevice( &log[k] );
        MockObject* pMesh = MockCreate( "ID3DXMesh", MOCK_SLOTS( ID3DXMesh ), &log[k], NULL );
        MockObject* pTexture = ( surfType == color ) ? NULL : MockCreateTexture( &log[k], 64, 64 );

        p[k]->m_pd3dDevice = (IDirect3DDevice9*)pDevice;
        p[k]->m_pDeviceObjects = &p[k]->m_DeviceObjectsArray[3];
        p[k]->m_pDeviceObjects->m_pMesh = (ID3DXMesh*)pMesh;
        p[k]->m_pDeviceObjects->m_pTexture = (IDirect3DTexture9*)pTexture;
        p[k]->m_pDeviceObjects->m_dwMeshUpdateCounter = 7;
        p[k]->m_dwMeshUpdateCounter = 7;

        p[k]->m_SurfType = surfType;
        p[k]->m_bSpecular = bSpecular;
        p[k]->m_bUseCustomColor = bCustomColor;
        p[k]->m_SurfaceColor = RGB( 200, 40, 90 );

        switch( iGeometry )
        {
            case 0:     // the rectangle is completely on this device
                SetRect( &p[k]->m_rcRenderCurDevice, 0, 0, 1280, 720 );
                p[k]->m_floatrect.xMin = 300.0f;
                p[k]->m_floatrect.yMin = 100.5f;
                p[k]->m_floatrect.xSize = 500.0f;
                p[k]->m_floatrect.ySize = 500.0f;
                break;

            case 1:     // it straddles two monitors
                SetRect( &p[k]->m_rcRenderCurDevice, -1024, 0, 0, 768 );
                p[k]->m_floatrect.xMin = -200.25f;
                p[k]->m_floatrect.yMin = 500.0f;
                p[k]->m_floatrect.xSize = 576.0f;
                p[k]->m_floatrect.ySize = 576.0f;
                break;

            default:    // it is on another monitor
                SetRect( &p[k]->m_rcRenderCurDevice, 1280, 0, 2560, 1024 );
                p[k]->m_floatrect.xMin = 100.0f;
                p[k]->m_floatrect.yMin = 100.0f;
                p[k]->m_floatrect.xSize = 400.0f;
                p[k]->m_floatrect.ySize = 400.0f;
                break;
        }

        p[k]->m_fTextMinX = 0.031f;
        p[k]->m_fTextMaxX = 5.72f;
        p[k]->m_fTextMinY = -0.21f;
        p[k]->m_fTextMaxY = 0.74f;
        p[k]->m_fTextOffsetX = -2.8755f;
        p[k]->m_fTextOffsetY = -0.265f;
        p[k]->m_fAngleX = 0.3f + iGeometry;
        p[k]->m_fAngleY = 1.1f;
        p[k]->m_fAngleZ = 5.9f;

        D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f ), vAt( 0.0f, 0.0f, 1.0f ), vEye( 0.0f, 0.0f, 0.0f );
        D3DXMatrixLookAtLH( &p[k]->m_matView, &vEye, &vAt, &vUp );

        if( k == 0 )
            hr[k] = pfn( pOrig );
        else
            hr[k] = pMine->Render();

        // The original leaves the alpha of the diffuse (and with it of the
        // ambient) colour of the solid colour material uninitialized.
        if( surfType == color )
        {
            for( size_t i = 0; i < log[k].records.size(); i++ )
            {
                CallRecord& r = log[k].records[i];
                if( strcmp( r.pszMethod, "SetMaterial" ) == 0 && r.payload.size() == sizeof(D3DMATERIAL9) )
                {
                    memset( &r.payload[offsetof( D3DMATERIAL9, Diffuse.a )], 0, sizeof(FLOAT) );
                    memset( &r.payload[offsetof( D3DMATERIAL9, Ambient.a )], 0, sizeof(FLOAT) );
                }
            }
        }
    }

    // (the matrices come from two different builds of D3DX, hence the tolerance)
    float fWorldDiff = MaxFloatDiff( (float*)&pOrig->m_matWorld, (float*)&pMine->m_matWorld, 16, NULL );

    char msg[200];
    sprintf( msg, "Render (surface %d, specular %d, custom colour %d, geometry %d): %u calls, world matrix differs by %g",
             (int)surfType, bSpecular, bCustomColor, iGeometry, (unsigned)log[0].records.size(), fWorldDiff );
    Check( hr[0] == hr[1] && CallLog::Equal( log[0], log[1], 2e-5f ) &&
           log[0].records.size() == ( iGeometry == 2 ? 2u : 34u ) &&
           ( iGeometry == 2 || fWorldDiff <= 2e-5f ), msg );
}


//-----------------------------------------------------------------------------
// Name: TestReadSettings()
//-----------------------------------------------------------------------------
static void SetRegDword( HKEY hkey, const WCHAR* pszName, DWORD dwValue )
{
    RegSetValueExW( hkey, pszName, 0, REG_DWORD, (BYTE*)&dwValue, sizeof(dwValue) );
}

static void SetRegString( HKEY hkey, const WCHAR* pszName, const WCHAR* pszValue )
{
    RegSetValueExW( hkey, pszName, 0, REG_SZ, (BYTE*)pszValue, ( lstrlenW( pszValue ) + 1 ) * sizeof(WCHAR) );
}

static void TestReadSettings( CTextScreensaver* pOrig, CTextScreensaver* pMine, int iScenario )
{
    static const WCHAR* const pszSection = L"Screen Saver.3DText";
    static const WCHAR* const pszIni = L"control.ini";
    HKEY hkey;

    // start from nothing
    RegDeleteKeyW( HKEY_CURRENT_USER, g_pszRegPath );
    WritePrivateProfileStringW( pszSection, NULL, NULL, pszIni );

    if( iScenario == 1 || iScenario == 3 || iScenario == 4 )
    {
        // settings of the OpenGL screen saver
        WritePrivateProfileStringW( pszSection, L"DemoType", ( iScenario == 4 ) ? L"0" : L"1", pszIni );
        WritePrivateProfileStringW( pszSection, L"RotStyle", ( iScenario == 4 ) ? L"3" : L"1", pszIni );
        WritePrivateProfileStringW( pszSection, L"Tesselation", ( iScenario == 4 ) ? L"250" : L"37", pszIni );
        WritePrivateProfileStringW( pszSection, L"Size", L"45", pszIni );
        WritePrivateProfileStringW( pszSection, L"Speed", L"80", pszIni );
        WritePrivateProfileStringW( pszSection, L"Font", L"Courier New", pszIni );
        WritePrivateProfileStringW( pszSection, L"FontAttributes", ( iScenario == 4 ) ? L"1" : L"2", pszIni );
        WritePrivateProfileStringW( pszSection, L"CharSet", L"161", pszIni );
        WritePrivateProfileStringW( pszSection, L"Text", L"OpenGL text", pszIni );
        WritePrivateProfileStringW( pszSection, L"SurfStyle", ( iScenario == 4 ) ? L"0" : L"1", pszIni );
        if( iScenario != 4 )
            WritePrivateProfileStringW( pszSection, L"Texture", L"C:\\old\\texture.bmp", pszIni );
    }

    if( iScenario == 2 || iScenario == 3 )
    {
        RegCreateKeyExW( HKEY_CURRENT_USER, g_pszRegPath, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL );
        SetRegString( hkey, L"DisplayString", L"A string that is far too long to fit" );
        SetRegDword( hkey, L"FontHeight", 48 );
        SetRegDword( hkey, L"FontWeight", 700 );
        SetRegDword( hkey, L"FontItalic", 1 );
        SetRegDword( hkey, L"FontCharSet", 0 );
        SetRegDword( hkey, L"FontPitchFamily", 34 );
        SetRegString( hkey, L"FontFace", L"Georgia" );
        SetRegDword( hkey, L"DisplayTime", 1 );
        SetRegDword( hkey, L"MeshQuality", 5000 );
        SetRegDword( hkey, L"SurfaceType", 2 );
        SetRegDword( hkey, L"Specular", 0 );
        SetRegDword( hkey, L"RotationSpeed", 17 );
        SetRegDword( hkey, L"Size", ( iScenario == 2 ) ? 0 : 50 );
        SetRegDword( hkey, L"RotationStyle", 4 );
        SetRegDword( hkey, L"SurfaceColor", 0x00123456 );
        SetRegDword( hkey, L"UseCustomColor", 1 );
        SetRegDword( hkey, L"UseCustomTexture", 1 );
        SetRegDword( hkey, L"UseCustomEnvironment", 1 );
        SetRegString( hkey, L"CustomTexture", L"C:\\pictures\\texture.jpg" );
        if( iScenario == 3 )
            SetRegString( hkey, L"CustomEnvironment", L"C:\\pictures\\reflection.png" );
        RegCloseKey( hkey );
    }

    // poison the settings, so that everything must be written by the code
    size_t ofs = Offset( pMine, &pMine->m_szDisplayString );
    size_t cb = Offset( pMine, &pMine->m_matWorld ) - ofs;
    memset( (BYTE*)pOrig + ofs, 0xEE, cb );
    memset( (BYTE*)pMine + ofs, 0xEE, cb );

    typedef void ( *PFN_ReadSettings )( void* );
    ( (PFN_ReadSettings)( *(void***)pOrig )[VTBL_ReadSettings] )( pOrig );
    pMine->ReadSettings();

    // Bytes the code never writes stay poisoned on both sides (the tails of
    // the strings, the padding)
    bool bSame = memcmp( (BYTE*)pOrig + ofs, (BYTE*)pMine + ofs, cb ) == 0;
    if( !bSame )
    {
        for( size_t i = 0; i < cb; i++ )
        {
            if( ( (BYTE*)pOrig )[ofs + i] != ( (BYTE*)pMine )[ofs + i] )
            {
                printf( "    first difference at offset 0x%x: %02x / %02x\n", (unsigned)( ofs + i ),
                        ( (BYTE*)pOrig )[ofs + i], ( (BYTE*)pMine )[ofs + i] );
                break;
            }
        }
    }

    char msg[200];
    sprintf( msg, "ReadSettings (scenario %d): \"%ls\", font \"%ls\" %ld/%ld, time %d, surface %d, style %d, speed %u, size %u, quality %u",
             iScenario, pMine->m_szDisplayString, pMine->m_Font.lfFaceName, pMine->m_Font.lfHeight, pMine->m_Font.lfWeight,
             pMine->m_bUseTime, (int)pMine->m_SurfType, (int)pMine->m_RotType, (unsigned)pMine->m_dwRotationSpeed,
             (unsigned)pMine->m_dwSize, (unsigned)pMine->m_dwMeshQuality );
    Check( bSame, msg );

    RegDeleteKeyW( HKEY_CURRENT_USER, g_pszRegPath );
    WritePrivateProfileStringW( pszSection, NULL, NULL, pszIni );
}


//-----------------------------------------------------------------------------
// Name: SameMesh()
//-----------------------------------------------------------------------------
static bool SameMesh( ID3DXMesh* pA, ID3DXMesh* pB, char* pszInfo )
{
    pszInfo[0] = '\0';

    if( pA == NULL || pB == NULL )
    {
        strcpy( pszInfo, ( pA == pB ) ? "no mesh" : "only one mesh" );
        return pA == pB;
    }

    DWORD cVertices = pA->GetNumVertices();
    DWORD cFaces = pA->GetNumFaces();

    sprintf( pszInfo, "%u vertices, %u faces", (unsigned)cVertices, (unsigned)cFaces );

    if( cVertices != pB->GetNumVertices() || cFaces != pB->GetNumFaces() ||
        pA->GetFVF() != pB->GetFVF() || pA->GetNumBytesPerVertex() != pB->GetNumBytesPerVertex() ||
        pA->GetOptions() != pB->GetOptions() )
    {
        sprintf( pszInfo + strlen( pszInfo ), " / %u vertices, %u faces", (unsigned)pB->GetNumVertices(),
                 (unsigned)pB->GetNumFaces() );
        return false;
    }

    bool  bSame = true;
    void* pDataA = NULL;
    void* pDataB = NULL;

    if( SUCCEEDED( pA->LockVertexBuffer( D3DLOCK_READONLY, &pDataA ) ) &&
        SUCCEEDED( pB->LockVertexBuffer( D3DLOCK_READONLY, &pDataB ) ) )
    {
        if( memcmp( pDataA, pDataB, (size_t)cVertices * pA->GetNumBytesPerVertex() ) != 0 )
        {
            size_t cDifferent;
            float d = MaxFloatDiff( (float*)pDataA, (float*)pDataB, (size_t)cVertices * 6, &cDifferent );
            sprintf( pszInfo + strlen( pszInfo ), ", %u vertex floats differ (max %g)", (unsigned)cDifferent, d );
            bSame = false;
        }
    }
    else
    {
        bSame = false;
    }
    pA->UnlockVertexBuffer();
    pB->UnlockVertexBuffer();

    if( SUCCEEDED( pA->LockIndexBuffer( D3DLOCK_READONLY, &pDataA ) ) &&
        SUCCEEDED( pB->LockIndexBuffer( D3DLOCK_READONLY, &pDataB ) ) )
    {
        if( memcmp( pDataA, pDataB, (size_t)cFaces * 3 * sizeof(WORD) ) != 0 )
        {
            strcat( pszInfo, ", indices differ" );
            bSame = false;
        }
    }
    else
    {
        bSame = false;
    }
    pA->UnlockIndexBuffer();
    pB->UnlockIndexBuffer();

    return bSame;
}


//-----------------------------------------------------------------------------
// Name: TestTextMesh()
// Desc: The Shape3D library against the worker of the original.
//-----------------------------------------------------------------------------
static void TestTextMesh( OrigImage& orig, IDirect3DDevice9* pDevice, const WCHAR* pszFace, LONG lWeight,
                          BYTE bItalic, const WCHAR* pszText, FLOAT fDeviation, FLOAT fExtrusion )
{
    PFN_D3DXCreateTextAW pfn = orig.At<PFN_D3DXCreateTextAW>( RVA_D3DXCreateTextAW );
    LOGFONTW lf;
    ZeroMemory( &lf, sizeof(lf) );
    lf.lfHeight = 100;
    lf.lfWeight = lWeight;
    lf.lfItalic = bItalic;
    lf.lfCharSet = DEFAULT_CHARSET;
    StringCchCopyW( lf.lfFaceName, LF_FACESIZE, pszFace );

    HDC   hdc = CreateCompatibleDC( NULL );
    HFONT hFont = CreateFontIndirectW( &lf );
    HFONT hFontOld = (HFONT)SelectObject( hdc, hFont );

    ID3DXMesh* pMeshA = NULL;
    ID3DXMesh* pMeshB = NULL;
    GLYPHMETRICSFLOAT gmA[64], gmB[64];
    memset( gmA, 0xEE, sizeof(gmA) );
    memset( gmB, 0xEE, sizeof(gmB) );

    HRESULT hrA = pfn( pDevice, hdc, pszText, fDeviation, fExtrusion, &pMeshA, NULL, gmA, TRUE );
    HFONT hFontAfterA = (HFONT)GetCurrentObject( hdc, OBJ_FONT );
    HRESULT hrB = Shape3D::CreateTextW( pDevice, hdc, pszText, fDeviation, fExtrusion, &pMeshB, NULL, gmB );
    HFONT hFontAfterB = (HFONT)GetCurrentObject( hdc, OBJ_FONT );

    char info[200];
    bool bSame = ( hrA == hrB ) && SameMesh( pMeshA, pMeshB, info ) &&
                 memcmp( gmA, gmB, sizeof(gmA) ) == 0 &&
                 hFontAfterA == hFont && hFontAfterB == hFont;       // both give the caller's font back

    char msg[400];
    sprintf( msg, "text mesh \"%ls\" (%ls%s%s, deviation %g, extrusion %g): hr 0x%08x/0x%08x, %s",
             pszText, pszFace, lWeight >= 700 ? " bold" : "", bItalic ? " italic" : "", fDeviation, fExtrusion,
             (unsigned)hrA, (unsigned)hrB, info );
    Check( bSame, msg );

    SAFE_RELEASE( pMeshA );
    SAFE_RELEASE( pMeshB );
    SelectObject( hdc, hFontOld );
    DeleteObject( hFont );
    DeleteDC( hdc );
}


//-----------------------------------------------------------------------------
// Name: TestBuildTextMesh()
//-----------------------------------------------------------------------------
static void TestBuildTextMesh( OrigImage& orig, CTextScreensaver* pOrig, CTextScreensaver* pMine,
                               IDirect3DDevice9* pDevice, const WCHAR* pszFace, const WCHAR* pszText,
                               DWORD dwMeshQuality )
{
    PFN_BuildTextMesh pfn = orig.At<PFN_BuildTextMesh>( RVA_BuildTextMesh );
    CTextScreensaver* p[2] = { pOrig, pMine };
    HRESULT hr[2];
    LOGFONTW lf;

    ZeroMemory( &lf, sizeof(lf) );
    lf.lfHeight = 100;
    lf.lfCharSet = DEFAULT_CHARSET;
    StringCchCopyW( lf.lfFaceName, LF_FACESIZE, pszFace );

    for( int k = 0; k < 2; k++ )
    {
        p[k]->m_pd3dDevice = pDevice;
        p[k]->m_pDeviceObjects = &p[k]->m_DeviceObjectsArray[0];
        p[k]->m_pDeviceObjects->m_pMesh = NULL;
        p[k]->m_hFont = CreateFontIndirectW( &lf );
        p[k]->m_dwMeshQuality = dwMeshQuality;
        p[k]->m_fTextMinX = p[k]->m_fTextMaxX = p[k]->m_fTextMinY = p[k]->m_fTextMaxY = 12345.0f;

        if( k == 0 )
            hr[k] = pfn( pOrig, pszText );
        else
            hr[k] = pMine->BuildTextMesh( pszText );
    }

    char info[200];
    bool bSame = hr[0] == hr[1] &&
                 SameMesh( pOrig->m_pDeviceObjects->m_pMesh, pMine->m_pDeviceObjects->m_pMesh, info ) &&
                 memcmp( &pOrig->m_fTextMinX, &pMine->m_fTextMinX, 4 * sizeof(FLOAT) ) == 0;

    char msg[400];
    sprintf( msg, "BuildTextMesh \"%ls\" (%ls, quality %u): hr 0x%08x/0x%08x, %s, extent x %g .. %g, y %g .. %g",
             pszText, pszFace, (unsigned)dwMeshQuality, (unsigned)hr[0], (unsigned)hr[1], info,
             pMine->m_fTextMinX, pMine->m_fTextMaxX, pMine->m_fTextMinY, pMine->m_fTextMaxY );
    Check( bSame && SUCCEEDED( hr[0] ), msg );

    for( int k = 0; k < 2; k++ )
    {
        SAFE_RELEASE( p[k]->m_pDeviceObjects->m_pMesh );
        DeleteObject( p[k]->m_hFont );
        p[k]->m_hFont = NULL;
        p[k]->m_pd3dDevice = NULL;
    }
}


//-----------------------------------------------------------------------------
// Name: CreateDevice()
//-----------------------------------------------------------------------------
static IDirect3DDevice9* CreateDevice( HWND hwnd )
{
    IDirect3D9* pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( pD3D == NULL )
        return NULL;

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory( &pp, sizeof(pp) );
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = 64;
    pp.BackBufferHeight = 64;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* pDevice = NULL;
    static const D3DDEVTYPE types[] = { D3DDEVTYPE_HAL, D3DDEVTYPE_REF, D3DDEVTYPE_NULLREF };

    for( size_t i = 0; i < sizeof(types) / sizeof(types[0]) && pDevice == NULL; i++ )
    {
        if( FAILED( pD3D->CreateDevice( D3DADAPTER_DEFAULT, types[i], hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                        &pp, &pDevice ) ) )
        {
            pDevice = NULL;
        }
    }

    pD3D->Release();
    return pDevice;
}


//-----------------------------------------------------------------------------
// Name: main()
//-----------------------------------------------------------------------------
int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        printf( "usage: test_text3d <original ssText3d.scr>\n" );
        return 2;
    }

    OrigImage orig;
    if( !orig.Load( argv[1], TEXT3D_TIMESTAMP ) )
        return 2;

    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );

    // The two screen saver objects: the original constructor builds one, the
    // rebuilt one the other.  Both are then looked at through the rebuilt
    // class declaration.
    const size_t cbSaver = 0x2fc0;
    BYTE* rawOrig = (BYTE*)_aligned_malloc( cbSaver, 16 );
    BYTE* rawMine = (BYTE*)_aligned_malloc( cbSaver, 16 );
    memset( rawOrig, 0, cbSaver );
    memset( rawMine, 0, cbSaver );

    orig.At<PFN_ctor>( RVA_ctor )( rawOrig );
    CTextScreensaver* pOrig = (CTextScreensaver*)rawOrig;
    CTextScreensaver* pMine = new( rawMine ) CTextScreensaver;

    TestLayout( pMine );

    Check( lstrcmpW( pOrig->m_strRegPath, pMine->m_strRegPath ) == 0 &&
           lstrcmpW( pOrig->m_strRegPath, g_pszRegPath ) == 0 &&
           lstrcmpW( pOrig->m_strWindowTitle, pMine->m_strWindowTitle ) == 0 &&
           pOrig->m_bUseDepthBuffer == pMine->m_bUseDepthBuffer &&
           pOrig->m_dwMinDepthBits == pMine->m_dwMinDepthBits &&
           pOrig->m_floatrect.xSize == 0.0f && pMine->m_floatrect.xSize == 0.0f,
           "CTextScreensaver constructor" );

    TestUpdateAngles( orig, pOrig, pMine );

    TestFrameMove( orig, pOrig, pMine, TRUE, 10, tumble, 1 );
    TestFrameMove( orig, pOrig, pMine, TRUE, 4, seesaw, 2 );
    TestFrameMove( orig, pOrig, pMine, FALSE, 7, wobble, 3 );
    TestFrameMove( orig, pOrig, pMine, FALSE, 1, none, 4 );
    TestFrameMove( orig, pOrig, pMine, FALSE, 5, spin, 5 );

    for( int iGeometry = 0; iGeometry < 3; iGeometry++ )
    {
        TestRender( orig, pOrig, pMine, environment, TRUE, FALSE, iGeometry );
        TestRender( orig, pOrig, pMine, texture, FALSE, FALSE, iGeometry );
        TestRender( orig, pOrig, pMine, color, TRUE, TRUE, iGeometry );
        TestRender( orig, pOrig, pMine, color, FALSE, FALSE, iGeometry );
    }
    for( int k = 0; k < 2; k++ )
    {
        CTextScreensaver* p = k ? pMine : pOrig;
        p->m_pd3dDevice = NULL;
        p->m_pDeviceObjects = NULL;
        memset( p->m_DeviceObjectsArray, 0, sizeof(p->m_DeviceObjectsArray) );
    }

    for( int iScenario = 0; iScenario < 5; iScenario++ )
        TestReadSettings( pOrig, pMine, iScenario );

    // Everything below needs a real device
    WNDCLASSW wc;
    ZeroMemory( &wc, sizeof(wc) );
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.lpszClassName = L"TestText3D";
    RegisterClassW( &wc );
    HWND hwnd = CreateWindowExW( 0, L"TestText3D", L"test", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200, NULL, NULL,
                                 wc.hInstance, NULL );

    IDirect3DDevice9* pDevice = CreateDevice( hwnd );
    Check( pDevice != NULL, "Direct3D device for the mesh tests" );

    if( pDevice != NULL )
    {
        static const struct
        {
            const WCHAR* pszFace;
            LONG         lWeight;
            BYTE         bItalic;
            const WCHAR* pszText;
            FLOAT        fDeviation;
            FLOAT        fExtrusion;
        }
        cases[] =
        {
            { L"Tahoma",          FW_NORMAL, 0, L"Windows Vista",        5.0f / 510.0f,  0.5f },
            { L"Tahoma",          FW_BOLD,   0, L"12:34:56 PM",          5.0f / 510.0f,  0.5f },
            { L"Tahoma",          FW_NORMAL, 0, L"O",                    0.0f,           0.5f },     // one font unit
            { L"Tahoma",          FW_NORMAL, 1, L"a b&@%$#g8QB",         0.5f,           0.5f },     // clamped
            { L"Tahoma",          FW_NORMAL, 0, L"i\x00e4\x00d6\x00df\x00e9j;:!?", 5.0f / 110.0f, 0.0f },   // flat
            { L"Times New Roman", FW_NORMAL, 0, L"Windows Vista",        5.0f / 1010.0f, 0.5f },
            { L"Times New Roman", FW_BOLD,   1, L"Quick brown fox 0123", 5.0f / 20.0f,   0.1f },
            { L"Courier New",     FW_NORMAL, 0, L"{[(<Mesh>)]}",         5.0f / 510.0f,  2.0f },
            { L"Arial",           FW_NORMAL, 0, L"@WMwm&8%",             0.002f,         0.5f },
            { L"Symbol",          FW_NORMAL, 0, L"abcdefg",              5.0f / 510.0f,  0.5f },
            { L"Wingdings",       FW_NORMAL, 0, L"JKL123",               5.0f / 510.0f,  0.5f },
            { L"Tahoma",          FW_NORMAL, 0, L" ",                    5.0f / 510.0f,  0.5f },     // nothing to draw
            { L"Tahoma",          FW_NORMAL, 0, L"",                     5.0f / 510.0f,  0.5f },     // invalid
            { L"Tahoma",          FW_NORMAL, 0, L"x",                    -1.0f,          0.5f },     // invalid
            { L"System",          FW_NORMAL, 0, L"bitmap font",          5.0f / 510.0f,  0.5f },     // not a TrueType font
        };

        for( size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++ )
        {
            TestTextMesh( orig, pDevice, cases[i].pszFace, cases[i].lWeight, cases[i].bItalic, cases[i].pszText,
                          cases[i].fDeviation, cases[i].fExtrusion );
        }

        TestBuildTextMesh( orig, pOrig, pMine, pDevice, L"Tahoma", L"Windows Vista", 500 );
        TestBuildTextMesh( orig, pOrig, pMine, pDevice, L"Tahoma", L"11:59:59 PM", 1000 );
        TestBuildTextMesh( orig, pOrig, pMine, pDevice, L"Times New Roman", L"Ag", 0 );
        TestBuildTextMesh( orig, pOrig, pMine, pDevice, L"Tahoma", L"abc \x05e9\x05dc\x05d5\x05dd xyz", 300 );    // mixed direction
        TestBuildTextMesh( orig, pOrig, pMine, pDevice, L"Tahoma", L"exactly twenty chars", 100 );

        pDevice->Release();
    }

    DestroyWindow( hwnd );
    CoUninitialize();

    return Summary( "test_text3d" );
}
