//-----------------------------------------------------------------------------
// File: ss3dfo.cpp
//
// Desc: 3D Flying Objects: the screen saver class, the settings, the
//       materials and lights, and the window that bounces around the
//       screen.
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "ss3dfo.h"
#include "../d3dsaver/dxutil.h"
#include "resource.h"

#define BUF_SIZE                255

#ifndef min     // (windef.h of the Windows SDK)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


//-----------------------------------------------------------------------------
// The styles
//-----------------------------------------------------------------------------
static PFNUPDATESCENE updateFuncs[NUM_STYLES] =
{
    updateWinLogoScene,
    updateExplodeScene,
    updateRibbonScene,
    updateRibbonScene,
    updateSplashScene,
    updateTwistScene,
    updateFlagScene,
};

// (never called: the scenes of a device are not freed)
static PFNDELSCENE delFuncs[NUM_STYLES] =
{
    delWinLogoScene,
    delExplodeScene,
    delRibbonScene,
    delRibbonScene,
    delSplashScene,
    delTwistScene,
    delFlagScene,
};

static PFNINITSCENE initFuncs[NUM_STYLES] =
{
    initWinLogoScene,
    initExplodeScene,
    initRibbonScene,
    initRibbonScene,
    initSplashScene,
    initTwistScene,
    initFlagScene,
};

static int idsStyles[NUM_STYLES] =
{
    IDS_LOGO, IDS_EXPLODE, IDS_RIBBON, IDS_2RIBBONS, IDS_SPLASH, IDS_TWIST, IDS_FLAG
};

// The settings each style uses
static ULONG gulStyleOptions[NUM_STYLES] =
{
    0,
    OPT_COLOR_CYCLE | OPT_SMOOTH_SHADE | OPT_TESSEL | OPT_SIZE,
    OPT_COLOR_CYCLE | OPT_SMOOTH_SHADE | OPT_TESSEL | OPT_SIZE,
    OPT_COLOR_CYCLE | OPT_SMOOTH_SHADE | OPT_TESSEL | OPT_SIZE,
    OPT_COLOR_CYCLE | OPT_SMOOTH_SHADE | OPT_TESSEL | OPT_SIZE,
    OPT_COLOR_CYCLE | OPT_SMOOTH_SHADE | OPT_TESSEL | OPT_SIZE,
    OPT_SMOOTH_SHADE | OPT_TESSEL | OPT_SIZE | OPT_TEXTURE,
};


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
UINT        g_iDevice = (UINT) -1;
BOOL        bSmoothShading = TRUE;
UINT        uSize = 100;
FLOAT       fTesselFact = 2.0f;

static FLOAT matlColors[7][4] =
{
    { 1.0f,   0.0f, 0.0f,  1.0f },
    { 0.0f,   1.0f, 0.0f,  1.0f },
    { 0.0f,   0.0f, 1.0f,  1.0f },
    { 1.0f,   1.0f, 0.0f,  1.0f },
    { 0.0f,   1.0f, 1.0f,  1.0f },
    { 1.0f,   0.0f, 1.0f,  1.0f },
    { 0.235f, 0.0f, 0.78f, 1.0f },
};

// The current material (myglMaterialfv)
static D3DMATERIAL8 gMaterial =
{
    { 1.0f, 1.0f, 1.0f, 1.0f },     // Diffuse
    { 1.0f, 1.0f, 1.0f, 1.0f },     // Ambient
    { 1.0f, 1.0f, 1.0f, 1.0f },     // Specular
    { 0.0f, 0.0f, 0.0f, 0.0f },     // Emissive
    30.0f                           // Power
};

static PFNUPDATESCENE pfnUpdateScene;
TCHAR                 g_szSectName[BUF_SIZE+1];
D3DMATERIAL8          Material[16];
TCHAR                 g_szFname[BUF_SIZE+1];

CFlyingObjectsScreensaver* g_pMyFlyingObjectsScreensaver = NULL;
POINT                   g_ptMonitorOrigin;  // of the screen being drawn
FLOATRECT*              g_pFloatRect;
BOOL                    gbBounced;          // the window bounced
BOOL                    bColorCycle;
DeviceObjects*          g_pDeviceObjects;
BOOL                    gbReturnToOrigin;   // send the window back home
BOOL                    gbAtOrigin;         // ... it is there
int                     gulUpdateFlags;     // MF_ flags of the update functions
int                     Type;
LPDIRECT3DDEVICE8       g_pd3dDevice;
LPDIRECT3DINDEXBUFFER8  g_pIB;
LPDIRECT3DVERTEXBUFFER8 g_pVB;
LPDIRECT3DVERTEXBUFFER8 g_pVBTex;
TEXFILE                 gTexFile;




