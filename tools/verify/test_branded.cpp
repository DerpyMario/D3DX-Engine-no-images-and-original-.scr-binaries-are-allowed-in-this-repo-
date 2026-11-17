//-----------------------------------------------------------------------------
// File: test_branded.cpp
//
// Desc: Differential test of the reconstructed Windows Energy screen saver
//       (branded\src) against the original ssBranded.scr.
//
//           test_branded <path to the original ssBranded.scr>
//
//       The addresses below belong to ssBranded.scr 6.0.6001.18000 (amd64),
//       link time stamp 0x479197c3.
//
//       Checked, step by step against the original code:
//         * CAnimation::Load + CLinearAnim::LoadKey - every key frame resource
//           (times, rotations, positions, the reversed keys)
//         * CAnimation::Go / Stop / OnFrameMove - the animated matrices and
//           the play state for the three loop modes
//         * CToricAnim                    - the camera path of the flare scene
//         * CCamera                       - Setup, OnFrameMove, CreateProjection
//         * CWipe::Go / Stop / OnFrameMove - progress and threshold band
//         * CSolidColorWipe::OnFrameMove   - wipe maps, colours and
//                                            g_cTargetColor over a minute,
//                                            for the three colour orders
//
//       The original links D3DX statically.  So that both sides compute with
//       the same D3DX, the test is linked with
//
//           -Wl,--wrap=D3DXMatrixMultiply,--wrap=D3DXMatrixRotationQuaternion,
//           --wrap=D3DXMatrixTranslation,--wrap=D3DXQuaternionInverse,
//           --wrap=D3DXQuaternionSlerp,--wrap=D3DXMatrixDecompose,
//           --wrap=D3DXVec3Normalize,--wrap=D3DXMatrixRotationYawPitchRoll,
//           --wrap=D3DXMatrixLookAtLH
//
//       and the rebuilt code calls the D3DX math of the original image (its
//       CPU dispatch table); the results must then be bit-identical.  Two
//       things remain that depend on the d3dx9 headers the rebuilt code is
//       compiled with, and are compared with a tolerance:
//         * D3DXMatrixPerspectiveFovLH, which the original has inlined
//         * D3DXVec3Lerp, an inline function: the DirectX SDK computes
//           a + s * (b - a), the MinGW-w64 / Wine headers (1 - s) * a + s * b,
//           so the animated positions may differ in the last bits; the
//           animated rotations must still be bit-identical
//
//       The test executable must contain the RT_RCDATA resources of
//       ssbranded.rc (the build links them in, SCRNSAVE_NO_ASSETS is enough).
//-----------------------------------------------------------------------------
#include <new>
#include "origimage.h"

// the test has to look into the classes
#define protected public
#define private public
#include "linearanim.h"
#include "toricanim.h"
#include "camera.h"
#include "wipe.h"
#include "solidcolorwipe.h"
#include "ssview.h"
#undef protected
#undef private

#include "resource.h"

#define BRANDED_TIMESTAMP           0x479197c3

#define RVA_CAnimation_Go           0x0002fb24
#define RVA_CAnimation_Stop         0x0002fb68
#define RVA_CAnimation_Load         0x0002fbf0
#define RVA_CAnimation_OnFrameMove  0x000300c8
#define RVA_CCamera_ctor            0x00031c24
#define RVA_CCamera_Setup           0x00031d10
#define RVA_CCamera_OnFrameMove     0x00031d94
#define RVA_CCamera_CreateProj      0x00031f04
#define RVA_CSolidColorWipe_Move    0x00036274
#define RVA_CToricAnim_ctor         0x0003813c
#define RVA_CToricAnim_GetLength    0x00038274
#define RVA_CToricAnim_OnFrameMove  0x000382c0
#define RVA_CWipe_Go                0x0003860c
#define RVA_CWipe_Stop              0x00038670
#define RVA_CWipe_OnFrameMove       0x0003883c

#define RVA_VTBL_CLinearAnim        0x00001f30
#define RVA_VTBL_CSolidColorWipe    0x00023520
#define RVA_VTBL_CWipe              0x000237c0

