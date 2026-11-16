//-----------------------------------------------------------------------------
// File: test_bubbles.cpp
//
// Desc: Differential test: reconstructed Bubbles / Helpers sources against the
//       original Bubbles.scr (6.0.6000.16386, amd64, link time 0x4549BB49).
//
//           test_bubbles.exe <path to original Bubbles.scr>
//-----------------------------------------------------------------------------
#include <new>
#include "origimage.h"
#include "mockd3d.h"

#include "point.h"
#include "noise.h"
#include "color.h"
#include "particle.h"
#include "bubbles.h"

// Bubbles.scr 6.0.6000.16386 (amd64)
#define BUBBLES_TIMESTAMP               0x4549bb49
#define RVA_ctor                        0x15968
#define RVA_OneTimeSceneInit            0x15e24
#define RVA_FrameMove                   0x16134
#define RVA_Render                      0x16c64
#define RVA_RestoreDeviceObjects        0x18590
#define RVA_SetPerFrameStates           0x188a0
#define RVA_InvalidateDeviceObjects     0x18c28
#define RVA_SetDevice                   0x18cf0
#define RVA_ValidateSettings            0x192d0
#define RVA_Turbulence                  0x26324
#define RVA_HueToRGB                    0x263b8
#define RVA_Grid2D_Create               0x26460
#define RVA_Grid2D_AddParticle          0x26628
#define RVA_Grid2D_MoveParticle         0x2668c
#define RVA_Grid2D_MapPoint             0x2672c

// CD3DScreensaver members the test has to reach (protected in the class)
#define OFS_m_SaverMode                 0x0008
#define OFS_m_pd3dDevice                0x2460
#define OFS_m_rcRenderTotal             0x2468
#define OFS_m_rcRenderCurDevice         0x2478

typedef void*   ( *PFN_ctor )( void* );
typedef HRESULT ( *PFN_this_hr )( void* );
typedef void    ( *PFN_this_void )( void* );
typedef void    ( *PFN_SetDevice )( void*, UINT );
typedef float   ( *PFN_Turbulence )( const Point3f*, int, bool );
typedef float   ( *PFN_HueToRGB )( float, float, float );
typedef HRESULT ( *PFN_Grid2D_Create )( Grid2D**, ULONGLONG, ULONGLONG, int, int );
typedef void    ( *PFN_Grid2D_Particle )( void*, Particle2D* );
typedef void    ( *PFN_Grid2D_MapPoint )( void*, const Point2f*, int*, int* );

template <class T> static T& Field( void* pObject, size_t offset )
{
    return *(T*)( (BYTE*)pObject + offset );
}

static float Rand( float lo, float hi )
{
    return lo + ( hi - lo ) * ( (float)rand() / 32768.0f );
}

// A Point2f passed by value travels in a general register on x64
static ULONGLONG PackPoint( float x, float y )
{
    ULONGLONG v;
    float f[2] = { x, y };
    memcpy( &v, f, 8 );
    return v;
}

// Order of the particles in every cell of a grid, as sphere indices
static void DumpGrid( Grid2D* pGrid, int nCellsX, int nCellsY, std::vector<int>* pOut )
{
    Particle2DList** ppCells = *(Particle2DList***)pGrid;
    pOut->clear();
    for( int x = 0; x < nCellsX; x++ )
    {
        for( int y = 0; y < nCellsY; y++ )
        {
            pOut->push_back( -1 - ( x * 1000 + y ) );
            int nGuard = 0;
            for( Particle2D* p = ppCells[x][y].m_pHead; p != NULL && nGuard < 100000; p = p->m_pNext, nGuard++ )
                pOut->push_back( ( (Sphere*)p )->nIndex );
        }
    }
}