//-----------------------------------------------------------------------------
// Name: myglMaterialfv()
// Desc: glMaterialfv: sets the current material
//-----------------------------------------------------------------------------
void myglMaterialfv( int face, int pname, FLOAT* params )
{
    if( pname == GL_AMBIENT_AND_DIFFUSE )
    {
        gMaterial.Diffuse = *(D3DCOLORVALUE*) params;
        gMaterial.Ambient = gMaterial.Diffuse;
    }
    else if( pname == GL_SPECULAR )
    {
        gMaterial.Specular = *(D3DCOLORVALUE*) params;
    }
    else if( pname == GL_SHININESS )
    {
        gMaterial.Power = params[0];
    }

    g_pd3dDevice->SetMaterial( &gMaterial );
}




//-----------------------------------------------------------------------------
// Name: myglMaterialf()
// Desc: glMaterialf
//-----------------------------------------------------------------------------
void myglMaterialf( int face, int pname, FLOAT param )
{
    if( pname == GL_SHININESS )
        gMaterial.Power = param;

    g_pd3dDevice->SetMaterial( &gMaterial );
}




//-----------------------------------------------------------------------------
// Name: HsvToRgb()
// Desc: Hue (degrees), saturation and value to an opaque color
//-----------------------------------------------------------------------------
void HsvToRgb( FLOAT h, FLOAT s, FLOAT v, D3DXCOLOR* pColor )
{
    int i;
    FLOAT f, p, q, t;

    pColor->a = 1.0f;

    if( s == 0.0f )
    {
        // no hue
        pColor->r = pColor->g = pColor->b = v;
    }
    else
    {
        if( h >= 360.0f )
            h = 0.0f;
        h = h / 60.0f;
        i = (int) floor( h );
        f = h - i;
        p = v * (1.0f - s);
        q = v * (1.0f - (s * f));
        t = v * (1.0f - (s * (1.0f - f)));

        switch( i )
        {
            case 0: pColor->r = v; pColor->g = t; pColor->b = p; break;
            case 1: pColor->r = q; pColor->g = v; pColor->b = p; break;
            case 2: pColor->r = p; pColor->g = v; pColor->b = t; break;
            case 3: pColor->r = p; pColor->g = q; pColor->b = v; break;
            case 4: pColor->r = t; pColor->g = p; pColor->b = v; break;
            case 5: pColor->r = v; pColor->g = p; pColor->b = q; break;
            default: break;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: SaverAlloc(), SaverFree()
// Desc:
//-----------------------------------------------------------------------------
void* SaverAlloc( ULONG size )
{
    return malloc( size );
}

void SaverFree( void* pMem )
{
    free( pMem );
}




//-----------------------------------------------------------------------------
// Name: initMaterial()
// Desc: The material of a color: specular white
//-----------------------------------------------------------------------------
static void initMaterial( int id, FLOAT r, FLOAT g, FLOAT b, FLOAT a )
{
    Material[id].Ambient.r = r;
    Material[id].Ambient.g = g;
    Material[id].Ambient.b = b;
    Material[id].Ambient.a = a;

    Material[id].Diffuse.r = r;
    Material[id].Diffuse.g = g;
    Material[id].Diffuse.b = b;
    Material[id].Diffuse.a = a;

    Material[id].Specular.r = 1.0f;
    Material[id].Specular.g = 1.0f;
    Material[id].Specular.b = 1.0f;
    Material[id].Specular.a = 1.0f;

    Material[id].Power = 128.0f;
}




//-----------------------------------------------------------------------------
// Name: InitScene()
// Desc: Materials, light, and the scene of the style
//-----------------------------------------------------------------------------
BOOL CFlyingObjectsScreensaver::InitScene()
{
    D3DLIGHT8 light;
    int i;

    for( i = 0; i < 7; i++ )
        initMaterial( i, matlColors[i][0], matlColors[i][1], matlColors[i][2], matlColors[i][3] );

    m_pd3dDevice->SetRenderState( D3DRS_SHADEMODE, bSmoothShading ? D3DSHADE_GOURAUD : D3DSHADE_FLAT );

    ZeroMemory( &light, sizeof(light) );
    light.Type = D3DLIGHT_POINT;
    light.Diffuse.r = 0.7f;
    light.Diffuse.g = 0.7f;
    light.Diffuse.b = 0.7f;
    light.Diffuse.a = 1.0f;
    light.Specular.r = 1.0f;
    light.Specular.g = 1.0f;
    light.Specular.b = 1.0f;
    light.Specular.a = 1.0f;
    light.Ambient.r = 0.21f;
    light.Ambient.g = 0.21f;
    light.Ambient.b = 0.21f;
    light.Ambient.a = 1.0f;
    light.Position.x = 100.0f;
    light.Position.y = 100.0f;
    light.Position.z = 100.0f;
    light.Range = 1000.0f;
    light.Attenuation0 = 1.0f;
    light.Attenuation1 = 0.0f;
    light.Attenuation2 = 0.0f;
    m_pd3dDevice->SetLight( 0, &light );
    m_pd3dDevice->LightEnable( 0, TRUE );

    m_pd3dDevice->SetMaterial( &Material[0] );

    if( !(*initFuncs[Type])() )
        return FALSE;

    pfnUpdateScene = updateFuncs[Type];
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: updateDialogControls()
// Desc: Enables the settings of the style
//-----------------------------------------------------------------------------
static void updateDialogControls( HWND hDlg )
{
    CheckDlgButton( hDlg, DLG_SETUP_SMOOTH, bSmoothShading );
    CheckDlgButton( hDlg, DLG_SETUP_CYCLE, bColorCycle );

    EnableWindow( GetDlgItem( hDlg, DLG_SETUP_SMOOTH ),    gulStyleOptions[Type] & OPT_SMOOTH_SHADE );
    EnableWindow( GetDlgItem( hDlg, DLG_SETUP_CYCLE ),     gulStyleOptions[Type] & OPT_COLOR_CYCLE );

    EnableWindow( GetDlgItem( hDlg, DLG_SETUP_TESSEL ),    gulStyleOptions[Type] & OPT_TESSEL );
    EnableWindow( GetDlgItem( hDlg, IDC_STATIC_TESS ),     gulStyleOptions[Type] & OPT_TESSEL );
    EnableWindow( GetDlgItem( hDlg, IDC_STATIC_TESS_MIN ), gulStyleOptions[Type] & OPT_TESSEL );
    EnableWindow( GetDlgItem( hDlg, IDC_STATIC_TESS_MAX ), gulStyleOptions[Type] & OPT_TESSEL );

    EnableWindow( GetDlgItem( hDlg, DLG_SETUP_SIZE ),      gulStyleOptions[Type] & OPT_SIZE );
    EnableWindow( GetDlgItem( hDlg, IDC_STATIC_SIZE ),     gulStyleOptions[Type] & OPT_SIZE );
    EnableWindow( GetDlgItem( hDlg, IDC_STATIC_SIZE_MIN ), gulStyleOptions[Type] & OPT_SIZE );
    EnableWindow( GetDlgItem( hDlg, IDC_STATIC_SIZE_MAX ), gulStyleOptions[Type] & OPT_SIZE );

    EnableWindow( GetDlgItem( hDlg, DLG_SETUP_TEXTURE ),   gulStyleOptions[Type] & OPT_TEXTURE );
}




//-----------------------------------------------------------------------------
// Name: CFlyingObjectsScreensaver()
// Desc:
//-----------------------------------------------------------------------------
CFlyingObjectsScreensaver::CFlyingObjectsScreensaver()
{
    g_pFloatRect = &m_floatrect;
    g_pMyFlyingObjectsScreensaver = this;

    ZeroMemory( m_DeviceObjectsArray, sizeof(m_DeviceObjectsArray) );
    m_pDeviceObjects = NULL;

    LoadString( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );
    m_bUseDepthBuffer = TRUE;

    lstrcpy( m_strRegPath, TEXT("Software\\Microsoft\\Screensavers\\Flying Objects") );

    m_floatrect.xSize = 0.0f;   // (the first frame sets the window up)

    InitCommonControls();

    bSmoothShading = FALSE;
    bColorCycle = FALSE;
    gulUpdateFlags = 0;
    Type = 0;
    fTesselFact = 2.0f;
    uSize = 50;
    ss_GetDefaultBmpFile( gTexFile.szPathName );
    gTexFile.nOffset = 0;
    ss_LoadTextureResourceStrings();

    srand( (unsigned) time( NULL ) );
}




//-----------------------------------------------------------------------------
// Name: RegisterSoftwareDevice()
// Desc: This can register the D3D8RGBRasterizer or any other
//       pluggable software rasterizer.
//-----------------------------------------------------------------------------
#ifndef NO_D3D8RGB
extern "C" void* D3D8RGBRasterizer;
#endif

HRESULT CFlyingObjectsScreensaver::RegisterSoftwareDevice()
{
#ifndef NO_D3D8RGB
    m_pD3D->RegisterSoftwareDevice( D3D8RGBRasterizer );
#endif

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Moves the window of the object around the screen (or back to where
//       it started)
//-----------------------------------------------------------------------------
HRESULT CFlyingObjectsScreensaver::FrameMove()
{
    FLOAT fOldX, fOldY;
    RECT rc;

    if( m_floatrect.xSize == 0.0f )
    {
        // First frame: a square, uSize percent of the way from a quarter
        // to (almost) all of the smaller side of the screen(s), centred
        FLOAT fSize = uSize * (1.0f / 150.0f) * 0.7f + 0.25f;
        DWORD dwWidth, dwHeight, dwSize;

        rc = m_rcRenderTotal;
        dwWidth = rc.right - rc.left;
        dwHeight = rc.bottom - rc.top;
        dwSize = (DWORD) (min( dwWidth, dwHeight ) * fSize);

        if( dwSize > dwWidth )
            dwSize = dwWidth;
        if( dwSize > dwHeight )
            dwSize = dwHeight;

        // In full screen mode, start in the middle of the first screen
        if( !m_bWindowed )
            rc = m_Monitors[m_RenderUnits[0].iMonitor].rcScreen;

        m_floatrect.xMin = rc.left + (DWORD) (rc.right - dwSize - rc.left) * 0.5f;
        m_floatrect.yMin = rc.top + (DWORD) (rc.bottom - dwSize - rc.top) * 0.5f;
        m_floatrect.xOrigin = m_floatrect.xMin;
        m_floatrect.yOrigin = m_floatrect.yMin;
        m_floatrect.xSize = (FLOAT) dwSize;
        m_floatrect.ySize = (FLOAT) dwSize;

        m_floatrect.xVel = 0.01f * (FLOAT) dwSize;
        if( rand() % 2 == 0 )
            m_floatrect.xVel = -m_floatrect.xVel;

        m_floatrect.yVel = 0.01f * (FLOAT) dwSize;
        if( rand() % 2 == 0 )
            m_floatrect.yVel = -m_floatrect.yVel;

        m_floatrect.xVelMax = m_floatrect.xVel;
        m_floatrect.yVelMax = m_floatrect.yVel;
    }

    fOldX = m_floatrect.xMin;
    fOldY = m_floatrect.yMin;
    rc = m_rcRenderTotal;

    if( !gbReturnToOrigin )
    {
        // Bounce around the screen
        gbAtOrigin = FALSE;

        if( m_floatrect.xVel == 0.0f )
        {
            m_floatrect.xVel = m_floatrect.xVelMax;
            if( rand() % 2 == 0 )
                m_floatrect.xVel = -m_floatrect.xVel;
        }
        if( m_floatrect.yVel == 0.0f )
        {
            m_floatrect.yVel = m_floatrect.yVelMax;
            if( rand() % 2 == 0 )
                m_floatrect.yVel = -m_floatrect.yVel;
        }

        m_floatrect.xMin += m_floatrect.xVel * m_fElapsedTime * 20.0f;
        m_floatrect.yMin += m_floatrect.yVel * m_fElapsedTime * 20.0f;

        if( (m_floatrect.xVel < 0.0f && m_floatrect.xMin < rc.left) ||
            (m_floatrect.xVel > 0.0f && m_floatrect.xMin + m_floatrect.xSize > rc.right) )
        {
            gbBounced = TRUE;
            m_floatrect.xMin = fOldX;
            m_floatrect.xVel = -m_floatrect.xVel;
        }
        if( (m_floatrect.yVel < 0.0f && m_floatrect.yMin < rc.top) ||
            (m_floatrect.yVel > 0.0f && m_floatrect.yMin + m_floatrect.ySize > rc.bottom) )
        {
            gbBounced = TRUE;
            m_floatrect.yMin = fOldY;
            m_floatrect.yVel = -m_floatrect.yVel;
        }
    }
    else
    {
        // Go back to where it started
        if( (m_floatrect.xVel < 0.0f && m_floatrect.xMin < m_floatrect.xOrigin) ||
            (m_floatrect.xVel > 0.0f && m_floatrect.xMin > m_floatrect.xOrigin) )
        {
            m_floatrect.xVel = -m_floatrect.xVel;
        }
        if( (m_floatrect.yVel < 0.0f && m_floatrect.yMin < m_floatrect.yOrigin) ||
            (m_floatrect.yVel > 0.0f && m_floatrect.yMin > m_floatrect.yOrigin) )
        {
            m_floatrect.yVel = -m_floatrect.yVel;
        }

        m_floatrect.xMin += m_floatrect.xVel * m_fElapsedTime * 20.0f;
        m_floatrect.yMin += m_floatrect.yVel * m_fElapsedTime * 20.0f;

        if( (m_floatrect.xVel < 0.0f && m_floatrect.xMin < m_floatrect.xOrigin) ||
            (m_floatrect.xVel > 0.0f && m_floatrect.xMin > m_floatrect.xOrigin) )
        {
            m_floatrect.xVel = 0.0f;
            m_floatrect.xMin = m_floatrect.xOrigin;
        }
        if( (m_floatrect.yVel < 0.0f && m_floatrect.yMin < m_floatrect.yOrigin) ||
            (m_floatrect.yVel > 0.0f && m_floatrect.yMin > m_floatrect.yOrigin) )
        {
            m_floatrect.yVel = 0.0f;
            m_floatrect.yMin = m_floatrect.yOrigin;
        }

        if( m_floatrect.xMin == m_floatrect.xOrigin && m_floatrect.yMin == m_floatrect.yOrigin )
            gbAtOrigin = TRUE;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetProjectionParams()
// Desc: The projection of the style: glOrtho or glFrustum of the OpenGL
//       version (width and height of the view at the near plane)
//-----------------------------------------------------------------------------
void SetProjectionParams( BOOL bOrtho, FLOAT fWidth, FLOAT fHeight, FLOAT fNear, FLOAT fFar )
{
    g_pMyFlyingObjectsScreensaver->m_bOrtho = bOrtho;
    g_pMyFlyingObjectsScreensaver->m_fProjWidth = fWidth;
    g_pMyFlyingObjectsScreensaver->m_fProjHeight = fHeight;
    g_pMyFlyingObjectsScreensaver->m_fProjNear = fNear;
    g_pMyFlyingObjectsScreensaver->m_fProjFar = fFar;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CFlyingObjectsScreensaver::Render()
{
    static FLOAT s_fLastTime;
    D3DVIEWPORT8 vp;
    RECT rcView;        // window of the object, in screen coordinates
    RECT rcVisible;     // the part of it on this device's screen
    D3DXMATRIX matProj;
    FLOAT fElapsedTime;

    // Clear the whole render target
    vp.X = 0;
    vp.Y = 0;
    vp.Width = m_rcRenderCurDevice.right - m_rcRenderCurDevice.left;
    vp.Height = m_rcRenderCurDevice.bottom - m_rcRenderCurDevice.top;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0L );

    rcView.left = (INT) m_floatrect.xMin;
    rcView.top = (INT) m_floatrect.yMin;
    rcView.right = rcView.left + (INT) m_floatrect.xSize;
    rcView.bottom = rcView.top + (INT) m_floatrect.ySize;

    if( !IntersectRect( &rcVisible, &rcView, &m_rcRenderCurDevice ) )
        return S_OK;

    // Device coordinates
    OffsetRect( &rcView, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );
    OffsetRect( &rcVisible, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );

    vp.X = rcVisible.left;
    vp.Y = rcVisible.top;
    vp.Width = rcVisible.right - rcVisible.left;
    vp.Height = rcVisible.bottom - rcVisible.top;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

    // Project onto the visible part of the window only, so that a window
    // that straddles two screens is drawn in two pieces
    FLOAT xc = (rcView.left + rcView.right) * 0.5f;
    FLOAT yc = (rcView.top + rcView.bottom) * 0.5f;
    FLOAT l = m_fProjWidth * -0.5f * ((rcVisible.left - xc) / (rcView.left - xc));
    FLOAT r = m_fProjWidth * 0.5f * ((rcVisible.right - xc) / (rcView.right - xc));
    FLOAT t = m_fProjHeight * 0.5f * ((rcVisible.top - yc) / (rcView.top - yc));
    FLOAT b = m_fProjHeight * -0.5f * ((rcVisible.bottom - yc) / (rcView.bottom - yc));

    if( m_bOrtho )
        D3DXMatrixOrthoOffCenterLH( &matProj, l, r, b, t, m_fProjNear, m_fProjFar );
    else
        D3DXMatrixPerspectiveOffCenterLH( &matProj, l, r, b, t, m_fProjNear, m_fProjFar );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Only the first screen of a frame moves the object
    if( m_fTime != s_fLastTime )
        fElapsedTime = m_fElapsedTime;
    else
        fElapsedTime = 0.0f;
    s_fLastTime = m_fTime;

    if( SUCCEEDED( m_pd3dDevice->BeginScene() ) )
    {
        g_pd3dDevice = m_pd3dDevice;
        g_pIB = m_pDeviceObjects->m_pIB;
        g_pVB = m_pDeviceObjects->m_pVB;
        g_pVBTex = m_pDeviceObjects->m_pVBTex;

        (*pfnUpdateScene)( gulUpdateFlags, fElapsedTime );

        m_pd3dDevice->EndScene();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDevice()
// Desc:
//-----------------------------------------------------------------------------
VOID CFlyingObjectsScreensaver::SetDevice( UINT iDevice )
{
    g_pDeviceObjects = &m_DeviceObjectsArray[iDevice];
    m_pDeviceObjects = g_pDeviceObjects;

    INT iMonitor = m_RenderUnits[iDevice].iMonitor;
    g_ptMonitorOrigin.x = m_Monitors[iMonitor].rcScreen.left;
    g_ptMonitorOrigin.y = m_Monitors[iMonitor].rcScreen.top;

    g_iDevice = iDevice;
}




//-----------------------------------------------------------------------------
// Name: LoadTextureFromResource()
// Desc: Creates a texture from a picture in the resources of the screen
//       saver.
//-----------------------------------------------------------------------------
static HRESULT LoadTextureFromResource( LPDIRECT3DDEVICE8 pd3dDevice, TCHAR* strRes,
                                        TCHAR* strResType, LPDIRECT3DTEXTURE8* ppTex )
{
    HRESULT hr;
    HMODULE hModule = NULL;
    HRSRC rsrc;
    HGLOBAL hgData;
    LPVOID pvData;
    DWORD cbData;

    rsrc = FindResource( hModule, strRes, strResType );
    if( rsrc != NULL )
    {
        cbData = SizeofResource( hModule, rsrc );
        if( cbData > 0 )
        {
            hgData = LoadResource( hModule, rsrc );
            if( hgData != NULL )
            {
                pvData = LockResource( hgData );
                if( pvData != NULL )
                {
                    if( FAILED( hr = D3DXCreateTextureFromFileInMemory( pd3dDevice,
                        pvData, cbData, ppTex ) ) )
                    {
                        return hr;
                    }
                }
            }
        }
    }

    if( *ppTex == NULL )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CFlyingObjectsScreensaver::RestoreDeviceObjects()
{
    D3DMATERIAL8 mtrl;

    if( m_pd3dDevice == NULL )
        return S_OK;

    g_pd3dDevice = m_pd3dDevice;

    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x20202020 );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    ZeroMemory( &mtrl, sizeof(mtrl) );
    mtrl.Diffuse.r = mtrl.Ambient.r = 1.0f;
    mtrl.Diffuse.g = mtrl.Ambient.g = 1.0f;
    mtrl.Diffuse.b = mtrl.Ambient.b = 1.0f;
    mtrl.Diffuse.a = mtrl.Ambient.a = 1.0f;
    m_pd3dDevice->SetMaterial( &mtrl );

    if( !InitScene() )
        return E_FAIL;

    // The pictures of the style
    if( Type == STYLE_FLAG )
    {
        if( FAILED( D3DXCreateTextureFromFile( m_pd3dDevice, gTexFile.szPathName,
                                               &m_pDeviceObjects->m_pTexture ) ) )
        {
            LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCE( IDR_FLAG_JPG ), TEXT("JPG"),
                                     &m_pDeviceObjects->m_pTexture );
        }
    }
    else if( Type == STYLE_WINLOGO )
    {
        LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCE( IDR_LOGO_DDS ), TEXT("DDS"),
                                 &m_pDeviceObjects->m_pTexture );
        LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCE( IDR_LOGO_PNG ), TEXT("PNG"),
                                 &m_pDeviceObjects->m_pTexture2 );
    }

    m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->m_pTexture );

    // The buffers the styles draw with
    if( FAILED( m_pd3dDevice->CreateIndexBuffer( INDEX_BUFFER_SIZE, D3DUSAGE_WRITEONLY,
                                                 D3DFMT_INDEX16, D3DPOOL_MANAGED,
                                                 &m_pDeviceObjects->m_pIB ) ) ||
        FAILED( m_pd3dDevice->CreateVertexBuffer( VERTEX_BUFFER_SIZE, D3DUSAGE_WRITEONLY,
                                                  D3DFVF_MESHVERTEX, D3DPOOL_MANAGED,
                                                  &m_pDeviceObjects->m_pVB ) ) ||
        FAILED( m_pd3dDevice->CreateVertexBuffer( TEXVERTEX_BUFFER_SIZE, D3DUSAGE_WRITEONLY,
                                                  D3DFVF_TEXVERTEX, D3DPOOL_MANAGED,
                                                  &m_pDeviceObjects->m_pVBTex ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CFlyingObjectsScreensaver::InvalidateDeviceObjects()
{
    SAFE_RELEASE( m_pDeviceObjects->m_pTexture );
    SAFE_RELEASE( m_pDeviceObjects->m_pTexture2 );
    SAFE_RELEASE( m_pDeviceObjects->m_pIB );
    SAFE_RELEASE( m_pDeviceObjects->m_pVB );
    SAFE_RELEASE( m_pDeviceObjects->m_pVBTex );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ss_RegistrySetup()
// Desc: Looks up the control.ini section of the OpenGL version of the
//       screen saver.  Returns TRUE if it exists.
//-----------------------------------------------------------------------------
BOOL CFlyingObjectsScreensaver::ss_RegistrySetup( int section, int file )
{
    TCHAR szSection[100];

    if( LoadString( m_hInstance, section, g_szSectName, BUF_SIZE ) &&
        LoadString( m_hInstance, file, g_szFname, BUF_SIZE ) )
    {
        if( GetPrivateProfileSection( g_szSectName, szSection, 100, g_szFname ) > 0 )
            return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ss_GetRegistryInt()
// Desc: Reads an integer from the control.ini section (0 if the string
//       resource with its name cannot be loaded).
//-----------------------------------------------------------------------------
int CFlyingObjectsScreensaver::ss_GetRegistryInt( int name, int iDefault )
{
    TCHAR szName[BUF_SIZE+1];

    if( LoadString( m_hInstance, name, szName, BUF_SIZE ) )
        return GetPrivateProfileInt( g_szSectName, szName, iDefault, g_szFname );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: ss_GetRegistryString()
// Desc: Reads a string from the control.ini section.
//-----------------------------------------------------------------------------
VOID CFlyingObjectsScreensaver::ss_GetRegistryString( int name, LPTSTR lpDefault, LPTSTR lpDest,
                                                      int bufSize )
{
    TCHAR szName[BUF_SIZE+1];

    if( LoadString( m_hInstance, name, szName, BUF_SIZE ) )
        GetPrivateProfileString( g_szSectName, szName, lpDefault, lpDest, bufSize, g_szFname );
}




//-----------------------------------------------------------------------------
// Name: ConfirmDevice()
// Desc: (The styles read device state back: no pure device)
//-----------------------------------------------------------------------------
HRESULT CFlyingObjectsScreensaver::ConfirmDevice( D3DCAPS8* pCaps, DWORD dwBehavior,
                                                  D3DFORMAT fmtBackBuffer )
{
    if( dwBehavior & D3DCREATE_PUREDEVICE )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetupTrackbar()
// Desc: Sets the range, position, page size and line size of a trackbar.
//-----------------------------------------------------------------------------
void SetupTrackbar( HWND hDlg, int item, int lo, int hi, int lineSize, int pageSize, int pos )
{
    SendDlgItemMessage( hDlg, item, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(lo, hi) );
    SendDlgItemMessage( hDlg, item, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)pos );
    SendDlgItemMessage( hDlg, item, TBM_SETPAGESIZE, (WPARAM)0, (LPARAM)pageSize );
    SendDlgItemMessage( hDlg, item, TBM_SETLINESIZE, (WPARAM)0, (LPARAM)lineSize );
}




//-----------------------------------------------------------------------------
// Name: GetTrackbarPos()
// Desc:
//-----------------------------------------------------------------------------
int GetTrackbarPos( HWND hDlg, int item )
{
    return (int)SendDlgItemMessage( hDlg, item, TBM_GETPOS, 0, 0 );
}




//-----------------------------------------------------------------------------
// Name: WriteSettings()
// Desc: Takes the settings from the dialog and saves them
//-----------------------------------------------------------------------------
VOID CFlyingObjectsScreensaver::WriteSettings( HWND hDlg )
{
    HKEY hkey;

    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ) )
    {
        bSmoothShading = IsDlgButtonChecked( hDlg, DLG_SETUP_SMOOTH );
        bColorCycle = IsDlgButtonChecked( hDlg, DLG_SETUP_CYCLE );
        DXUtil_WriteIntRegKey( hkey, TEXT("Options"), (bColorCycle << 1) | bSmoothShading );

        Type = (int) SendDlgItemMessage( hDlg, DLG_SETUP_TYPES, CB_GETCURSEL, 0, 0 );
        DXUtil_WriteIntRegKey( hkey, TEXT("Type"), Type );

        DXUtil_WriteIntRegKey( hkey, TEXT("Tesselation"), GetTrackbarPos( hDlg, DLG_SETUP_TESSEL ) );
        DXUtil_WriteIntRegKey( hkey, TEXT("Size"), GetTrackbarPos( hDlg, DLG_SETUP_SIZE ) );

        DXUtil_WriteStringRegKey( hkey, TEXT("Texture"), gTexFile.szPathName );
        DXUtil_WriteIntRegKey( hkey, TEXT("TextureFileOffset"), gTexFile.nOffset );

        WriteScreenSettings( hkey );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: initConfigDialog()
// Desc:
//-----------------------------------------------------------------------------
static void initConfigDialog( HWND hDlg )
{
    int pos;

    InitCommonControls();

    if( fTesselFact <= 1.0f )
        pos = (int) (fTesselFact * 100.0f);
    else
        pos = 100 - (int) ((fTesselFact - 1.0f) * -100.0f);

    SetupTrackbar( hDlg, DLG_SETUP_TESSEL, 0, 200, 1, 10, pos );
    SetupTrackbar( hDlg, DLG_SETUP_SIZE, 0, 100, 1, 10, uSize );

    updateDialogControls( hDlg );
}




//-----------------------------------------------------------------------------
// Name: ScreenSaverConfigureDialog()
// Desc:
//-----------------------------------------------------------------------------
INT_PTR CALLBACK CFlyingObjectsScreensaver::ScreenSaverConfigureDialog( HWND hDlg, UINT message,
                                                                        WPARAM wParam, LPARAM lParam )
{
    TCHAR szString[64];
    int i;

    switch( message )
    {
        case WM_INITDIALOG:
            g_pMyFlyingObjectsScreensaver->ReadSettings();
            initConfigDialog( hDlg );

            for( i = 0; i < NUM_STYLES; i++ )
            {
                LoadString( NULL, idsStyles[i], szString, 64 );
                SendDlgItemMessage( hDlg, DLG_SETUP_TYPES, CB_ADDSTRING, 0, (LPARAM) szString );
            }
            SendDlgItemMessage( hDlg, DLG_SETUP_TYPES, CB_SETCURSEL, Type, 0 );
            return TRUE;

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDOK:
                    g_pMyFlyingObjectsScreensaver->WriteSettings( hDlg );
                    EndDialog( hDlg, TRUE );
                    return TRUE;

                case IDCANCEL:
                    EndDialog( hDlg, FALSE );
                    return TRUE;

                case DLG_SETUP_TYPES:
                    if( HIWORD( wParam ) == CBN_SELCHANGE || HIWORD( wParam ) == CBN_EDITCHANGE )
                    {
                        Type = (int) SendDlgItemMessage( hDlg, DLG_SETUP_TYPES, CB_GETCURSEL, 0, 0 );
                        updateDialogControls( hDlg );
                    }
                    break;

                case DLG_SETUP_TEXTURE:
                    ss_SelectTextureFile( hDlg, &gTexFile );
                    return TRUE;

                case IDC_SCREENSETTINGS:
                    g_pMyFlyingObjectsScreensaver->DoScreenSettingsDialog( hDlg );
                    return TRUE;
            }
            break;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT )
{
    HRESULT hr;
    CFlyingObjectsScreensaver flyingObjectsSS;

    if( FAILED( hr = flyingObjectsSS.Create( hInst ) ) )
    {
        flyingObjectsSS.DisplayErrorMsg( hr );
        return 0;
    }

    return flyingObjectsSS.Run();
}




//-----------------------------------------------------------------------------
// Name: ss_ReadSettings()
// Desc: Reads the settings of the OpenGL version (control.ini)
//-----------------------------------------------------------------------------
VOID CFlyingObjectsScreensaver::ss_ReadSettings()
{
    int options;
    int tessel;
    TCHAR szDefaultBitmap[MAX_PATH];

    if( !ss_RegistrySetup( IDS_SAVERNAME, IDS_INIFILE ) )
        return;

    options = ss_GetRegistryInt( IDS_OPTIONS, -1 );
    if( options >= 0 )
    {
        bColorCycle = (options >> 1) & 1;
        bSmoothShading = options & 1;
        gulUpdateFlags = bColorCycle * MF_CYCLE_COLORS;
    }

    Type = ss_GetRegistryInt( IDS_OBJTYPE, 0 );
    if( Type > STYLE_TWIST )
        Type = STYLE_FLAG;
    if( Type == STYLE_2RIBBONS )
        gulUpdateFlags |= MF_TWO_RIBBONS;

    // 0 .. 100 -> 0.0 .. 1.0, 100 .. 200 -> 1.0 .. 2.0
    tessel = ss_GetRegistryInt( IDS_TESSELATION, 100 );
    if( tessel < 0 )
        tessel = 0;
    else if( tessel > 200 )
        tessel = 200;

    if( tessel <= 100 )
        fTesselFact = (FLOAT) tessel * 0.01f;
    else
        fTesselFact = ((FLOAT) tessel - 100.0f) * 0.01f + 1.0f;

    uSize = ss_GetRegistryInt( IDS_SIZE, 50 );
    if( uSize > 100 )
        uSize = 100;

    ss_GetDefaultBmpFile( szDefaultBitmap );
    ss_GetRegistryString( IDS_TEXTURE, szDefaultBitmap, gTexFile.szPathName, MAX_PATH );
    gTexFile.nOffset = ss_GetRegistryInt( IDS_TEXTURE_FILE_OFFSET, 0 );
}




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc:
//-----------------------------------------------------------------------------
VOID CFlyingObjectsScreensaver::DoConfig()
{
    DialogBox( NULL, MAKEINTRESOURCE( DLG_SCRNSAVECONFIGURE ), m_hWndParent,
               ScreenSaverConfigureDialog );
}




//-----------------------------------------------------------------------------
// Name: ReadSettings()
// Desc:
//-----------------------------------------------------------------------------
VOID CFlyingObjectsScreensaver::ReadSettings()
{
    HKEY hkey;
    DWORD dwOptions;
    int tessel = 0;

    // Read the OpenGL settings first, so OS upgrade cases work
    ss_ReadSettings();

    if( ERROR_SUCCESS != RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ) )
    {
        return;
    }

    dwOptions = (bColorCycle << 1) | bSmoothShading;
    DXUtil_ReadIntRegKey( hkey, TEXT("Options"), &dwOptions, dwOptions );
    if( (int) dwOptions >= 0 )
    {
        bColorCycle = (dwOptions >> 1) & 1;
        bSmoothShading = dwOptions & 1;
        gulUpdateFlags = bColorCycle * MF_CYCLE_COLORS;
    }

    DXUtil_ReadIntRegKey( hkey, TEXT("Type"), (DWORD*) &Type, Type );
    if( Type > STYLE_TWIST )
        Type = STYLE_FLAG;
    if( Type == STYLE_2RIBBONS )
        gulUpdateFlags |= MF_TWO_RIBBONS;

    tessel = (int) (fTesselFact * 100.0f);
    DXUtil_ReadIntRegKey( hkey, TEXT("Tesselation"), (DWORD*) &tessel, tessel );
    if( tessel < 0 )
        tessel = 0;
    else if( tessel > 200 )
        tessel = 200;

    if( tessel <= 100 )
        fTesselFact = (FLOAT) tessel * 0.01f;
    else
        fTesselFact = ((FLOAT) tessel - 100.0f) * 0.01f + 1.0f;

    DXUtil_ReadIntRegKey( hkey, TEXT("Size"), (DWORD*) &uSize, uSize );
    if( uSize > 100 )
        uSize = 100;

    // The logo has its own size and shading
    if( Type == STYLE_WINLOGO )
    {
        uSize = 75;
        bSmoothShading = TRUE;
    }

    DXUtil_ReadStringRegKey( hkey, TEXT("Texture"), gTexFile.szPathName, MAX_PATH,
                             gTexFile.szPathName );
    DXUtil_ReadIntRegKey( hkey, TEXT("TextureFileOffset"), (DWORD*) &gTexFile.nOffset,
                          gTexFile.nOffset );

    ReadScreenSettings( hkey );

    RegCloseKey( hkey );
}