// D3DX math: the dispatch table of the original, and D3DXMatrixLookAtLH
#define RVA_PFN_MatrixMultiply      0x0012aea8
#define RVA_PFN_Vec3Normalize       0x0012aec8
#define RVA_PFN_MatrixDecompose     0x0012aef0
#define RVA_PFN_MatrixTranslation   0x0012af68
#define RVA_PFN_QuaternionSlerp     0x0012afb0
#define RVA_PFN_MatrixRotationQuat  0x0012afc8
#define RVA_PFN_MatrixRotationYPR   0x0012afd8
#define RVA_PFN_QuaternionInverse   0x0012aff8
#define RVA_D3DXMatrixLookAtLH      0x0003e490

#define RVA_g_eState                0x00130d18
#define RVA_g_pView                 0x00130d20
#define RVA_g_cTargetColor          0x00130f70

typedef void*   ( *PFN_ctor )( void* );
typedef void    ( *PFN_time )( void*, FLOAT );
typedef HRESULT ( *PFN_load )( void*, DWORD );
typedef void    ( *PFN_move )( void*, FLOAT, FLOAT, D3DXMATRIX* );
typedef FLOAT   ( *PFN_length )( void* );
typedef void    ( *PFN_setup )( void*, CAMPARMS* );
typedef void    ( *PFN_void )( void* );

static const float TOLERANCE = 1e-6f;   // D3DXMatrixPerspectiveFovLH only

static char g_szMsg[256];

static bool SameMatrix( const D3DXMATRIX& a, const D3DXMATRIX& b, size_t* pcIdentical )
{
    size_t cDifferent;
    float fDiff = MaxFloatDiff( (const float*)&a, (const float*)&b, 16, &cDifferent );
    if( cDifferent == 0 && pcIdentical != NULL )
        ( *pcIdentical )++;
    return fDiff <= TOLERANCE;
}

static bool IdenticalMatrix( const D3DXMATRIX& a, const D3DXMATRIX& b )
{
    return memcmp( &a, &b, sizeof(D3DXMATRIX) ) == 0;
}

// Rotation (and last column) bit-identical, translation up to the rounding of
// the interpolation.  (The positions of the animations are below 100; when an
// animation is resumed with Go() it extrapolates from its current key, where
// a + s * (b - a) and (1 - s) * a + s * b cancel differently.)
static bool SameRigidMatrix( const D3DXMATRIX& a, const D3DXMATRIX& b, bool* pbIdentical )
{
    *pbIdentical = IdenticalMatrix( a, b );
    if( memcmp( &a.m[0][0], &b.m[0][0], 12 * sizeof(FLOAT) ) != 0 || a._44 != b._44 )
        return false;
    for( int i = 0; i < 3; i++ )
    {
        if( fabsf( a.m[3][i] - b.m[3][i] ) > 1e-4f + 1e-6f * fabsf( a.m[3][i] ) )
            return false;
    }
    return true;
}




//-----------------------------------------------------------------------------
// The D3DX math of the original, for the rebuilt code
//-----------------------------------------------------------------------------
static OrigImage* g_pOrigD3DX = NULL;

template <class PFN> static PFN OrigD3DX( DWORD rvaPointer )
{
    return *g_pOrigD3DX->At<PFN*>( rvaPointer );
}

extern "C"
{
D3DXMATRIX* WINAPI __real_D3DXMatrixMultiply( D3DXMATRIX*, const D3DXMATRIX*, const D3DXMATRIX* );
D3DXMATRIX* WINAPI __wrap_D3DXMatrixMultiply( D3DXMATRIX* pOut, const D3DXMATRIX* pM1, const D3DXMATRIX* pM2 )
{
    typedef D3DXMATRIX* ( WINAPI *PFN )( D3DXMATRIX*, const D3DXMATRIX*, const D3DXMATRIX* );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_MatrixMultiply )( pOut, pM1, pM2 )
                       : __real_D3DXMatrixMultiply( pOut, pM1, pM2 );
}