// Everything about the spheres except the list pointers
static bool SameSpheres( BubblesScreensaver* a, BubblesScreensaver* b, int n, float* pfMaxDiff, size_t* pcDifferent )
{
    bool bSame = true;
    *pfMaxDiff = 0.0f;
    *pcDifferent = 0;
    for( int i = 0; i < n; i++ )
    {
        const Sphere& sa = a->m_Spheres[i];
        const Sphere& sb = b->m_Spheres[i];
        size_t c;
        float d = MaxFloatDiff( &sa.m_pos.x, &sb.m_pos.x, 8, &c );     // pos, prevPos, force, mass, radius
        *pcDifferent += c;
        if( d > *pfMaxDiff )
            *pfMaxDiff = d;
        if( sa.m_nCellX != sb.m_nCellX || sa.m_nCellY != sb.m_nCellY || sa.nIndex != sb.nIndex ||
            sa.dwColor != sb.dwColor || sa.bCollides != sb.bCollides )
            bSame = false;
    }
    return bSame && *pcDifferent == 0;
}

int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        printf( "usage: test_bubbles <original Bubbles.scr>\n" );
        return 2;
    }

    OrigImage orig;
    if( !orig.Load( argv[1], BUBBLES_TIMESTAMP ) )
        return 2;

    char msg[256];

    //-------------------------------------------------------------------------
    // HueToRGB(), Turbulence()
    //-------------------------------------------------------------------------
    {
        PFN_HueToRGB pfn = orig.At<PFN_HueToRGB>( RVA_HueToRGB );
        int nDifferent = 0;
        srand( 1 );
        for( int i = 0; i < 200000; i++ )
        {
            float v1 = Rand( 0.0f, 1.0f ), v2 = Rand( 0.0f, 1.0f ), h = Rand( -0.5f, 1.5f );
            float a = pfn( v1, v2, h );
            float b = HueToRGB( v1, v2, h );
            if( memcmp( &a, &b, 4 ) != 0 )
                nDifferent++;
        }
        sprintf( msg, "HueToRGB(): 200000 samples, %d not bit-identical", nDifferent );
        Check( nDifferent == 0, msg );
    }
    {
        PFN_Turbulence pfn = orig.At<PFN_Turbulence>( RVA_Turbulence );
        int nDifferent = 0;
        srand( 2 );
        for( int i = 0; i < 50000; i++ )
        {
            Point3f p( Rand( -1000.0f, 1000.0f ), Rand( -1000.0f, 1000.0f ), Rand( -1000.0f, 1000.0f ) );
            int nOctaves = 1 + i % 4;
            float a = pfn( &p, nOctaves, false );
            float b = Turbulence( p, nOctaves, false );
            if( memcmp( &a, &b, 4 ) != 0 )
                nDifferent++;
        }
        sprintf( msg, "Turbulence(): 50000 samples, %d not bit-identical", nDifferent );
        Check( nDifferent == 0, msg );
    }

    //-------------------------------------------------------------------------
    // Grid2D
    //-------------------------------------------------------------------------
    {
        Grid2D* pOrig = NULL;
        Grid2D* pMine = NULL;
        HRESULT hr1 = orig.At<PFN_Grid2D_Create>( RVA_Grid2D_Create )(
                          &pOrig, PackPoint( -100.0f, 50.0f ), PackPoint( 1820.0f, 1130.0f ), 13, 7 );
        HRESULT hr2 = Grid2D::Create( &pMine, Point2f( -100.0f, 50.0f ), Point2f( 1820.0f, 1130.0f ), 13, 7 );
        bool bOk = ( hr1 == hr2 && pOrig != NULL && pMine != NULL &&
                     memcmp( (BYTE*)pOrig + 8, (BYTE*)pMine + 8, 0x20 ) == 0 );
        Check( bOk, "Grid2D::Create(): identical grid description" );

        if( bOk )
        {
            int nBad = 0;
            srand( 3 );
            for( int i = 0; i < 100000; i++ )
            {
                Point2f pt( Rand( -400.0f, 2200.0f ), Rand( -300.0f, 1500.0f ) );
                int x1 = 99, y1 = 99, x2 = 77, y2 = 77;
                orig.At<PFN_Grid2D_MapPoint>( RVA_Grid2D_MapPoint )( pOrig, &pt, &x1, &y1 );
                pMine->MapPoint( pt, x2, y2 );
                if( x1 != x2 || y1 != y2 )
                    nBad++;
            }
            sprintf( msg, "Grid2D::MapPoint(): 100000 points, %d mismatches", nBad );
            Check( nBad == 0, msg );

            // Shuffle a few hundred particles around both grids
            const int N = 400;
            static Sphere sa[N], sb[N];
            for( int i = 0; i < N; i++ )
            {
                sa[i].m_pos = sb[i].m_pos = Point2f( Rand( -100.0f, 1820.0f ), Rand( 50.0f, 1130.0f ) );
                sa[i].nIndex = sb[i].nIndex = i;
                orig.At<PFN_Grid2D_Particle>( RVA_Grid2D_AddParticle )( pOrig, &sa[i] );
                pMine->AddParticle( sb[i] );
            }
            for( int k = 0; k < 20000; k++ )
            {
                int i = rand() % N;
                Point2f pt( sa[i].m_pos.x + Rand( -200.0f, 200.0f ), sa[i].m_pos.y + Rand( -200.0f, 200.0f ) );
                sa[i].m_pos = sb[i].m_pos = pt;
                orig.At<PFN_Grid2D_Particle>( RVA_Grid2D_MoveParticle )( pOrig, &sa[i] );
                pMine->MoveParticle( sb[i] );
            }
            std::vector<int> la, lb;
            DumpGrid( pOrig, 13, 7, &la );
            DumpGrid( pMine, 13, 7, &lb );
            Check( la == lb, "Grid2D::AddParticle() / MoveParticle(): identical cell lists after 20000 moves" );
        }
    }

    //-------------------------------------------------------------------------
    // BubblesScreensaver
    //-------------------------------------------------------------------------
    const size_t cbSaver = sizeof(BubblesScreensaver);
    BYTE* rawOrig = (BYTE*)calloc( 1, cbSaver + 256 );
    BYTE* rawMine = (BYTE*)calloc( 1, cbSaver + 256 );

    orig.At<PFN_ctor>( RVA_ctor )( rawOrig );
    BubblesScreensaver* pMine = new( rawMine ) BubblesScreensaver;
    BubblesScreensaver* pOrig = (BubblesScreensaver*)rawOrig;

    // Everything but the vtable pointer must match
    Check( memcmp( rawOrig + 8, rawMine + 8, cbSaver - 8 ) == 0,
           "BubblesScreensaver::BubblesScreensaver(): identical object state" );

    // ValidateSettings()
    {
        bool bSame = true;
        srand( 4 );
        for( int i = 0; i < 2000; i++ )
        {
            BubblesScreensaver* p[2] = { pOrig, pMine };
            float d = Rand( -1.0f, 2.0f ), r = Rand( -50.0f, 400.0f ), s = Rand( -0.1f, 0.3f ), f = Rand( -500.0f, 4000.0f );
            int o = rand() % 9 - 2;
            for( int k = 0; k < 2; k++ )
            {
                p[k]->m_fSphereDensity = d;
                p[k]->m_fRadius = r;
                p[k]->m_nTurbulenceNumOctaves = o;
                p[k]->m_fTurbulenceSpeed = s;
                p[k]->m_fTurbulenceForce = f;
            }
            orig.At<PFN_this_void>( RVA_ValidateSettings )( rawOrig );
            pMine->ValidateSettings();
            if( memcmp( &pOrig->m_bSpanMultiMon, &pMine->m_bSpanMultiMon, 0x20 ) != 0 )
                bSame = false;
        }
        Check( bSame, "BubblesScreensaver::ValidateSettings(): 2000 random settings" );
    }

    // Two scenarios: one monitor, and the left one of two spanned monitors
    for( int iScenario = 0; iScenario < 2; iScenario++ )
    {
        memset( rawOrig, 0, cbSaver );
        memset( rawMine, 0, cbSaver );
        orig.At<PFN_ctor>( RVA_ctor )( rawOrig );
        pMine = new( rawMine ) BubblesScreensaver;

        RECT rcTotal = { 0, 0, 1280, 720 };
        RECT rcDevice = { 0, 0, 1280, 720 };
        if( iScenario == 1 )
        {
            SetRect( &rcTotal, -1024, 0, 1280, 768 );
            SetRect( &rcDevice, -1024, 0, 0, 768 );
        }

        CallLog logOrig, logMine;
        MockObject* pDev[2] = { MockCreateDevice( &logOrig ), MockCreateDevice( &logMine ) };
        BYTE* raw[2] = { rawOrig, rawMine };
        BubblesScreensaver* pSaver[2] = { pOrig, pMine };

        for( int k = 0; k < 2; k++ )
        {
            Field<int>( raw[k], OFS_m_SaverMode ) = sm_full;
            Field<MockObject*>( raw[k], OFS_m_pd3dDevice ) = pDev[k];
            Field<RECT>( raw[k], OFS_m_rcRenderTotal ) = rcTotal;
            Field<RECT>( raw[k], OFS_m_rcRenderCurDevice ) = rcDevice;
            pSaver[k]->m_fRadius = ( iScenario == 0 ) ? 90.0f : 35.0f;
            pSaver[k]->m_nTurbulenceNumOctaves = 1 + iScenario * 2;
            pSaver[k]->m_bMaterialGlass = ( iScenario == 0 );
        }

        // SetDevice()
        orig.At<PFN_SetDevice>( RVA_SetDevice )( rawOrig, 2 );
        pMine->SetDevice( 2 );
        Check( pOrig->m_iDevice == pMine->m_iDevice &&
               (BYTE*)pOrig->m_pDeviceObjects - rawOrig == (BYTE*)pMine->m_pDeviceObjects - rawMine,
               "BubblesScreensaver::SetDevice()" );

        // OneTimeSceneInit()
        srand( 20 + iScenario );
        HRESULT hr1 = orig.At<PFN_this_hr>( RVA_OneTimeSceneInit )( rawOrig );
        srand( 20 + iScenario );
        HRESULT hr2 = pMine->OneTimeSceneInit();
        float fMaxDiff;
        size_t cDifferent;
        bool bSame = SameSpheres( pOrig, pMine, BUBBLES_MAX_SPHERES, &fMaxDiff, &cDifferent );
        sprintf( msg, "BubblesScreensaver::OneTimeSceneInit() [%d]: %d spheres wanted, identical sphere table",
                 iScenario, pOrig->m_nNumSpheresTarget );
        Check( hr1 == hr2 && bSame && pOrig->m_nNumSpheres == pMine->m_nNumSpheres &&
               pOrig->m_nNumSpheresTarget == pMine->m_nNumSpheresTarget &&
               pOrig->m_fRadius == pMine->m_fRadius, msg );

        // FrameMove() - no re-synchronisation: the two simulations have to
        // stay bit-identical on their own
        const int nFrames = ( iScenario == 0 ) ? 3000 : 6000;
        int nFirstBad = -1;
        float fWorst = 0.0f;
        std::vector<int> la, lb;
        int nCellsX = (int)( (float)( rcTotal.right - rcTotal.left ) * 0.5f / pOrig->m_fRadius );
        int nCellsY = (int)( (float)( rcTotal.bottom - rcTotal.top ) * 0.5f / pOrig->m_fRadius );
        for( int f = 0; f < nFrames && nFirstBad < 0; f++ )
        {
            srand( 5000 + f );
            orig.At<PFN_this_hr>( RVA_FrameMove )( rawOrig );
            srand( 5000 + f );
            pMine->FrameMove();

            bSame = SameSpheres( pOrig, pMine, BUBBLES_MAX_SPHERES, &fMaxDiff, &cDifferent ) &&
                    pOrig->m_nNumSpheres == pMine->m_nNumSpheres;
            if( fMaxDiff > fWorst )
                fWorst = fMaxDiff;
            if( bSame && ( f % 50 ) == 0 )
            {
                DumpGrid( pOrig->m_pGrid, nCellsX, nCellsY, &la );
                DumpGrid( pMine->m_pGrid, nCellsX, nCellsY, &lb );
                bSame = ( la == lb );
            }
            if( !bSame )
                nFirstBad = f;
        }
        if( nFirstBad >= 0 )
            sprintf( msg, "BubblesScreensaver::FrameMove() [%d]: diverged at frame %d (max difference %g, %u floats)",
                     iScenario, nFirstBad, fWorst, (unsigned)cDifferent );
        else
            sprintf( msg, "BubblesScreensaver::FrameMove() [%d]: %d frames, %d spheres in play, bit-identical",
                     iScenario, nFrames, pOrig->m_nNumSpheres );
        Check( nFirstBad < 0, msg );

        // Per-device objects: stand-in textures and a synthetic desktop snapshot
        for( int k = 0; k < 2; k++ )
        {
            CallLog* pLog = k ? &logMine : &logOrig;
            DeviceObjects* pObjects = pSaver[k]->m_pDeviceObjects;
            pObjects->pShadowTexture    = (IDirect3DTexture9*)MockCreateTexture( pLog, 256, 256 );
            pObjects->pSolidTexture     = (IDirect3DTexture9*)MockCreateTexture( pLog, 256, 256 );
            pObjects->pHighlightTexture = (IDirect3DTexture9*)MockCreateTexture( pLog, 256, 256 );
            pObjects->pGlassTexture     = (IDirect3DTexture9*)MockCreateTexture( pLog, 256, 256 );

            const int W = 200, H = 120;
            BYTE* pImage = (BYTE*)calloc( 1, 18 + W * H * 4 );
            pImage[2] = 2;
            pImage[12] = (BYTE)W;
            pImage[14] = (BYTE)H;
            pImage[16] = 32;
            for( int i = 0; i < W * H * 4; i++ )
                pImage[18 + i] = (BYTE)( i * 7 + ( i >> 9 ) );
            pObjects->pDesktopImage = pImage;
            pObjects->nDesktopWidth = W;
            pObjects->nDesktopHeight = H;
        }

        // RestoreDeviceObjects()
        logOrig.Clear();
        logMine.Clear();
        hr1 = orig.At<PFN_this_hr>( RVA_RestoreDeviceObjects )( rawOrig );
        hr2 = pMine->RestoreDeviceObjects();
        sprintf( msg, "BubblesScreensaver::RestoreDeviceObjects() [%d]: %u Direct3D calls, identical",
                 iScenario, (unsigned)logOrig.records.size() );
        Check( hr1 == hr2 && CallLog::Equal( logOrig, logMine, 1e-5f ) &&
               ( pOrig->m_pDeviceObjects->pDesktopTexture != NULL ) == ( pMine->m_pDeviceObjects->pDesktopTexture != NULL ),
               msg );

        // SetPerFrameStates()
        logOrig.Clear();
        logMine.Clear();
        orig.At<PFN_this_void>( RVA_SetPerFrameStates )( rawOrig );
        pMine->SetPerFrameStates();
        sprintf( msg, "BubblesScreensaver::SetPerFrameStates() [%d]: %u Direct3D calls, identical",
                 iScenario, (unsigned)logOrig.records.size() );
        Check( CallLog::Equal( logOrig, logMine ), msg );

        // Render(), in every combination of the three switches
        for( int iVariant = 0; iVariant < 8; iVariant++ )
        {
            for( int k = 0; k < 2; k++ )
            {
                pSaver[k]->m_bMaterialGlass = ( iVariant & 1 ) != 0;
                pSaver[k]->m_bShowShadows   = ( iVariant & 2 ) != 0;
                pSaver[k]->m_bShowBubbles   = ( iVariant & 4 ) != 0;
                pSaver[k]->m_bSpanMultiMon  = ( iScenario == 1 ) || ( iVariant == 5 );
            }
            logOrig.Clear();
            logMine.Clear();
            hr1 = orig.At<PFN_this_hr>( RVA_Render )( rawOrig );
            hr2 = pMine->Render();
            sprintf( msg, "BubblesScreensaver::Render() [%d] glass=%d shadows=%d desktop=%d: %u Direct3D calls, identical",
                     iScenario, iVariant & 1, ( iVariant >> 1 ) & 1, ( iVariant >> 2 ) & 1,
                     (unsigned)logOrig.records.size() );
            Check( hr1 == hr2 && CallLog::Equal( logOrig, logMine, 1e-5f ), msg );
        }

        // InvalidateDeviceObjects()
        logOrig.Clear();
        logMine.Clear();
        orig.At<PFN_this_hr>( RVA_InvalidateDeviceObjects )( rawOrig );
        pMine->InvalidateDeviceObjects();
        sprintf( msg, "BubblesScreensaver::InvalidateDeviceObjects() [%d]: %u releases, identical order",
                 iScenario, (unsigned)logOrig.records.size() );
        Check( CallLog::Equal( logOrig, logMine ) && logOrig.records.size() == 5, msg );
    }

    return Summary( "test_bubbles" );
}
