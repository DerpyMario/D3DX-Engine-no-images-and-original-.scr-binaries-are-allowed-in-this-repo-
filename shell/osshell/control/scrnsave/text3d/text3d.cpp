//-----------------------------------------------------------------------------
// File: text3d.cpp
//
// Desc: The 3D Text screen saver (ssText3d.scr) of Windows Vista SP1.
//
//       A string, or the time of day, is turned into an extruded mesh
//       (Shape3D.lib) which rotates in one of several ways and - unless it
//       just spins - bounces around the desktop.  It is lit by one directional
//       light and is either solid coloured, textured, or reflects an
//       environment map.
//
//       Reconstructed from ssText3d.scr 6.0.6001.18000 (amd64).  See text3d.h
//       for the naming conventions.
//
//       Original object:
//           shell\osshell\control\scrnsave\text3d\objfre\amd64\text3d.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <usp10.h>
#include <wincodec.h>
#include <strsafe.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "d3dsaver9.h"
#include "dxutil9.h"
#include "texture.h"
#include "shape3d.h"
#include "winbrand.h"
#include "text3d.h"
#include "resource.h"

#define BUFFER_SIZE             255

// The square the text lives in bounces with this many of its own speed units
// per second, in steps of a sixtieth of a second.
#define FLOATRECT_SPEED         20.0f
#define TIME_STEP               ( 1.0f / 60.0f )

#define TEXT_EXTRUSION          0.5f

// Where the OpenGL version of this screen saver kept its settings
WCHAR                   g_szSectName[BUFFER_SIZE + 1];
WCHAR                   g_szFname[BUFFER_SIZE + 1];

CTextScreensaver*       g_pMyTextScreensaver = NULL;




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
    HRESULT hr;

    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );

    CTextScreensaver textSS;

    if( FAILED( hr = textSS.Create( hInstance ) ) )
    {
        textSS.DisplayErrorMsg( hr );
        return 0;
    }

    return textSS.Run();
}




//-----------------------------------------------------------------------------
// Name: GetDefaultDisplayString()
// Desc: The text that is shown until the user chooses another one: the short
//       product name ("Windows Vista").
//-----------------------------------------------------------------------------
static inline VOID GetDefaultDisplayString( WCHAR* pszText, size_t cchText )
{
    LPWSTR pszProduct = BrandingFormatString( L"%WINDOWS_SHORT%" );

    if( pszProduct != NULL )
    {
        StringCchCopyW( pszText, cchText, pszProduct );
        GlobalFree( pszProduct );
    }
    else
    {
        LoadStringW( NULL, IDS_DEFAULT_TEXT, pszText, (INT)cchText );
    }
}




//-----------------------------------------------------------------------------
// Name: CTextScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
CTextScreensaver::CTextScreensaver()
{
    m_hFont = NULL;
    m_fAngleX = 0.0f;
    m_fAngleY = 0.0f;
    m_fAngleZ = 0.0f;
    m_dwMeshUpdateCounter = 0;

    g_pMyTextScreensaver = this;

    LoadStringW( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );
    InitCommonControls();

    ZeroMemory( m_DeviceObjectsArray, sizeof(m_DeviceObjectsArray) );

    m_bUseDepthBuffer = TRUE;
    m_dwMinDepthBits = 16;

    // FrameMove() sets the rectangle up when it runs for the first time
    m_floatrect.xSize = 0.0f;

    StringCchCopyW( m_strRegPath, 200, L"Software\\Microsoft\\Windows\\CurrentVersion\\Screensavers\\ssText3D" );

    srand( (UINT)time( NULL ) );
}