D3DXMATRIX* WINAPI __real_D3DXMatrixRotationQuaternion( D3DXMATRIX*, const D3DXQUATERNION* );
D3DXMATRIX* WINAPI __wrap_D3DXMatrixRotationQuaternion( D3DXMATRIX* pOut, const D3DXQUATERNION* pQ )
{
    typedef D3DXMATRIX* ( WINAPI *PFN )( D3DXMATRIX*, const D3DXQUATERNION* );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_MatrixRotationQuat )( pOut, pQ )
                       : __real_D3DXMatrixRotationQuaternion( pOut, pQ );
}

D3DXMATRIX* WINAPI __real_D3DXMatrixTranslation( D3DXMATRIX*, FLOAT, FLOAT, FLOAT );
D3DXMATRIX* WINAPI __wrap_D3DXMatrixTranslation( D3DXMATRIX* pOut, FLOAT x, FLOAT y, FLOAT z )
{
    typedef D3DXMATRIX* ( WINAPI *PFN )( D3DXMATRIX*, FLOAT, FLOAT, FLOAT );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_MatrixTranslation )( pOut, x, y, z )
                       : __real_D3DXMatrixTranslation( pOut, x, y, z );
}

D3DXQUATERNION* WINAPI __real_D3DXQuaternionInverse( D3DXQUATERNION*, const D3DXQUATERNION* );
D3DXQUATERNION* WINAPI __wrap_D3DXQuaternionInverse( D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ )
{
    typedef D3DXQUATERNION* ( WINAPI *PFN )( D3DXQUATERNION*, const D3DXQUATERNION* );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_QuaternionInverse )( pOut, pQ )
                       : __real_D3DXQuaternionInverse( pOut, pQ );
}

D3DXQUATERNION* WINAPI __real_D3DXQuaternionSlerp( D3DXQUATERNION*, const D3DXQUATERNION*, const D3DXQUATERNION*,
                                                   FLOAT );
D3DXQUATERNION* WINAPI __wrap_D3DXQuaternionSlerp( D3DXQUATERNION* pOut, const D3DXQUATERNION* pQ1,
                                                   const D3DXQUATERNION* pQ2, FLOAT t )
{
    typedef D3DXQUATERNION* ( WINAPI *PFN )( D3DXQUATERNION*, const D3DXQUATERNION*, const D3DXQUATERNION*, FLOAT );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_QuaternionSlerp )( pOut, pQ1, pQ2, t )
                       : __real_D3DXQuaternionSlerp( pOut, pQ1, pQ2, t );
}

HRESULT WINAPI __real_D3DXMatrixDecompose( D3DXVECTOR3*, D3DXQUATERNION*, D3DXVECTOR3*, const D3DXMATRIX* );
HRESULT WINAPI __wrap_D3DXMatrixDecompose( D3DXVECTOR3* pScale, D3DXQUATERNION* pRot, D3DXVECTOR3* pTrans,
                                           const D3DXMATRIX* pM )
{
    typedef HRESULT ( WINAPI *PFN )( D3DXVECTOR3*, D3DXQUATERNION*, D3DXVECTOR3*, const D3DXMATRIX* );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_MatrixDecompose )( pScale, pRot, pTrans, pM )
                       : __real_D3DXMatrixDecompose( pScale, pRot, pTrans, pM );
}

D3DXVECTOR3* WINAPI __real_D3DXVec3Normalize( D3DXVECTOR3*, const D3DXVECTOR3* );
D3DXVECTOR3* WINAPI __wrap_D3DXVec3Normalize( D3DXVECTOR3* pOut, const D3DXVECTOR3* pV )
{
    typedef D3DXVECTOR3* ( WINAPI *PFN )( D3DXVECTOR3*, const D3DXVECTOR3* );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_Vec3Normalize )( pOut, pV ) : __real_D3DXVec3Normalize( pOut, pV );
}

D3DXMATRIX* WINAPI __real_D3DXMatrixRotationYawPitchRoll( D3DXMATRIX*, FLOAT, FLOAT, FLOAT );
D3DXMATRIX* WINAPI __wrap_D3DXMatrixRotationYawPitchRoll( D3DXMATRIX* pOut, FLOAT y, FLOAT p, FLOAT r )
{
    typedef D3DXMATRIX* ( WINAPI *PFN )( D3DXMATRIX*, FLOAT, FLOAT, FLOAT );
    return g_pOrigD3DX ? OrigD3DX<PFN>( RVA_PFN_MatrixRotationYPR )( pOut, y, p, r )
                       : __real_D3DXMatrixRotationYawPitchRoll( pOut, y, p, r );
}

D3DXMATRIX* WINAPI __real_D3DXMatrixLookAtLH( D3DXMATRIX*, const D3DXVECTOR3*, const D3DXVECTOR3*, const D3DXVECTOR3* );
D3DXMATRIX* WINAPI __wrap_D3DXMatrixLookAtLH( D3DXMATRIX* pOut, const D3DXVECTOR3* pEye, const D3DXVECTOR3* pAt,
                                              const D3DXVECTOR3* pUp )
{
    typedef D3DXMATRIX* ( WINAPI *PFN )( D3DXMATRIX*, const D3DXVECTOR3*, const D3DXVECTOR3*, const D3DXVECTOR3* );
    return g_pOrigD3DX ? g_pOrigD3DX->At<PFN>( RVA_D3DXMatrixLookAtLH )( pOut, pEye, pAt, pUp )
                       : __real_D3DXMatrixLookAtLH( pOut, pEye, pAt, pUp );
}
}

// An object of the original with the same contents as one of ours: copy ours
// and point the virtual function table at the original's.
template <class T> static T* CloneForOriginal( OrigImage& orig, const T& mine, DWORD rvaVtbl )
{
    T* p = (T*)_aligned_malloc( sizeof(T), 16 );
    memcpy( (void*)p, (const void*)&mine, sizeof(T) );
    *(BYTE**)p = orig.Base() + rvaVtbl;
    return p;
}




//-----------------------------------------------------------------------------
// Key frame animations
//-----------------------------------------------------------------------------
static bool SameKeys( const ANIM_KEY* a, const ANIM_KEY* b, UINT cKeys )
{
    for( UINT i = 0; i < cKeys; i++ )
    {
        if( a[i].fTime != b[i].fTime || memcmp( &a[i].qRot, &b[i].qRot, sizeof(D3DXQUATERNION) ) != 0 ||
            memcmp( &a[i].vPos, &b[i].vPos, sizeof(D3DXVECTOR3) ) != 0 )
        {
            return false;
        }
    }
    return true;
}