//-----------------------------------------------------------------------------
// Name: RegisterSoftwareDevice()
// Desc: This can register the D3D8RGBRasterizer or any other
//       pluggable software rasterizer.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::RegisterSoftwareDevice()
{
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::FrameMove()
{
    DWORD dwTick = GetTickCount();
    DWORD dwLastTick = m_dwLastTick;

    m_dwLastTick = dwTick;

    if( m_floatrect.xSize == 0.0f )
    {
        // Initialize the floatrect: a square whose size depends on the "Size"
        // setting (25% .. 100% of the smaller side of the desktop) ...
        RECT rcBounds = m_rcRenderTotal;
        INT  nWidth = rcBounds.right - rcBounds.left;
        INT  nHeight = rcBounds.bottom - rcBounds.top;
        INT  nSize = nWidth;

        if( nHeight < nSize )
            nSize = nHeight;

        nSize = (INT)( ( (FLOAT)m_dwSize / 10.0f * 0.75f + 0.25f ) * (FLOAT)nSize );

        if( nSize > nWidth )
            nSize = nWidth;
        if( nSize > nHeight )
            nSize = nHeight;

        // ... which starts in the middle of the first monitor
        if( !m_bWindowed )
            rcBounds = m_Monitors[m_RenderUnits[0].iMonitor].rcScreen;

        m_floatrect.xSize = (FLOAT)nSize;
        m_floatrect.xMin = (FLOAT)( rcBounds.right - nSize - rcBounds.left ) * 0.5f + (FLOAT)rcBounds.left;
        m_floatrect.ySize = (FLOAT)nSize;
        m_floatrect.xVel = (FLOAT)nSize * 0.01f;
        m_floatrect.yMin = (FLOAT)( rcBounds.bottom - rcBounds.top - nSize ) * 0.5f + (FLOAT)rcBounds.top;

        if( rand() % 2 == 0 )
            m_floatrect.xVel = -m_floatrect.xVel;

        m_floatrect.yVel = (FLOAT)nSize * 0.01f;

        if( rand() % 2 == 0 )
            m_floatrect.yVel = -m_floatrect.yVel;
    }

    // Spinning text stays where it is; everything else bounces off the edges
    // of the desktop.
    FLOAT xMinOld = m_floatrect.xMin;
    FLOAT yMinOld = m_floatrect.yMin;

    if( m_RotType != spin )
    {
        FLOAT xVel = m_floatrect.xVel;
        FLOAT yVel = m_floatrect.yVel;

        m_floatrect.xMin = xVel * FLOATRECT_SPEED * TIME_STEP + xMinOld;
        m_floatrect.yMin = yVel * FLOATRECT_SPEED * TIME_STEP + yMinOld;

        if( ( xVel < 0.0f && m_floatrect.xMin < (FLOAT)m_rcRenderTotal.left ) ||
            ( xVel > 0.0f && m_floatrect.xMin + m_floatrect.xSize > (FLOAT)m_rcRenderTotal.right ) )
        {
            m_floatrect.xMin = xMinOld;         // undo the move
            m_floatrect.xVel = -xVel;           // change direction
        }

        if( ( yVel < 0.0f && m_floatrect.yMin < (FLOAT)m_rcRenderTotal.top ) ||
            ( yVel > 0.0f && m_floatrect.yMin + m_floatrect.ySize > (FLOAT)m_rcRenderTotal.bottom ) )
        {
            m_floatrect.yMin = yMinOld;
            m_floatrect.yVel = -yVel;
        }
    }

    UpdateAngles( dwTick - dwLastTick );

    if( m_bUseTime )
    {
        if( UpdateTimeString( m_szDisplayString, MAX_DISPLAY_STRING + 1 ) )
            m_dwMeshUpdateCounter++;            // tells Render() that the mesh is out of date
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::Render()
{
    D3DVIEWPORT9 vp;

    // First, clear the entire back buffer to the background color
    vp.X = 0;
    vp.Y = 0;
    vp.Width = m_rcRenderCurDevice.right - m_rcRenderCurDevice.left;
    vp.Height = m_rcRenderCurDevice.bottom - m_rcRenderCurDevice.top;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vp );
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0L );

    // Now determine what part of the floatrect, if any, intersects the current screen
    RECT rcFloatThisScreen;
    RECT rcFloatThisScreenClipped;

    rcFloatThisScreen.left = (INT)m_floatrect.xMin;
    rcFloatThisScreen.top = (INT)m_floatrect.yMin;
    rcFloatThisScreen.right = rcFloatThisScreen.left + (INT)m_floatrect.xSize;
    rcFloatThisScreen.bottom = rcFloatThisScreen.top + (INT)m_floatrect.ySize;

    if( !IntersectRect( &rcFloatThisScreenClipped, &rcFloatThisScreen, &m_rcRenderCurDevice ) )
        return S_OK;    // no intersection, so nothing further to render on this screen

    // Convert rcFloatThisScreen from screen to window coordinates
    OffsetRect( &rcFloatThisScreen, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );
    OffsetRect( &rcFloatThisScreenClipped, -m_rcRenderCurDevice.left, -m_rcRenderCurDevice.top );

    // Now set up the viewport to render to the clipped rect
    vp.X = rcFloatThisScreenClipped.left;
    vp.Y = rcFloatThisScreenClipped.top;
    vp.Width = rcFloatThisScreenClipped.right - rcFloatThisScreenClipped.left;
    vp.Height = rcFloatThisScreenClipped.bottom - rcFloatThisScreenClipped.top;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &vp );

    // Now set up the projection matrix to only render the onscreen part of the
    // rect to the viewport
    D3DXMATRIX matProj;
    FLOAT fCenterX = (FLOAT)( rcFloatThisScreen.right + rcFloatThisScreen.left ) * 0.5f;
    FLOAT fCenterY = (FLOAT)( rcFloatThisScreen.bottom + rcFloatThisScreen.top ) * 0.5f;
    FLOAT l = ( (FLOAT)rcFloatThisScreenClipped.left - fCenterX ) / ( (FLOAT)rcFloatThisScreen.left - fCenterX ) * -0.8f;
    FLOAT r = ( (FLOAT)rcFloatThisScreenClipped.right - fCenterX ) / ( (FLOAT)rcFloatThisScreen.right - fCenterX ) * 0.8f;
    FLOAT b = ( (FLOAT)rcFloatThisScreenClipped.top - fCenterY ) / ( (FLOAT)rcFloatThisScreen.top - fCenterY ) * -0.8f;
    FLOAT t = ( (FLOAT)rcFloatThisScreenClipped.bottom - fCenterY ) / ( (FLOAT)rcFloatThisScreen.bottom - fCenterY ) * 0.8f;

    D3DXMatrixPerspectiveOffCenterLH( &matProj, l, r, b, t, 1.0f, 50.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Mesh updating happens in Render() instead of FrameMove() since there is
    // one mesh per device
    if( m_pDeviceObjects->m_dwMeshUpdateCounter != m_dwMeshUpdateCounter )
    {
        BuildTextMesh( m_szDisplayString );
        m_pDeviceObjects->m_dwMeshUpdateCounter = m_dwMeshUpdateCounter;
    }

    m_pd3dDevice->BeginScene();

    // Set world matrix: centre the text, rotate it, and move it far enough
    // away for its width
    D3DXMATRIX matTrans, matRotX, matRotY, matRotZ, matTrans2;

    D3DXMatrixTranslation( &matTrans, m_fTextOffsetX, m_fTextOffsetY, 0.25f );
    D3DXMatrixRotationX( &matRotX, m_fAngleX );
    D3DXMatrixRotationY( &matRotY, m_fAngleY );
    D3DXMatrixRotationZ( &matRotZ, m_fAngleZ );
    D3DXMatrixTranslation( &matTrans2, 0.0f, 0.0f, m_fTextMaxX - m_fTextMinX + 1.5f );

    m_matWorld = matTrans * matRotX * matRotY * matRotZ * matTrans2;
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );

    SetPerFrameStates();

    // Draw mesh
    m_pDeviceObjects->m_pMesh->DrawSubset( 0 );

    m_pd3dDevice->EndScene();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Restore device-memory objects and state after a device is created or
//       resized.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::RestoreDeviceObjects()
{
    HRESULT hr;

    // Set up view matrix: at the origin, looking down +z
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 1.0f );
    D3DXVECTOR3 vEye( 0.0f, 0.0f, 0.0f );

    D3DXMatrixLookAtLH( &m_matView, &vEye, &vAt, &vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    // Set up misc. render states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, m_bSpecular );

    // The mesh code works in units of the em square anyway; the height only
    // has to be something sensible while the font is created.
    LONG lHeight = m_Font.lfHeight;
    m_Font.lfHeight = 100;
    m_hFont = CreateFontIndirectW( &m_Font );
    m_Font.lfHeight = lHeight;

    if( m_hFont == NULL )
        return E_FAIL;

    // Initialize the display string if it shows the time
    if( m_bUseTime )
        UpdateTimeString( m_szDisplayString, MAX_DISPLAY_STRING + 1 );

    if( FAILED( hr = BuildTextMesh( m_szDisplayString ) ) )
        return hr;

    // Rotate around the middle of the text
    m_fTextOffsetX = ( m_fTextMaxX + m_fTextMinX ) * -0.5f;
    m_fTextOffsetY = ( m_fTextMaxY + m_fTextMinY ) * -0.5f;

    // Load the texture
    switch( m_SurfType )
    {
        case color:
            m_pDeviceObjects->m_pTexture = NULL;
            break;

        case environment:
            if( m_bUseCustomEnvironment )
                m_pDeviceObjects->m_pTexture = CreateTextureFromFile( m_szCustomEnvironment );

            if( m_pDeviceObjects->m_pTexture == NULL )
            {
                Texture::LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCEW( IDR_ENVIRONMENT ), L"JPG",
                                                  0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8,
                                                  &m_pDeviceObjects->m_pTexture );
            }
            break;

        case texture:
            if( m_bUseCustomTexture )
                m_pDeviceObjects->m_pTexture = CreateTextureFromFile( m_szCustomTexture );

            if( m_pDeviceObjects->m_pTexture == NULL )
            {
                Texture::LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCEW( IDR_TEXTURE ), L"JPG",
                                                  0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8,
                                                  &m_pDeviceObjects->m_pTexture );
            }
            break;

        default:
            return E_FAIL;
    }

    // Set per-frame states
    SetPerFrameStates();

    m_dwLastTick = GetTickCount();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetPerFrameStates()