static void TestAnimation( OrigImage& orig, DWORD idAnim, const char* pszName )
{
    CLinearAnim mine;
    CLinearAnim* pOrig = CloneForOriginal( orig, mine, RVA_VTBL_CLinearAnim );

    HRESULT hrMine = mine.Load( idAnim );
    HRESULT hrOrig = orig.At<PFN_load>( RVA_CAnimation_Load )( pOrig, idAnim );

    sprintf( g_szMsg, "%s: Load -> %08lx / %08lx, %u keys, %.4f s, type %d", pszName, (unsigned long)hrMine,
             (unsigned long)hrOrig, mine.m_cKeys, mine.m_fLength, (int)mine.m_eType );
    Check( hrMine == hrOrig && mine.m_cKeys == pOrig->m_cKeys && mine.m_fLength == pOrig->m_fLength &&
           mine.m_eType == pOrig->m_eType && mine.m_eState == pOrig->m_eState, g_szMsg );
    if( FAILED( hrMine ) || FAILED( hrOrig ) )
        return;

    sprintf( g_szMsg, "%s: forward and reversed keys", pszName );
    Check( SameKeys( mine.m_pFwdKeys, pOrig->m_pFwdKeys, mine.m_cKeys ) &&
           SameKeys( mine.m_pRevKeys, pOrig->m_pRevKeys, mine.m_cKeys ) &&
           mine.m_pLastKey - mine.m_pKeys == pOrig->m_pLastKey - pOrig->m_pKeys, g_szMsg );

    static const char* s_aszLoop[] = { "once", "repeat", "ping-pong" };
    for( int eLoop = CAnimation::LOOP_ONCE; eLoop <= CAnimation::LOOP_PINGPONG; eLoop++ )
    {
        mine.m_eLoop = pOrig->m_eLoop = (CAnimation::LoopMode)eLoop;

        FLOAT fTime = 0.25f;
        mine.Go( fTime );
        orig.At<PFN_time>( RVA_CAnimation_Go )( pOrig, fTime );

        bool bSame = true;
        size_t cIdentical = 0;
        int cSteps = (int)( mine.m_fLength * 3.0f / 0.04f ) + 10;
        for( int i = 0; i < cSteps && bSame; i++ )
        {
            // a pause during the first run.  (Not after the end: a "once"
            // animation that is stopped when done and started again
            // interpolates past the last key - in the original as well.)
            if( i == cSteps / 8 )
            {
                mine.Stop( fTime );
                orig.At<PFN_time>( RVA_CAnimation_Stop )( pOrig, fTime );
            }
            else if( i == cSteps / 8 + 5 )
            {
                mine.Go( fTime );
                orig.At<PFN_time>( RVA_CAnimation_Go )( pOrig, fTime );
            }

            D3DXMATRIX mMine, mOrig;
            D3DXMatrixIdentity( &mMine );
            D3DXMatrixIdentity( &mOrig );
            mine.OnFrameMove( fTime, 0.04f, mMine );
            orig.At<PFN_move>( RVA_CAnimation_OnFrameMove )( pOrig, fTime, 0.04f, &mOrig );

            bool bIdentical;
            bSame = SameRigidMatrix( mMine, mOrig, &bIdentical ) && mine.m_eState == pOrig->m_eState &&
                    mine.m_iKey == pOrig->m_iKey && mine.m_iNextKey == pOrig->m_iNextKey &&
                    mine.m_bReverse == pOrig->m_bReverse && mine.m_fStartTime == pOrig->m_fStartTime;
            if( !bSame )
            {
                printf( "        step %d (t=%.3f): state %d/%d key %u/%u\n", i, fTime, mine.m_eState,
                        pOrig->m_eState, mine.m_iKey, pOrig->m_iKey );
                for( int k = 0; k < 16; k++ )
                {
                    if( memcmp( &( (float*)&mMine )[k], &( (float*)&mOrig )[k], sizeof(float) ) != 0 )
                        printf( "          [%d] %.9g / %.9g\n", k, ( (float*)&mMine )[k], ( (float*)&mOrig )[k] );
                }
            }
            else if( bIdentical )
                cIdentical++;

            fTime += 0.04f;
        }

        sprintf( g_szMsg, "%s: OnFrameMove, loop %s, %d frames (%u bit-identical)", pszName, s_aszLoop[eLoop],
                 cSteps, (unsigned)cIdentical );
        Check( bSame, g_szMsg );
    }
}




//-----------------------------------------------------------------------------
// The camera path of the flare scene, and the camera
//-----------------------------------------------------------------------------
static void SetFlareToric( TORICPARMS* p )
{
    p->vCenter = D3DXVECTOR3( 8.75f, 0.0f, 0.0f );
    p->vLookAt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    p->vUp = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    p->fFreq1 = 1.0f / 41.0f;
    p->fPhase1 = 0.0f;
    p->fFreq2 = 1.0f / 14.0f;
    p->fPhase2 = 0.0f;
    p->fRadius1X = 4.125f;
    p->fRadius1Z = 5.0f;
    p->fRadius2X = 4.125f;
    p->fRadius2Y = 6.0f;
}

static void TestToricAndCamera( OrigImage& orig, CSSView* pView )
{
    // constructors (both on memory filled with the same garbage)
    CToricAnim* pMine = (CToricAnim*)_aligned_malloc( sizeof(CToricAnim), 16 );
    memset( (void*)pMine, 0xcd, sizeof(CToricAnim) );
    new( pMine ) CToricAnim;
    CToricAnim& mine = *pMine;
    CToricAnim* pOrig = (CToricAnim*)_aligned_malloc( sizeof(CToricAnim), 16 );
    memset( (void*)pOrig, 0xcd, sizeof(CToricAnim) );
    orig.At<PFN_ctor>( RVA_CToricAnim_ctor )( pOrig );
    Check( memcmp( (BYTE*)pMine + 8, (BYTE*)pOrig + 8, sizeof(CToricAnim) - 8 ) == 0,
           "CToricAnim: constructor" );

    Check( mine.GetLength() == orig.At<PFN_length>( RVA_CToricAnim_GetLength )( pOrig ),
           "CToricAnim: GetLength" );

    SetFlareToric( &mine.m_Parms );
    SetFlareToric( &pOrig->m_Parms );
    mine.Go( 1.0f );
    orig.At<PFN_time>( RVA_CAnimation_Go )( pOrig, 1.0f );

    bool bSame = true;
    for( int i = 0; i < 2000 && bSame; i++ )
    {
        FLOAT fTime = 1.0f + i * 0.15f;
        D3DXMATRIX mMine, mOrig;
        mine.OnFrameMove( fTime, 0.15f, mMine );
        orig.At<PFN_move>( RVA_CToricAnim_OnFrameMove )( pOrig, fTime, 0.15f, &mOrig );
        bSame = IdenticalMatrix( mMine, mOrig );
    }
    sprintf( g_szMsg, "CToricAnim: OnFrameMove, 300 s, bit-identical" );
    Check( bSame, g_szMsg );

    // the camera of the flare
    CCamera* pCamMine = (CCamera*)_aligned_malloc( sizeof(CCamera), 16 );
    memset( (void*)pCamMine, 0xcd, sizeof(CCamera) );
    new( pCamMine ) CCamera;
    CCamera& camMine = *pCamMine;
    CCamera* pCamOrig = (CCamera*)_aligned_malloc( sizeof(CCamera), 16 );
    memset( (void*)pCamOrig, 0xcd, sizeof(CCamera) );
    orig.At<PFN_ctor>( RVA_CCamera_ctor )( pCamOrig );
    size_t cId = 0;
    Check( memcmp( pCamMine, pCamOrig, offsetof(CCamera, m_mProj) ) == 0 &&
           memcmp( &pCamMine->m_fFov, &pCamOrig->m_fFov, sizeof(CCamera) - offsetof(CCamera, m_fFov) ) == 0 &&
           SameMatrix( pCamMine->m_mProj, pCamOrig->m_mProj, &cId ), "CCamera: constructor" );

    CAMPARMS cam = { 40.0f, D3DXVECTOR3( 107.0f, 34.0f, 0.0f ), D3DXVECTOR3( 0.0f, 0.0f, 0.0f ),
                     D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) };
    camMine.Setup( &cam );
    orig.At<PFN_setup>( RVA_CCamera_Setup )( pCamOrig, &cam );
    Check( memcmp( pCamMine, pCamOrig, offsetof(CCamera, m_mProj) ) == 0 &&
           memcmp( &pCamMine->m_fFov, &pCamOrig->m_fFov, sizeof(CCamera) - offsetof(CCamera, m_fFov) ) == 0,
           "CCamera: Setup" );

    camMine.m_pAnim = &mine;
    pCamOrig->m_pAnim = pOrig;

    bSame = true;
    D3DXMATRIX mParent;
    D3DXMatrixTranslation( &mParent, 1.0f, 2.0f, 3.0f );
    for( int i = 0; i < 200 && bSame; i++ )
    {
        FLOAT fTime = 1.0f + i * 0.7f;
        camMine.OnFrameMove( fTime, 0.7f, mParent );
        orig.At<PFN_move>( RVA_CCamera_OnFrameMove )( pCamOrig, fTime, 0.7f, &mParent );
        bSame = IdenticalMatrix( camMine.m_mAnim, pCamOrig->m_mAnim ) &&
                IdenticalMatrix( camMine.m_mView, pCamOrig->m_mView );
    }
    sprintf( g_szMsg, "CCamera: OnFrameMove on the toric path, bit-identical" );
    Check( bSame, g_szMsg );

    // landscape and portrait back buffers
    static const UINT s_auSize[][2] = { { 800, 600 }, { 1280, 1024 }, { 600, 800 }, { 64, 48 } };
    for( int i = 0; i < 4; i++ )
    {
        D3DSURFACE_DESC* pDesc = &pView->m_d3dsdBackBuffer;
        pDesc->Width = s_auSize[i][0];
        pDesc->Height = s_auSize[i][1];
        g_pView = pView;
        *orig.At<CSSView**>( RVA_g_pView ) = pView;     // only m_d3dsdBackBuffer is read

        camMine.CreateProjection();
        orig.At<PFN_void>( RVA_CCamera_CreateProj )( pCamOrig );

        cId = 0;
        sprintf( g_szMsg, "CCamera: CreateProjection %ux%u (fov %g / %g)", s_auSize[i][0], s_auSize[i][1],
                 camMine.m_fFov, pCamOrig->m_fFov );
        Check( camMine.m_fFov == pCamOrig->m_fFov && SameMatrix( camMine.m_mProj, pCamOrig->m_mProj, &cId ),
               g_szMsg );
    }
}