// Desc: The render states that depend on the settings: texture, light,
//       material and texture coordinate generation.
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SetPerFrameStates()
{
    m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->m_pTexture );

    // Set up texture pipeline
    if( m_SurfType == color )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    }
    else
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    }
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    // Set up lighting
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, m_bSpecular ? 0x00464646 : 0x00646464 );

    D3DLIGHT9 light;
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 0.0f );
    light.Specular = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f );
    light.Ambient = D3DXCOLOR( 0.0f, 0.0f, 0.0f, 0.0f );
    light.Position = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    light.Direction = D3DXVECTOR3( 0.5f, -0.5f, 1.0f );
    light.Range = 10000.0f;
    light.Falloff = 0.0f;
    light.Attenuation0 = 0.0f;
    light.Attenuation1 = 0.0f;
    light.Attenuation2 = 0.0f;
    light.Theta = 0.0f;
    light.Phi = 0.0f;
    m_pd3dDevice->SetLight( 0, &light );
    m_pd3dDevice->LightEnable( 0, TRUE );

    // Set up material
    D3DMATERIAL9 mtrl;

    if( m_SurfType == color )
    {
        COLORREF cr = m_bUseCustomColor ? m_SurfaceColor : RGB( 119, 119, 119 );

        mtrl.Diffuse.r = (FLOAT)GetRValue( cr ) / 255.0f;
        mtrl.Diffuse.g = (FLOAT)GetGValue( cr ) / 255.0f;
        mtrl.Diffuse.b = (FLOAT)GetBValue( cr ) / 255.0f;
        mtrl.Diffuse.a = 0.0f;      // (left uninitialized in the original)
    }
    else
    {
        mtrl.Diffuse = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 0.0f );
    }
    mtrl.Ambient = mtrl.Diffuse;
    mtrl.Specular = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 0.0f );
    mtrl.Power = 5.0f;
    mtrl.Emissive = D3DXCOLOR( 0.0f, 0.0f, 0.0f, 0.0f );
    m_pd3dDevice->SetMaterial( &mtrl );

    // Set up texture coordinate generation
    if( m_SurfType == environment )
    {
        // sphere map: the reflection vector, squeezed into 0 .. 1
        D3DXMATRIX matTex;

        ZeroMemory( &matTex, sizeof(matTex) );
        matTex._11 = 0.5f;
        matTex._22 = 0.5f;
        matTex._31 = 0.5f;
        matTex._32 = 0.5f;
        matTex._33 = 1.0f;
        matTex._44 = 1.0f;

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
        m_pd3dDevice->SetTransform( D3DTS_TEXTURE0, &matTex );
    }
    else
    {
        // the texture sticks to the text: camera space position, taken back
        // to object space
        D3DXMATRIX matWorldView = m_matWorld * m_matView;
        D3DXMATRIX matWorldViewInv;

        D3DXMatrixInverse( &matWorldViewInv, NULL, &matWorldView );

        m_pd3dDevice->SetTransform( D3DTS_TEXTURE0, &matWorldViewInv );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
    }

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, m_bSpecular );
    m_pd3dDevice->SetRenderState( D3DRS_WRAP0, D3DWRAP_U | D3DWRAP_V );
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );

    m_pd3dDevice->SetTexture( 0, m_pDeviceObjects->m_pTexture );
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::InvalidateDeviceObjects()
{
    SAFE_RELEASE( m_pDeviceObjects->m_pTexture );
    SAFE_RELEASE( m_pDeviceObjects->m_pMesh );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ConfirmDevice()
// Desc: Called during device initialization, this code checks the device
//       for some minimum set of capabilities
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::ConfirmDevice( D3DCAPS9* pCaps, DWORD dwBehavior, D3DFORMAT fmtBackBuffer )
{
    (void)fmtBackBuffer;

    // Need D3DVTXPCAPS_TEXGEN for D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR
    if( ( ( dwBehavior & D3DCREATE_HARDWARE_VERTEXPROCESSING ) ||
          ( dwBehavior & D3DCREATE_MIXED_VERTEXPROCESSING ) ) &&
        !( pCaps->VertexProcessingCaps & D3DVTXPCAPS_TEXGEN ) )
    {
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDevice()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SetDevice( UINT iDevice )
{
    m_pDeviceObjects = &m_DeviceObjectsArray[iDevice];
}




//-----------------------------------------------------------------------------
// Name: CreateTextureFromFile()
// Desc: Loads a picture the user chose.  Returns NULL if that is not possible.
//-----------------------------------------------------------------------------
IDirect3DTexture9* CTextScreensaver::CreateTextureFromFile( const WCHAR* pszFileName )
{
    IDirect3DDevice9*   pd3dDevice = m_pd3dDevice;
    IWICImagingFactory* pFactory = NULL;
    IWICBitmapDecoder*  pDecoder = NULL;
    IDirect3DTexture9*  pTexture = NULL;
    HRESULT             hr;

    hr = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                           IID_IWICImagingFactory, (void**)&pFactory );
    if( SUCCEEDED( hr ) )
    {
        hr = pFactory->CreateDecoderFromFilename( pszFileName, NULL, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, &pDecoder );
        if( SUCCEEDED( hr ) )
        {
            hr = Texture::LoadTextureFromDecoder( pd3dDevice, pFactory, pDecoder, 0,
                                                  D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, &pTexture );
        }
    }

    SAFE_RELEASE( pDecoder );
    SAFE_RELEASE( pFactory );

    if( FAILED( hr ) )
        pTexture = NULL;

    return pTexture;
}




//-----------------------------------------------------------------------------
// Name: BuildTextMesh()
// Desc: Creates the mesh of the given string for the current device and
//       measures it.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::BuildTextMesh( const WCHAR* pszDisplayString )
{
    HRESULT                 hr;
    HDC                     hdc;
    WCHAR                   szText[MAX_DISPLAY_STRING + 1];
    WCHAR                   szVisual[MAX_DISPLAY_STRING + 1];
    UINT                    rgOrder[MAX_DISPLAY_STRING + 1];
    GLYPHMETRICSFLOAT       rgGlyphMetrics[MAX_DISPLAY_STRING + 1];
    SCRIPT_STRING_ANALYSIS  ssa;
    size_t                  cch;

    SAFE_RELEASE( m_pDeviceObjects->m_pMesh );

    hdc = CreateDCW( L"DISPLAY", NULL, NULL, NULL );
    SelectObject( hdc, m_hFont );

    StringCchCopyW( szText, MAX_DISPLAY_STRING + 1, pszDisplayString );

    hr = StringCchLengthW( pszDisplayString, MAX_DISPLAY_STRING + 1, &cch );
    if( FAILED( hr ) )
        return hr;

    // The mesh is built glyph by glyph from left to right, so the characters
    // have to be put into their visual order first (right-to-left scripts).
    hr = ScriptStringAnalyse( hdc, pszDisplayString, (INT)cch, (INT)( (FLOAT)cch * 1.5f + 16.0f ), -1,
                              SSA_GLYPHS, 1000, NULL, NULL, NULL, NULL, NULL, &ssa );
    if( FAILED( hr ) )
        return hr;

    hr = ScriptStringGetOrder( ssa, rgOrder );
    if( FAILED( hr ) )
        return hr;

    for( size_t i = 0; i < cch; i++ )
        szVisual[i] = szText[rgOrder[i]];

    StringCchCopyW( szText, cch + 1, szVisual );

    hr = ScriptStringFree( &ssa );
    if( FAILED( hr ) )
        return hr;

    // The higher the quality setting, the smaller the chordal deviation
    FLOAT fDeviation = 5.0f / (FLOAT)( m_dwMeshQuality + 10 );

    hr = Shape3D::CreateTextW( m_pd3dDevice, hdc, szText, fDeviation, TEXT_EXTRUSION,
                               &m_pDeviceObjects->m_pMesh, NULL, rgGlyphMetrics );
    if( FAILED( hr ) )
    {
        // The font cannot show this string: fall back on the product name
        GetDefaultDisplayString( szText, MAX_DISPLAY_STRING + 1 );

        hr = Shape3D::CreateTextW( m_pd3dDevice, hdc, szText, fDeviation, TEXT_EXTRUSION,
                                   &m_pDeviceObjects->m_pMesh, NULL, rgGlyphMetrics );
    }

    DeleteDC( hdc );

    if( FAILED( hr ) )
        return hr;

    // Update m_fTextMinX, etc.
    INT   nLength = lstrlenW( szText );
    FLOAT fOriginX = 0.0f;
    FLOAT fOriginY = 0.0f;

    m_fTextMinX = 1000000.0f;
    m_fTextMaxX = -1000000.0f;
    m_fTextMaxY = -1000000.0f;
    m_fTextMinY = 1000000.0f;

    for( INT i = 0; i < nLength; i++ )
    {
        GLYPHMETRICSFLOAT* pgm = &rgGlyphMetrics[i];
        FLOAT              f;

        f = fOriginX + pgm->gmfptGlyphOrigin.x;
        if( f < m_fTextMinX )
            m_fTextMinX = f;
        if( f + pgm->gmfBlackBoxX > m_fTextMaxX )
            m_fTextMaxX = f + pgm->gmfBlackBoxX;

        f = fOriginY + pgm->gmfptGlyphOrigin.y;
        if( f > m_fTextMaxY )
            m_fTextMaxY = f;
        if( f - pgm->gmfBlackBoxY < m_fTextMinY )
            m_fTextMinY = f - pgm->gmfBlackBoxY;

        fOriginX += pgm->gmfCellIncX;
        fOriginY += pgm->gmfCellIncY;
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: UpdateAngles()
// Desc: Advances the rotation by dwElapsed milliseconds.  The angles are kept
//       in revolutions and wrapped to 0 .. 1.
//-----------------------------------------------------------------------------
static inline FLOAT Fraction( FLOAT f )
{
    return f + ( 0.0f - (FLOAT)(INT)f );
}

VOID CTextScreensaver::UpdateAngles( DWORD dwElapsed )
{
    static FLOAT s_fPhase = 0.0f;       // of the see-saw and wobble motions
    static FLOAT s_fRevZ = 0.0f;
    static FLOAT s_fRevY = 0.0f;
    static FLOAT s_fRevX = 0.0f;

    FLOAT fStep = (FLOAT)( m_dwRotationSpeed * dwElapsed );
    FLOAT fRevY = s_fRevY;

    switch( m_RotType )
    {
        case spin:
            fRevY = s_fRevY + fStep * 0.00002f;
            break;

        case seesaw:
            s_fPhase = s_fPhase + fStep * 0.000025f;
            fRevY = sinf( s_fPhase * ( 2.0f * D3DX_PI ) ) * 0.17f + 1.0f;
            break;

        case wobble:
        {
            s_fPhase = s_fPhase + fStep * 0.000025f;
            FLOAT fAngle = s_fPhase * ( 2.0f * D3DX_PI );
            fRevY = sinf( fAngle ) * 0.17f + 1.0f;
            s_fRevZ = cosf( fAngle ) * 0.09f + 1.0f;
            break;
        }

        case tumble:
            s_fRevZ = s_fRevZ + fStep * 0.000007f;
            s_fRevX = s_fRevX + fStep * 0.000004f;
            fRevY = s_fRevY + fStep * 0.000018f;
            break;

        default:
            break;
    }

    s_fRevX = Fraction( s_fRevX );
    s_fRevY = Fraction( fRevY );
    m_fAngleX = s_fRevX * ( 2.0f * D3DX_PI );
    s_fRevZ = Fraction( s_fRevZ );
    m_fAngleY = s_fRevY * ( 2.0f * D3DX_PI );
    m_fAngleZ = s_fRevZ * ( 2.0f * D3DX_PI );
}




//-----------------------------------------------------------------------------
// Name: UpdateTimeString()
// Desc: Puts the current time into pszTime.  Returns TRUE if that changed the
//       string.
//-----------------------------------------------------------------------------
BOOL CTextScreensaver::UpdateTimeString( WCHAR* pszTime, size_t cchTime )
{
    WCHAR szNow[MAX_DISPLAY_STRING + 1];

    GetTimeFormatW( LOCALE_USER_DEFAULT, 0, NULL, NULL, szNow, MAX_DISPLAY_STRING + 1 );

    if( lstrcmpW( pszTime, szNow ) == 0 )
        return FALSE;

    StringCchCopyW( pszTime, cchTime, szNow );
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: ReadSettings()
// Desc: Read user preferences from the registry
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ReadSettings()
{
    HKEY  hkey;
    DWORD dwType = REG_DWORD;
    DWORD dwLength = sizeof(DWORD);

    // Defaults
    GetDefaultDisplayString( m_szDisplayString, MAX_DISPLAY_STRING + 1 );

    m_Font.lfHeight = 96;
    m_Font.lfWidth = 0;
    m_Font.lfEscapement = 0;
    m_Font.lfOrientation = 0;
    m_Font.lfWeight = 0;
    m_Font.lfItalic = 0;
    m_Font.lfUnderline = 0;
    m_Font.lfStrikeOut = 0;
    m_Font.lfCharSet = DEFAULT_CHARSET;
    m_Font.lfOutPrecision = 0;
    m_Font.lfClipPrecision = 0;
    m_Font.lfQuality = 0;
    m_Font.lfPitchAndFamily = 0;
    LoadStringW( NULL, IDS_DEFAULT_FONT, m_Font.lfFaceName, LF_FACESIZE );

    m_SurfType = environment;
    m_bSpecular = TRUE;
    m_RotType = spin;
    m_dwRotationSpeed = 10;
    m_dwSize = 10;
    m_bUseTime = FALSE;
    m_dwMeshQuality = 500;
    m_SurfaceColor = RGB( 119, 119, 119 );
    m_bUseCustomColor = FALSE;
    m_bUseCustomTexture = FALSE;
    m_bUseCustomEnvironment = FALSE;
    StringCchCopyW( m_szCustomTexture, MAX_PATH, L"" );
    StringCchCopyW( m_szCustomEnvironment, MAX_PATH, L"" );

    // Read OpenGL settings first, so OS upgrade cases work
    ss_ReadSettings();

    if( ERROR_SUCCESS == RegOpenKeyExW( HKEY_CURRENT_USER, m_strRegPath, 0, KEY_READ, &hkey ) )
    {
        dwLength = sizeof(m_szDisplayString);
        if( ERROR_SUCCESS != RegQueryValueExW( hkey, L"DisplayString", NULL, &dwType,
                                               (BYTE*)m_szDisplayString, &dwLength ) )
        {
            GetDefaultDisplayString( m_szDisplayString, MAX_DISPLAY_STRING + 1 );
        }
        m_szDisplayString[MAX_DISPLAY_STRING] = L'\0';

        // (the BYTE members of the LOGFONT are stored as, and read back into,
        // four bytes each; the order of the calls makes that work out)
        dwLength = sizeof(DWORD);
        RegQueryValueExW( hkey, L"FontHeight", NULL, &dwType, (BYTE*)&m_Font.lfHeight, &dwLength );
        RegQueryValueExW( hkey, L"FontWeight", NULL, &dwType, (BYTE*)&m_Font.lfWeight, &dwLength );
        RegQueryValueExW( hkey, L"FontItalic", NULL, &dwType, (BYTE*)&m_Font.lfItalic, &dwLength );
        RegQueryValueExW( hkey, L"FontCharSet", NULL, &dwType, (BYTE*)&m_Font.lfCharSet, &dwLength );
        RegQueryValueExW( hkey, L"FontPitchFamily", NULL, &dwType, (BYTE*)&m_Font.lfPitchAndFamily, &dwLength );

        dwLength = sizeof(m_Font.lfFaceName);
        RegQueryValueExW( hkey, L"FontFace", NULL, &dwType, (BYTE*)m_Font.lfFaceName, &dwLength );

        dwLength = sizeof(DWORD);
        RegQueryValueExW( hkey, L"DisplayTime", NULL, &dwType, (BYTE*)&m_bUseTime, &dwLength );

        RegQueryValueExW( hkey, L"MeshQuality", NULL, &dwType, (BYTE*)&m_dwMeshQuality, &dwLength );
        if( m_dwMeshQuality > 1000 )
            m_dwMeshQuality = 1000;

        RegQueryValueExW( hkey, L"SurfaceType", NULL, &dwType, (BYTE*)&m_SurfType, &dwLength );
        RegQueryValueExW( hkey, L"Specular", NULL, &dwType, (BYTE*)&m_bSpecular, &dwLength );
        RegQueryValueExW( hkey, L"RotationSpeed", NULL, &dwType, (BYTE*)&m_dwRotationSpeed, &dwLength );

        RegQueryValueExW( hkey, L"Size", NULL, &dwType, (BYTE*)&m_dwSize, &dwLength );
        if( m_dwSize < 1 )
            m_dwSize = 1;
        if( m_dwSize > 10 )
            m_dwSize = 10;

        RegQueryValueExW( hkey, L"RotationStyle", NULL, &dwType, (BYTE*)&m_RotType, &dwLength );
        RegQueryValueExW( hkey, L"SurfaceColor", NULL, &dwType, (BYTE*)&m_SurfaceColor, &dwLength );
        RegQueryValueExW( hkey, L"UseCustomColor", NULL, &dwType, (BYTE*)&m_bUseCustomColor, &dwLength );
        RegQueryValueExW( hkey, L"UseCustomTexture", NULL, &dwType, (BYTE*)&m_bUseCustomTexture, &dwLength );
        RegQueryValueExW( hkey, L"UseCustomEnvironment", NULL, &dwType, (BYTE*)&m_bUseCustomEnvironment, &dwLength );

        dwLength = sizeof(m_szCustomTexture);
        if( ERROR_SUCCESS != RegQueryValueExW( hkey, L"CustomTexture", NULL, &dwType,
                                               (BYTE*)m_szCustomTexture, &dwLength ) )
        {
            StringCchCopyW( m_szCustomTexture, MAX_PATH, L"" );
        }

        dwLength = sizeof(m_szCustomEnvironment);
        if( ERROR_SUCCESS != RegQueryValueExW( hkey, L"CustomEnvironment", NULL, &dwType,
                                               (BYTE*)m_szCustomEnvironment, &dwLength ) )
        {
            StringCchCopyW( m_szCustomEnvironment, MAX_PATH, L"" );
        }

        ReadScreenSettings( hkey );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: ss_ReadSettings()
// Desc: Reads the settings of the OpenGL "3D Text" screen saver of earlier
//       versions of Windows.  It kept them in the section
//       [Screen Saver.3DText] of control.ini, which the system maps into the
//       registry.
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ss_ReadSettings()
{
    WCHAR szSection[100];

    if( !LoadStringW( m_hInstance, IDS_SAVERNAME, g_szSectName, BUFFER_SIZE ) )
        return;

    if( !LoadStringW( m_hInstance, IDS_INIFILE, g_szFname, BUFFER_SIZE ) )
        return;

    // Nothing to do if that screen saver was never configured
    if( GetPrivateProfileSectionW( g_szSectName, szSection, 100, g_szFname ) == 0 )
        return;

    // DemoType: 0 = string, 1 = time
    if( ss_GetRegistryInt( IDS_DEMOTYPE, 0 ) == 1 )
        m_bUseTime = TRUE;

    // RotStyle: 0 = none, 1 = see-saw, 2 = wobble, 3 = random
    m_RotType = (RotType)( ss_GetRegistryInt( IDS_ROTSTYLE, 0 ) + 1 );
    if( m_RotType == spin )
        m_RotType = none;
    if( m_RotType > wobble )
        m_RotType = spin;

    // Tesselation: 0 .. 100
    m_dwMeshQuality = ss_GetRegistryInt( IDS_TESSELATION, 0 ) * 10 + 1;
    if( m_dwMeshQuality > 1000 )
        m_dwMeshQuality = 1000;

    // Size, Speed: 0 .. 100
    m_dwSize = (DWORD)( ( (FLOAT)(DWORD)ss_GetRegistryInt( IDS_SIZE, 0 ) / 100.0f * 90.0f + 10.0f ) / 10.0f );
    m_dwRotationSpeed = (DWORD)( ( (FLOAT)(DWORD)ss_GetRegistryInt( IDS_SPEED, 0 ) / 100.0f * 190.0f + 10.0f ) / 10.0f );

    ss_GetRegistryString( IDS_FONT, L"", m_Font.lfFaceName, LF_FACESIZE );

    // FontAttributes: bit 0 = bold, bit 1 = italic
    INT nAttributes = ss_GetRegistryInt( IDS_FONT_ATTRIBUTES, 0 );
    if( nAttributes >= 0 )
    {
        m_Font.lfItalic = ( nAttributes & 2 ) ? TRUE : FALSE;
        m_Font.lfWeight = ( nAttributes & 1 ) ? FW_BOLD : FW_NORMAL;
    }

    m_Font.lfCharSet = (BYTE)ss_GetRegistryInt( IDS_CHARSET, 0 );

    ss_GetRegistryString( IDS_TEXT, L"", m_szDisplayString, MAX_DISPLAY_STRING );

    // SurfStyle: 0 = solid colour, 1 = texture
    m_SurfType = (SurfType)ss_GetRegistryInt( IDS_SURFSTYLE, 0 );
    if( m_SurfType > color )
        m_SurfType = texture;

    ss_GetRegistryString( IDS_TEXTURE, NULL, m_szCustomTexture, MAX_PATH );
    if( lstrlenW( m_szCustomTexture ) > 0 )
        m_bUseCustomTexture = TRUE;
}




//-----------------------------------------------------------------------------
// Name: ss_GetRegistryInt()
// Desc:
//-----------------------------------------------------------------------------
INT CTextScreensaver::ss_GetRegistryInt( INT idName, INT iDefault )
{
    WCHAR szItemName[BUFFER_SIZE + 1];

    if( LoadStringW( m_hInstance, idName, szItemName, BUFFER_SIZE ) )
        return GetPrivateProfileIntW( g_szSectName, szItemName, iDefault, g_szFname );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: ss_GetRegistryString()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ss_GetRegistryString( INT idName, const WCHAR* pszDefault, WCHAR* pszData, INT cchData )
{
    WCHAR szItemName[BUFFER_SIZE + 1];

    if( LoadStringW( m_hInstance, idName, szItemName, BUFFER_SIZE ) )
        GetPrivateProfileStringW( g_szSectName, szItemName, pszDefault, pszData, cchData, g_szFname );
}




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::DoConfig()
{
    ReadSettings();
    DialogBoxParamW( NULL, MAKEINTRESOURCEW( IDD_SETTINGS ), m_hWndParent, SettingsDialogProcStub, 0 );
}




//-----------------------------------------------------------------------------
// Name: SettingsDialogProcStub()
// Desc:
//-----------------------------------------------------------------------------
INT_PTR CALLBACK CTextScreensaver::SettingsDialogProcStub( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    return g_pMyTextScreensaver->SettingsDialogProc( hDlg, uMsg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: SettingsDialogProc()
// Desc:
//-----------------------------------------------------------------------------
BOOL CTextScreensaver::SettingsDialogProc( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    (void)lParam;

    switch( uMsg )
    {
        case WM_INITDIALOG:
            InitItems( hDlg );
            break;

        case WM_COMMAND:
            if( HIWORD( wParam ) == BN_CLICKED )
            {
                switch( LOWORD( wParam ) )
                {
                    case IDOK:
                        ExtractAndWriteSettings( hDlg );
                        EndDialog( hDlg, TRUE );
                        break;

                    case IDCANCEL:
                        ReadSettings();
                        EndDialog( hDlg, FALSE );
                        break;

                    case IDC_SCREENSETTINGS:
                        DoScreenSettingsDialog( hDlg );
                        break;

                    case IDC_RADIO_TIME:
                        EnableWindow( GetDlgItem( hDlg, IDC_DISPLAYSTRING ), FALSE );
                        break;

                    case IDC_RADIO_TEXT:
                        EnableWindow( GetDlgItem( hDlg, IDC_DISPLAYSTRING ), TRUE );
                        break;

                    case IDC_RADIO_COLOR:
                    case IDC_RADIO_TEXTURE:
                    case IDC_RADIO_REFLECTION:
                    case IDC_USE_CUSTOM_COLOR:
                    case IDC_USE_CUSTOM_TEXTURE:
                    case IDC_USE_CUSTOM_ENVIRONMENT:
                        if( IsDlgButtonChecked( hDlg, IDC_RADIO_COLOR ) )
                            m_SurfType = color;
                        else if( IsDlgButtonChecked( hDlg, IDC_RADIO_TEXTURE ) )
                            m_SurfType = texture;
                        else if( IsDlgButtonChecked( hDlg, IDC_RADIO_REFLECTION ) )
                            m_SurfType = environment;

                        EnableTextureWindows( hDlg, m_SurfType );
                        break;

                    case IDC_SURFACE_COLOR:
                    {
                        static COLORREF s_custColors[16];
                        CHOOSECOLORW    cc;

                        ZeroMemory( &cc, sizeof(cc) );
                        cc.lStructSize = sizeof(cc);
                        cc.hwndOwner = hDlg;
                        cc.hInstance = (HWND)m_hInstance;
                        cc.rgbResult = m_SurfaceColor;
                        cc.lpCustColors = s_custColors;
                        cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;

                        if( ChooseColorW( &cc ) )
                            m_SurfaceColor = cc.rgbResult;
                        break;
                    }

                    case IDC_BROWSE_TEXTURE:
                        SelectCustomTexture( hDlg );
                        break;

                    case IDC_BROWSE_ENVIRONMENT:
                        SelectCustomEnvironment( hDlg );
                        break;

                    case IDC_CHOOSEFONT:
                    {
                        CHOOSEFONTW cf;

                        ZeroMemory( &cf, sizeof(cf) );
                        cf.lStructSize = sizeof(cf);
                        cf.hwndOwner = hDlg;
                        cf.hInstance = m_hInstance;
                        cf.lpLogFont = &m_Font;
                        cf.iPointSize = 960;
                        cf.Flags = CF_SCREENFONTS | CF_ENABLETEMPLATE | CF_INITTOLOGFONTSTRUCT |
                                   CF_FORCEFONTEXIST | CF_TTONLY | CF_NOVERTFONTS;

                        // a template without the size list: the size does not matter
                        cf.lpTemplateName = MAKEINTRESOURCEW( FORMATDLGORD31 );

                        if( ChooseFontW( &cf ) )
                            m_Font = *cf.lpLogFont;
                        break;
                    }
                }
            }
            else if( HIWORD( wParam ) == EN_CHANGE && LOWORD( wParam ) == IDC_DISPLAYSTRING )
            {
                GetDlgItemTextW( hDlg, IDC_DISPLAYSTRING, m_szDisplayString, MAX_DISPLAY_STRING );
            }
            break;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: InitItems()
// Desc: Set up the dialog controls from the settings
//-----------------------------------------------------------------------------
VOID CTextScreensaver::InitItems( HWND hDlg )
{
    WCHAR sz[100];

    // Set the limit of the text box
    SendDlgItemMessageW( hDlg, IDC_DISPLAYSTRING, EM_SETLIMITTEXT, MAX_DISPLAY_STRING, 0 );

    // Set the text string
    SetDlgItemTextW( hDlg, IDC_DISPLAYSTRING, m_szDisplayString );

    // Set the radio button for time or text display
    if( m_bUseTime )
    {
        EnableWindow( GetDlgItem( hDlg, IDC_DISPLAYSTRING ), FALSE );
        CheckRadioButton( hDlg, IDC_RADIO_TIME, IDC_RADIO_TEXT, IDC_RADIO_TIME );
    }
    else
    {
        EnableWindow( GetDlgItem( hDlg, IDC_DISPLAYSTRING ), TRUE );
        CheckRadioButton( hDlg, IDC_RADIO_TIME, IDC_RADIO_TEXT, IDC_RADIO_TEXT );
    }

    // Set the selection for the surface style
    switch( m_SurfType )
    {
        case color:
            CheckRadioButton( hDlg, IDC_RADIO_COLOR, IDC_RADIO_REFLECTION, IDC_RADIO_COLOR );
            break;
        case environment:
            CheckRadioButton( hDlg, IDC_RADIO_COLOR, IDC_RADIO_REFLECTION, IDC_RADIO_REFLECTION );
            break;
        case texture:
            CheckRadioButton( hDlg, IDC_RADIO_COLOR, IDC_RADIO_REFLECTION, IDC_RADIO_TEXTURE );
            break;
    }

    // Check/uncheck the specular box
    SendDlgItemMessageW( hDlg, IDC_SPECULAR, BM_SETCHECK, m_bSpecular ? BST_CHECKED : BST_UNCHECKED, 0 );

    // Enable/disable the surface color button
    EnableWindow( GetDlgItem( hDlg, IDC_SURFACE_COLOR ), m_SurfType == color );

    // Check/uncheck the custom boxes
    SendDlgItemMessageW( hDlg, IDC_USE_CUSTOM_COLOR, BM_SETCHECK,
                         m_bUseCustomColor ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessageW( hDlg, IDC_USE_CUSTOM_TEXTURE, BM_SETCHECK,
                         m_bUseCustomTexture ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessageW( hDlg, IDC_USE_CUSTOM_ENVIRONMENT, BM_SETCHECK,
                         m_bUseCustomEnvironment ? BST_CHECKED : BST_UNCHECKED, 0 );

    // Enable all the texture windows properly
    EnableTextureWindows( hDlg, m_SurfType );

    // Fill the rotation style combo box, in the order of enum RotType
    LoadStringW( NULL, IDS_ROTATION_NONE, sz, 100 );
    SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadStringW( NULL, IDS_ROTATION_SPIN, sz, 100 );
    SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadStringW( NULL, IDS_ROTATION_SEESAW, sz, 100 );
    SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadStringW( NULL, IDS_ROTATION_WOBBLE, sz, 100 );
    SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadStringW( NULL, IDS_ROTATION_TUMBLE, sz, 100 );
    SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );

    // Set the current rotation style selection
    SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_SETCURSEL, m_RotType, 0 );

    // Set up the rotation speed slider
    SendDlgItemMessageW( hDlg, IDC_ROTATION_SPEED, TBM_SETRANGE, TRUE, MAKELONG( 1, 20 ) );
    SendDlgItemMessageW( hDlg, IDC_ROTATION_SPEED, TBM_SETPOS, TRUE, m_dwRotationSpeed );

    // Set up the resolution slider
    SendDlgItemMessageW( hDlg, IDC_RESOLUTION, TBM_SETRANGE, TRUE, MAKELONG( 1, 10 ) );
    SendDlgItemMessageW( hDlg, IDC_RESOLUTION, TBM_SETPOS, TRUE, m_dwMeshQuality / 100 );

    // Set up the size slider
    SendDlgItemMessageW( hDlg, IDC_VIEWPORTSIZE, TBM_SETRANGE, TRUE, MAKELONG( 1, 10 ) );
    SendDlgItemMessageW( hDlg, IDC_VIEWPORTSIZE, TBM_SETPOS, TRUE, m_dwSize );
}




//-----------------------------------------------------------------------------
// Name: ExtractAndWriteSettings()
// Desc: Get the settings from the dialog and write them to the registry
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ExtractAndWriteSettings( HWND hDlg )
{
    HKEY    hkey = NULL;
    LRESULT lResult;

    m_bSpecular = IsDlgButtonChecked( hDlg, IDC_SPECULAR );
    m_bUseCustomColor = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_COLOR );
    m_bUseCustomTexture = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_TEXTURE );
    m_bUseCustomEnvironment = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_ENVIRONMENT );

    lResult = SendDlgItemMessageW( hDlg, IDC_ROTATION_STYLE, CB_GETCURSEL, 0, 0 );
    if( lResult != CB_ERR )
        m_RotType = (RotType)lResult;

    m_dwSize = (DWORD)SendDlgItemMessageW( hDlg, IDC_VIEWPORTSIZE, TBM_GETPOS, 0, 0 );
    m_dwRotationSpeed = (DWORD)SendDlgItemMessageW( hDlg, IDC_ROTATION_SPEED, TBM_GETPOS, 0, 0 );
    m_dwMeshQuality = 100 * (DWORD)SendDlgItemMessageW( hDlg, IDC_RESOLUTION, TBM_GETPOS, 0, 0 );

    GetDlgItemTextW( hDlg, IDC_DISPLAYSTRING, m_szDisplayString, MAX_DISPLAY_STRING + 1 );

    if( IsDlgButtonChecked( hDlg, IDC_RADIO_TIME ) )
        m_bUseTime = TRUE;
    else
        m_bUseTime = FALSE;

    if( ERROR_SUCCESS == RegCreateKeyExW( HKEY_CURRENT_USER, m_strRegPath, 0, NULL,
                                          REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL ) )
    {
        WriteScreenSettings( hkey );

        RegSetValueExW( hkey, L"SurfaceType", 0, REG_DWORD, (BYTE*)&m_SurfType, sizeof(DWORD) );
        RegSetValueExW( hkey, L"Specular", 0, REG_DWORD, (BYTE*)&m_bSpecular, sizeof(DWORD) );
        RegSetValueExW( hkey, L"SurfaceColor", 0, REG_DWORD, (BYTE*)&m_SurfaceColor, sizeof(DWORD) );
        RegSetValueExW( hkey, L"CustomTexture", 0, REG_SZ, (BYTE*)m_szCustomTexture,
                        ( lstrlenW( m_szCustomTexture ) + 1 ) * sizeof(WCHAR) );
        RegSetValueExW( hkey, L"CustomEnvironment", 0, REG_SZ, (BYTE*)m_szCustomEnvironment,
                        ( lstrlenW( m_szCustomEnvironment ) + 1 ) * sizeof(WCHAR) );
        RegSetValueExW( hkey, L"UseCustomColor", 0, REG_DWORD, (BYTE*)&m_bUseCustomColor, sizeof(DWORD) );
        RegSetValueExW( hkey, L"UseCustomTexture", 0, REG_DWORD, (BYTE*)&m_bUseCustomTexture, sizeof(DWORD) );
        RegSetValueExW( hkey, L"UseCustomEnvironment", 0, REG_DWORD, (BYTE*)&m_bUseCustomEnvironment, sizeof(DWORD) );
        RegSetValueExW( hkey, L"MeshQuality", 0, REG_DWORD, (BYTE*)&m_dwMeshQuality, sizeof(DWORD) );
        RegSetValueExW( hkey, L"Size", 0, REG_DWORD, (BYTE*)&m_dwSize, sizeof(DWORD) );
        RegSetValueExW( hkey, L"RotationSpeed", 0, REG_DWORD, (BYTE*)&m_dwRotationSpeed, sizeof(DWORD) );
        RegSetValueExW( hkey, L"RotationStyle", 0, REG_DWORD, (BYTE*)&m_RotType, sizeof(DWORD) );
        RegSetValueExW( hkey, L"DisplayString", 0, REG_SZ, (BYTE*)m_szDisplayString,
                        ( lstrlenW( m_szDisplayString ) + 1 ) * sizeof(WCHAR) );
        RegSetValueExW( hkey, L"DisplayTime", 0, REG_DWORD, (BYTE*)&m_bUseTime, sizeof(DWORD) );
        RegSetValueExW( hkey, L"FontWeight", 0, REG_DWORD, (BYTE*)&m_Font.lfWeight, sizeof(DWORD) );
        RegSetValueExW( hkey, L"FontHeight", 0, REG_DWORD, (BYTE*)&m_Font.lfHeight, sizeof(DWORD) );
        RegSetValueExW( hkey, L"FontItalic", 0, REG_DWORD, (BYTE*)&m_Font.lfItalic, sizeof(DWORD) );
        RegSetValueExW( hkey, L"FontCharSet", 0, REG_DWORD, (BYTE*)&m_Font.lfCharSet, sizeof(DWORD) );
        RegSetValueExW( hkey, L"FontPitchFamily", 0, REG_DWORD, (BYTE*)&m_Font.lfPitchAndFamily, sizeof(DWORD) );
        RegSetValueExW( hkey, L"FontFace", 0, REG_SZ, (BYTE*)m_Font.lfFaceName,
                        ( lstrlenW( m_Font.lfFaceName ) + 1 ) * sizeof(WCHAR) );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: BrowseForPicture()
// Desc: The file dialog of the two "Choose ..." buttons.  (The original has
//       this code twice, in SelectCustomTexture() and
//       SelectCustomEnvironment().)
//-----------------------------------------------------------------------------
static inline BOOL BrowseForPicture( HWND hDlg, HINSTANCE hInstance, UINT idTitle, WCHAR* pszFile )
{
    WCHAR         szTitle[100];
    WCHAR         szFilter[200];
    WCHAR         szWindowsDir[MAX_PATH];
    OPENFILENAMEW ofn;

    LoadStringW( NULL, idTitle, szTitle, 100 );

    // The filter string is stored with '#' in place of the embedded nulls
    LoadStringW( NULL, IDS_TEXTURE_FILTER, szFilter, 200 );
    for( WCHAR* pch = szFilter; *pch != L'\0'; pch++ )
    {
        if( *pch == L'#' )
            *pch = L'\0';
    }

    if( GetWindowsDirectoryW( szWindowsDir, MAX_PATH ) == 0 )
        szWindowsDir[0] = L'\0';

    ZeroMemory( &ofn, sizeof(ofn) );
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.hInstance = hInstance;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = pszFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = szTitle;
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = L"mid";       // (sic)

    // start in the Windows directory unless there is a file to start with
    if( pszFile[0] == L'\0' )
        ofn.lpstrInitialDir = szWindowsDir;

    return GetOpenFileNameW( &ofn );
}




//-----------------------------------------------------------------------------
// Name: SelectCustomTexture()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SelectCustomTexture( HWND hDlg )
{
    WCHAR szFile[MAX_PATH] = L"";

    StringCchCopyW( szFile, MAX_PATH, m_szCustomTexture );

    if( BrowseForPicture( hDlg, m_hInstance, IDS_CHOOSE_TEXTURE, szFile ) )
        StringCchCopyW( m_szCustomTexture, MAX_PATH, szFile );
}




//-----------------------------------------------------------------------------
// Name: SelectCustomEnvironment()
// Desc: (Unlike SelectCustomTexture(), this does not start with the current
//       file.)
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SelectCustomEnvironment( HWND hDlg )
{
    WCHAR szFile[MAX_PATH] = L"";

    if( BrowseForPicture( hDlg, m_hInstance, IDS_CHOOSE_ENVIRONMENT, szFile ) )
        StringCchCopyW( m_szCustomEnvironment, MAX_PATH, szFile );
}




//-----------------------------------------------------------------------------
// Name: EnableTextureWindows()
// Desc: Enables the controls that belong to the selected surface style.
//-----------------------------------------------------------------------------
VOID CTextScreensaver::EnableTextureWindows( HWND hDlg, SurfType surfType )
{
    HWND hwndUseCustomColor = GetDlgItem( hDlg, IDC_USE_CUSTOM_COLOR );
    HWND hwndUseCustomTexture = GetDlgItem( hDlg, IDC_USE_CUSTOM_TEXTURE );
    HWND hwndUseCustomEnvironment = GetDlgItem( hDlg, IDC_USE_CUSTOM_ENVIRONMENT );
    HWND hwndSurfaceColor = GetDlgItem( hDlg, IDC_SURFACE_COLOR );
    HWND hwndBrowseTexture = GetDlgItem( hDlg, IDC_BROWSE_TEXTURE );
    HWND hwndBrowseEnvironment = GetDlgItem( hDlg, IDC_BROWSE_ENVIRONMENT );

    BOOL bUseCustomColor = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_COLOR );
    BOOL bUseCustomTexture = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_TEXTURE );
    BOOL bUseCustomEnvironment = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_ENVIRONMENT );

    switch( surfType )
    {
        case environment:
            EnableWindow( hwndUseCustomColor, FALSE );
            EnableWindow( hwndUseCustomTexture, FALSE );
            EnableWindow( hwndUseCustomEnvironment, TRUE );
            EnableWindow( hwndSurfaceColor, FALSE );
            EnableWindow( hwndBrowseTexture, FALSE );
            EnableWindow( hwndBrowseEnvironment, bUseCustomEnvironment );
            break;

        case texture:
            EnableWindow( hwndUseCustomColor, FALSE );
            EnableWindow( hwndUseCustomTexture, TRUE );
            EnableWindow( hwndUseCustomEnvironment, FALSE );
            EnableWindow( hwndSurfaceColor, FALSE );
            EnableWindow( hwndBrowseTexture, bUseCustomTexture );
            EnableWindow( hwndBrowseEnvironment, FALSE );
            break;

        default:    // color
            EnableWindow( hwndUseCustomColor, TRUE );
            EnableWindow( hwndUseCustomTexture, FALSE );
            EnableWindow( hwndUseCustomEnvironment, FALSE );
            EnableWindow( hwndSurfaceColor, bUseCustomColor );
            EnableWindow( hwndBrowseTexture, FALSE );
            EnableWindow( hwndBrowseEnvironment, FALSE );
            break;
    }
}