//-----------------------------------------------------------------------------
// Wipes
//-----------------------------------------------------------------------------
static void TestWipe( OrigImage& orig )
{
    CWipe mine;
    mine.m_WipeParms.fDuration = 9.0f;
    mine.m_WipeParms.fWidth = 0.5f;
    mine.m_eState = CAnimation::STATE_LOADED;
    CWipe* pOrig = CloneForOriginal( orig, mine, RVA_VTBL_CWipe );

    mine.Go( 2.0f );
    orig.At<PFN_time>( RVA_CWipe_Go )( pOrig, 2.0f );

    bool bSame = true;
    for( int i = 0; i < 400 && bSame; i++ )
    {
        FLOAT fTime = 2.0f + i * 0.033f;
        if( i == 100 )
        {
            mine.Stop( fTime );
            orig.At<PFN_time>( RVA_CWipe_Stop )( pOrig, fTime );
        }
        else if( i == 130 )
        {
            mine.Go( fTime );
            orig.At<PFN_time>( RVA_CWipe_Go )( pOrig, fTime );
        }

        D3DXMATRIX mParent;
        D3DXMatrixIdentity( &mParent );
        mine.OnFrameMove( fTime, 0.033f, mParent );
        orig.At<PFN_move>( RVA_CWipe_OnFrameMove )( pOrig, fTime, 0.033f, &mParent );

        bSame = mine.m_fProgress == pOrig->m_fProgress && mine.m_fLevel0 == pOrig->m_fLevel0 &&
                mine.m_fLevel1 == pOrig->m_fLevel1 && mine.m_eState == pOrig->m_eState &&
                mine.m_fStartTime == pOrig->m_fStartTime;
    }
    Check( bSame, "CWipe: Go/Stop/OnFrameMove, progress and threshold band bit-identical" );
}

struct SCWFRAME
{
    UINT        iColor;
    UINT        iWipe;
    FLOAT       fWipeStart;
    FLOAT       fNextWipe;
    D3DXCOLOR   cPrev;
    D3DXCOLOR   cNext;
    FLOAT       fProgress;
    FLOAT       afTarget[3];
};

static void RecordSolidColorWipe( OrigImage* pOrig, CSolidColorWipe* p, SCWFRAME* pFrames, int cFrames )
{
    srand( 4321 );

    for( int i = 0; i < cFrames; i++ )
    {
        FLOAT fTime = 10.0f + i / 30.0f;
        D3DXMATRIX mParent;
        D3DXMatrixIdentity( &mParent );

        if( pOrig != NULL )
            pOrig->At<PFN_move>( RVA_CSolidColorWipe_Move )( p, fTime, 1.0f / 30.0f, &mParent );
        else
            p->OnFrameMove( fTime, 1.0f / 30.0f, mParent );

        const D3DXCOLOR* pTarget = pOrig != NULL ? pOrig->At<D3DXCOLOR*>( RVA_g_cTargetColor ) : &g_cTargetColor;
        pFrames[i].iColor = p->m_iColor;
        pFrames[i].iWipe = p->m_iWipe;
        pFrames[i].fWipeStart = p->m_fWipeStart;
        pFrames[i].fNextWipe = p->m_fNextWipe;
        pFrames[i].cPrev = p->m_cPrevColor;
        pFrames[i].cNext = p->m_cNextColor;
        pFrames[i].fProgress = p->m_Wipe.m_fProgress;
        pFrames[i].afTarget[0] = pTarget->r;
        pFrames[i].afTarget[1] = pTarget->g;
        pFrames[i].afTarget[2] = pTarget->b;
    }
}

static void TestSolidColorWipe( OrigImage& orig )
{
    static const char* s_aszOrder[] = { "random", "cycle", "ping-pong" };
    const int cFrames = 30 * 75;    // 75 seconds: seven wipes

    SCWFRAME* pMine = new SCWFRAME[cFrames];
    SCWFRAME* pOrigFrames = new SCWFRAME[cFrames];

    for( int eOrder = 0; eOrder < 3; eOrder++ )
    {
        CSolidColorWipe mine;
        mine.m_eColorOrder = (CSolidColorWipe::ColorOrder)eOrder;

        CSolidColorWipe* pOrig = CloneForOriginal( orig, mine, RVA_VTBL_CSolidColorWipe );
        *(BYTE**)&pOrig->m_Wipe = orig.Base() + RVA_VTBL_CWipe;

        IDirect3DTexture9* apMapsMine[SCWIPE_COUNT] = { NULL };
        IDirect3DTexture9* apMapsOrig[SCWIPE_COUNT] = { NULL };
        mine.m_ppWipeMaps = apMapsMine;
        pOrig->m_ppWipeMaps = apMapsOrig;

        RecordSolidColorWipe( NULL, &mine, pMine, cFrames );
        RecordSolidColorWipe( &orig, pOrig, pOrigFrames, cFrames );

        int iFirstBad = -1;
        UINT cWipes = 0;
        for( int i = 0; i < cFrames && iFirstBad < 0; i++ )
        {
            if( memcmp( &pMine[i], &pOrigFrames[i], sizeof(SCWFRAME) ) != 0 )
                iFirstBad = i;
            if( i > 0 && pMine[i].fWipeStart != pMine[i - 1].fWipeStart )
                cWipes++;
        }
        if( iFirstBad >= 0 )
            printf( "        frame %d: colour %u/%u wipe %u/%u\n", iFirstBad, pMine[iFirstBad].iColor,
                    pOrigFrames[iFirstBad].iColor, pMine[iFirstBad].iWipe, pOrigFrames[iFirstBad].iWipe );

        sprintf( g_szMsg, "CSolidColorWipe: %s colour order, %d frames, %u wipes, bit-identical",
                 s_aszOrder[eOrder], cFrames, cWipes );
        Check( iFirstBad < 0, g_szMsg );

        mine.m_ppWipeMaps = NULL;   // (not ours to delete)
    }

    delete[] pMine;
    delete[] pOrigFrames;
}




//-----------------------------------------------------------------------------
// Name: main()
//-----------------------------------------------------------------------------
int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        printf( "usage: test_branded <path to the original ssBranded.scr>\n" );
        return 2;
    }

    OrigImage orig;
    if( !orig.Load( argv[1], BRANDED_TIMESTAMP ) )
        return 2;

    g_pOrigD3DX = &orig;

    printf( "key frame animations\n" );
    static const struct { DWORD id; const char* pszName; } s_aAnims[] =
    {
        { IDR_ANIM_LOGO, "logo" }, { IDR_ANIM_MSFLARE, "msflare" }, { IDR_ANIM_PILL1, "pill1" },
        { IDR_ANIM_PILL2_00, "pill2_00" }, { IDR_ANIM_PILL2_01, "pill2_01" }, { IDR_ANIM_PILL2_02, "pill2_02" },
        { IDR_ANIM_PILL2_03, "pill2_03" }, { IDR_ANIM_PILL2_04, "pill2_04" }, { IDR_ANIM_PILL2_05, "pill2_05" },
        { IDR_ANIM_PILL2_06, "pill2_06" }, { IDR_ANIM_PILL2_07, "pill2_07" }, { IDR_ANIM_PILL2_08, "pill2_08" },
        { IDR_ANIM_PILL2_09, "pill2_09" }, { IDR_ANIM_XFORM, "xform" }, { 9999, "missing resource" },
    };
    for( size_t i = 0; i < sizeof(s_aAnims) / sizeof(s_aAnims[0]); i++ )
        TestAnimation( orig, s_aAnims[i].id, s_aAnims[i].pszName );

    printf( "camera\n" );
    CSSView* pView = new CSSView;
    TestToricAndCamera( orig, pView );

    printf( "wipes\n" );
    TestWipe( orig );
    TestSolidColorWipe( orig );

    return Summary( "test_branded" );
}
