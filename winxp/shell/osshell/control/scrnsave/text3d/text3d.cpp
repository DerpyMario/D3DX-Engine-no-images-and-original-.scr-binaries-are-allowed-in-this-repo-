//-----------------------------------------------------------------------------
// File: text3d.cpp
//
// Desc: The 3D Text screen saver (sstext3d.scr) of Windows XP / Windows
//       Server 2003 (x86), Direct3D 8 version.
//
//       A string, or the time of day, is turned into an extruded mesh
//       (D3DXCreateText) which rotates in one of several ways and bounces
//       around the desktop.  It is lit by one directional light and is either
//       solid colored, textured, or reflects an environment map.
//
//       Reconstructed from text3d.obj; see text3d.h for the naming
//       conventions.  Original object:
//           d:\nt\shell\osshell\control\scrnsave\text3d\obj\i386\text3d.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <d3d8.h>
#include <d3dx8.h>
#include "../d3dsaver/d3dsaver.h"
#include "../d3dsaver/dxutil.h"
#include "text3d.h"
#include "resource.h"

#ifndef NO_D3D8RGB
// Software rasterizer of the DirectX 8 DDK, linked from d3d8rgb.lib (a
// library internal to the Windows source tree: public\internal\multimedia).
extern "C" void* D3D8RGBRasterizer;
#endif

#define BUF_SIZE                255

#define TEXT_EXTRUSION          0.5f

#ifndef min     // (windef.h of the Windows SDK)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

// Where the OpenGL version of this screen saver kept its settings
TCHAR                   g_szSectName[BUF_SIZE+1];
TCHAR                   g_szFname[BUF_SIZE+1];

CTextScreensaver*       g_pMyTextScreensaver = NULL;




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
    HRESULT hr;
    CTextScreensaver textSS;

    if( FAILED( hr = textSS.Create( hInst ) ) )
    {
        textSS.DisplayErrorMsg( hr );
        return 0;
    }

    return textSS.Run();
}




//-----------------------------------------------------------------------------
// Name: LoadTextureFromResource()
// Desc: Creates a texture from a picture (JPEG) in the resources of the
//       screen saver.
//-----------------------------------------------------------------------------
HRESULT LoadTextureFromResource( LPDIRECT3DDEVICE8 pd3dDevice, TCHAR* strRes, TCHAR* strResType,
                                 LPDIRECT3DTEXTURE8* ppTex )
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

    if( *ppTex == NULL)
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CTextScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
CTextScreensaver::CTextScreensaver()
{
    g_pMyTextScreensaver = this;

    LoadString( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );

    InitCommonControls();

    ZeroMemory( m_DeviceObjectsArray, sizeof(m_DeviceObjectsArray) );

    m_floatrect.xSize = 0.0f;
    m_hFont = NULL;
    m_bUseDepthBuffer = TRUE;
    m_dwMinDepthBits = 16;

    lstrcpy( m_strRegPath, TEXT("Software\\Microsoft\\Screensavers\\Text3D") );

    m_dwMeshUpdateCounter = 0;

    m_fAngleX = 0.0f;
    m_fAngleY = 0.0f;
    m_fAngleZ = 0.0f;

    srand((UINT)time(NULL));
}




//-----------------------------------------------------------------------------
// Name: RegisterSoftwareDevice()
// Desc: This can register the D3D8RGBRasterizer or any other
//       pluggable software rasterizer.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::RegisterSoftwareDevice()
{
#ifndef NO_D3D8RGB
    m_pD3D->RegisterSoftwareDevice( D3D8RGBRasterizer );
#endif

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
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    }
    else
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    }
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    // Set up lighting
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, m_bSpecular ? 0x00464646 : 0x00646464 );

    D3DLIGHT8 light;
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
    D3DMATERIAL8 mtrl;

    if( m_SurfType == color )
    {
        COLORREF cr = m_bUseCustomColor ? m_SurfaceColor : RGB( 119, 119, 119 );

        mtrl.Diffuse.r = (FLOAT)GetRValue( cr ) * (1.0f / 255.0f);
        mtrl.Diffuse.g = (FLOAT)GetGValue( cr ) * (1.0f / 255.0f);
        mtrl.Diffuse.b = (FLOAT)GetBValue( cr ) * (1.0f / 255.0f);
        mtrl.Diffuse.a = 0.0f;      // (left uninitialized in the original)
    }
    else
    {
        mtrl.Diffuse = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 0.0f );
    }
    mtrl.Ambient = mtrl.Diffuse;
    mtrl.Specular = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 0.0f );
    mtrl.Emissive = D3DXCOLOR( 0.0f, 0.0f, 0.0f, 0.0f );
    mtrl.Power = 5.0f;
    m_pd3dDevice->SetMaterial( &mtrl );

    // Set up texture coordinate generation
    if( m_SurfType == environment )
    {
        // sphere map: the reflection vector, squeezed into 0 .. 1
        D3DXMATRIX matTex;

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );

        ZeroMemory( &matTex, sizeof(matTex) );
        matTex._11 = 0.5f;
        matTex._22 = 0.5f;
        matTex._31 = 0.5f;
        matTex._32 = 0.5f;
        matTex._33 = 1.0f;
        matTex._44 = 1.0f;
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
// Desc: Texture coordinate generation is needed (environment mapping and
//       textures are projected); hardware vertex processing must do it.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::ConfirmDevice( D3DCAPS8* pCaps, DWORD dwBehavior, D3DFORMAT fmtBackBuffer )
{
    if( dwBehavior & (D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MIXED_VERTEXPROCESSING) )
    {
        if( !(pCaps->VertexProcessingCaps & D3DVTXPCAPS_TEXGEN) )
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
IDirect3DTexture8* CTextScreensaver::CreateTextureFromFile( const TCHAR* pszFile )
{
    IDirect3DTexture8* pTexture;

    if( FAILED( D3DXCreateTextureFromFile( m_pd3dDevice, pszFile, &pTexture ) ) )
        return NULL;

    return pTexture;
}




//-----------------------------------------------------------------------------
// Name: BuildTextMesh()
// Desc: Creates the mesh of the given string for the current device and
//       measures it.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::BuildTextMesh( const TCHAR* pszDisplayString )
{
    HRESULT hr;
    HDC hdc;
    TCHAR szText[MAX_DISPLAY_STRING + 1];
    GLYPHMETRICSFLOAT agmf[MAX_DISPLAY_STRING + 1];

    SAFE_RELEASE( m_pDeviceObjects->m_pMesh );

    hdc = CreateDC( TEXT("DISPLAY"), NULL, NULL, NULL );
    SelectObject( hdc, m_hFont );

    lstrcpyn( szText, pszDisplayString, MAX_DISPLAY_STRING + 1 );

    // The higher the quality setting, the smaller the chordal deviation
    FLOAT fDeviation = 5.0f / (FLOAT)( m_dwMeshQuality + 10 );

    hr = D3DXCreateText( m_pd3dDevice, hdc, szText, fDeviation, TEXT_EXTRUSION,
                         &m_pDeviceObjects->m_pMesh, NULL, agmf );
    if( FAILED( hr ) )
    {
        // The font cannot show this string: fall back on the default one
        LoadString( NULL, IDS_DEFAULT_TEXT, szText, MAX_DISPLAY_STRING );
        hr = D3DXCreateText( m_pd3dDevice, hdc, szText, fDeviation, TEXT_EXTRUSION,
                             &m_pDeviceObjects->m_pMesh, NULL, agmf );
    }

    DeleteDC( hdc );

    if( SUCCEEDED( hr ) )
    {
        // Update m_fTextMinX, etc.
        int nLength = lstrlen( szText );
        FLOAT fOriginX = 0.0f;
        FLOAT fOriginY = 0.0f;
        FLOAT f;

        m_fTextMinX = 1000000.0f;
        m_fTextMaxX = -1000000.0f;
        m_fTextMinY = 1000000.0f;
        m_fTextMaxY = -1000000.0f;

        for( int i = 0; i < nLength; i++ )
        {
            GLYPHMETRICSFLOAT* pgm = &agmf[i];

            f = fOriginX + pgm->gmfptGlyphOrigin.x;
            if( f < m_fTextMinX )
                m_fTextMinX = f;
            f = pgm->gmfBlackBoxX + pgm->gmfptGlyphOrigin.x + fOriginX;
            if( f > m_fTextMaxX )
                m_fTextMaxX = f;
            f = fOriginY + pgm->gmfptGlyphOrigin.y;
            if( f > m_fTextMaxY )
                m_fTextMaxY = f;
            f = f - pgm->gmfBlackBoxY;
            if( f < m_fTextMinY )
                m_fTextMinY = f;

            fOriginX += pgm->gmfCellIncX;
            fOriginY += pgm->gmfCellIncY;
        }
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: UpdateAngles()
// Desc: Advances the rotation by dwElapsed milliseconds.  The angles are kept
//       in revolutions and wrapped to 0 .. 1.
//-----------------------------------------------------------------------------
VOID CTextScreensaver::UpdateAngles( DWORD dwElapsed )
{
    static FLOAT fPhase = 0.0f;     // of the see-saw and wobble motions
    static FLOAT fRevZ = 0.0f;
    static FLOAT fRevY = 0.0f;
    static FLOAT fRevX = 0.0f;

    FLOAT fStep = (FLOAT)( m_dwRotationSpeed * dwElapsed );

    switch( m_RotType )
    {
        case spin:
            fRevY += fStep * 0.00002f;
            break;

        case seesaw:
            fPhase += fStep * 0.000025f;
            fRevY = sinf( fPhase * (2.0f * D3DX_PI) ) * 0.17f + 1.0f;
            break;

        case wobble:
        {
            fPhase += fStep * 0.000025f;
            FLOAT fAngle = fPhase * (2.0f * D3DX_PI);
            fRevY = sinf( fAngle ) * 0.17f + 1.0f;
            fRevZ = cosf( fAngle ) * 0.09f + 1.0f;
            break;
        }

        case tumble:
            fRevX += fStep * 0.000004f;
            fRevY += fStep * 0.000018f;
            fRevZ += fStep * 0.000007f;
            break;

        default:
            break;
    }

    fRevX -= (INT)fRevX;
    fRevY -= (INT)fRevY;
    fRevZ -= (INT)fRevZ;

    m_fAngleX = fRevX * (2.0f * D3DX_PI);
    m_fAngleY = fRevY * (2.0f * D3DX_PI);
    m_fAngleZ = fRevZ * (2.0f * D3DX_PI);
}




//-----------------------------------------------------------------------------
// Name: UpdateTimeString()
// Desc: Puts the current time into pszTime.  Returns TRUE if that changed the
//       string.
//-----------------------------------------------------------------------------
BOOL CTextScreensaver::UpdateTimeString( TCHAR* pszTime )
{
    TCHAR szTime[MAX_DISPLAY_STRING + 1];

    GetTimeFormat( LOCALE_USER_DEFAULT, 0, NULL, NULL, szTime, MAX_DISPLAY_STRING + 1 );

    if( lstrcmp( pszTime, szTime ) != 0 )
    {
        lstrcpy( pszTime, szTime );
        return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ss_RegistrySetup()
// Desc: Looks up the control.ini section of the OpenGL version of the
//       screen saver.  Returns TRUE if it exists.
//-----------------------------------------------------------------------------
BOOL CTextScreensaver::ss_RegistrySetup( int section, int file )
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
int CTextScreensaver::ss_GetRegistryInt( int name, int iDefault )
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
VOID CTextScreensaver::ss_GetRegistryString( int name, TCHAR* lpDefault, TCHAR* lpDest, int bufSize )
{
    TCHAR szName[BUF_SIZE+1];

    if( LoadString( m_hInstance, name, szName, BUF_SIZE ) )
        GetPrivateProfileString( g_szSectName, szName, lpDefault, lpDest, bufSize, g_szFname );
}




//-----------------------------------------------------------------------------
// Name: ExtractAndWriteSettings()
// Desc: Takes the settings from the dialog and saves them.
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ExtractAndWriteSettings( HWND hDlg )
{
    LRESULT lr;
    HKEY hKey = NULL;

    m_bSpecular = IsDlgButtonChecked( hDlg, IDC_SPECULAR );
    m_bUseCustomColor = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_COLOR );
    m_bUseCustomTexture = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_TEXTURE );
    m_bUseCustomEnvironment = IsDlgButtonChecked( hDlg, IDC_USE_CUSTOM_ENVIRONMENT );

    if( ( lr = SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_GETCURSEL, 0, 0 ) ) != CB_ERR )
        m_RotType = (RotType)lr;

    m_dwSize = (DWORD)SendDlgItemMessage( hDlg, IDC_VIEWPORTSIZE, TBM_GETPOS, 0, 0 );
    m_dwRotationSpeed = (DWORD)SendDlgItemMessage( hDlg, IDC_ROTATION_SPEED, TBM_GETPOS, 0, 0 );
    m_dwMeshQuality = (DWORD)SendDlgItemMessage( hDlg, IDC_RESOLUTION, TBM_GETPOS, 0, 0 ) * 100;

    GetDlgItemText( hDlg, IDC_DISPLAYSTRING, m_szDisplayString, MAX_DISPLAY_STRING + 1 );

    if( IsDlgButtonChecked( hDlg, IDC_RADIO_TIME ) )
        m_bUseTime = TRUE;
    else
        m_bUseTime = FALSE;

    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath, 0, NULL,
                                         REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL ) )
    {
        WriteScreenSettings( hKey );

        // (The BYTE fields of the LOGFONT are written as DWORDs.)
        RegSetValueEx( hKey, TEXT("SurfaceType"), 0, REG_DWORD, (BYTE*)&m_SurfType, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("Specular"), 0, REG_DWORD, (BYTE*)&m_bSpecular, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("SurfaceColor"), 0, REG_DWORD, (BYTE*)&m_SurfaceColor, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("CustomTexture"), 0, REG_SZ, (BYTE*)m_szCustomTexture,
                       (lstrlen(m_szCustomTexture)+1)*sizeof(TCHAR) );
        RegSetValueEx( hKey, TEXT("CustomEnvironment"), 0, REG_SZ, (BYTE*)m_szCustomEnvironment,
                       (lstrlen(m_szCustomEnvironment)+1)*sizeof(TCHAR) );
        RegSetValueEx( hKey, TEXT("UseCustomColor"), 0, REG_DWORD, (BYTE*)&m_bUseCustomColor, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("UseCustomTexture"), 0, REG_DWORD, (BYTE*)&m_bUseCustomTexture, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("UseCustomEnvironment"), 0, REG_DWORD, (BYTE*)&m_bUseCustomEnvironment, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("MeshQuality"), 0, REG_DWORD, (BYTE*)&m_dwMeshQuality, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("Size"), 0, REG_DWORD, (BYTE*)&m_dwSize, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("RotationSpeed"), 0, REG_DWORD, (BYTE*)&m_dwRotationSpeed, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("RotationStyle"), 0, REG_DWORD, (BYTE*)&m_RotType, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("DisplayString"), 0, REG_SZ, (BYTE*)m_szDisplayString,
                       (lstrlen(m_szDisplayString)+1)*sizeof(TCHAR) );
        RegSetValueEx( hKey, TEXT("DisplayTime"), 0, REG_DWORD, (BYTE*)&m_bUseTime, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("FontWeight"), 0, REG_DWORD, (BYTE*)&m_Font.lfWeight, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("FontHeight"), 0, REG_DWORD, (BYTE*)&m_Font.lfHeight, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("FontItalic"), 0, REG_DWORD, (BYTE*)&m_Font.lfItalic, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("FontCharSet"), 0, REG_DWORD, (BYTE*)&m_Font.lfCharSet, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("FontPitchFamily"), 0, REG_DWORD, (BYTE*)&m_Font.lfPitchAndFamily, sizeof(DWORD) );
        RegSetValueEx( hKey, TEXT("FontFace"), 0, REG_SZ, (BYTE*)m_Font.lfFaceName,
                       (lstrlen(m_Font.lfFaceName)+1)*sizeof(TCHAR) );

        RegCloseKey( hKey );
    }
}




//-----------------------------------------------------------------------------
// Name: SelectSurfaceColor()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SelectSurfaceColor( HWND hDlg )
{
    static COLORREF clrCustomColors[16];
    CHOOSECOLOR choosecolor;

    choosecolor.lStructSize = sizeof(choosecolor);
    choosecolor.hwndOwner = hDlg;
    choosecolor.hInstance = (HWND)m_hInstance;
    choosecolor.rgbResult = m_SurfaceColor;
    choosecolor.lpCustColors = clrCustomColors;
    choosecolor.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    choosecolor.lCustData = 0;
    choosecolor.lpfnHook = NULL;
    choosecolor.lpTemplateName = NULL;

    if( ChooseColor( &choosecolor ) )
        m_SurfaceColor = choosecolor.rgbResult;
}




//-----------------------------------------------------------------------------
// Name: SelectFont()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SelectFont( HWND hDlg )
{
    CHOOSEFONT choosefont;

    // (the fields not set here are left uninitialized by the original)
    ZeroMemory( &choosefont, sizeof(choosefont) );
    choosefont.lStructSize = sizeof(choosefont);
    choosefont.hwndOwner = hDlg;
    choosefont.hInstance = m_hInstance;
    choosefont.lpLogFont = &m_Font;
    choosefont.iPointSize = 720;
    choosefont.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_TTONLY |
                       CF_ENABLETEMPLATE | CF_FORCEFONTEXIST | CF_NOVERTFONTS;

    // a template without the size list: the size does not matter
    choosefont.lpTemplateName = MAKEINTRESOURCE( FORMATDLGORD31 );

    if( ChooseFont( &choosefont ) )
        m_Font = *choosefont.lpLogFont;
}




//-----------------------------------------------------------------------------
// Name: SelectCustomTexture()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SelectCustomTexture( HWND hDlg )
{
    TCHAR szFile[MAX_PATH] = TEXT("\0");
    TCHAR szFilter[200];
    TCHAR szTitle[100];
    TCHAR szWindowsDir[MAX_PATH+1];
    TCHAR* pszFilter;
    OPENFILENAME ofn;

    LoadString( NULL, IDS_CHOOSE_TEXTURE, szTitle, 100 );
    lstrcpy( szFile, m_szCustomTexture );

    // The filter string is stored with '#' in place of the embedded nulls
    LoadString( NULL, IDS_TEXTURE_FILTER, szFilter, 200 );
    for( pszFilter = szFilter; *pszFilter != TEXT('\0'); pszFilter++ )
    {
        if( *pszFilter == TEXT('#') )
            *pszFilter = TEXT('\0');
    }

    if( GetWindowsDirectory( szWindowsDir, MAX_PATH+1 ) == 0 )
        szWindowsDir[0] = TEXT('\0');

    // (the Windows NT 4.0 size of the structure)
    ZeroMemory( &ofn, OPENFILENAME_SIZE_VERSION_400 );
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner = hDlg;
    ofn.hInstance = m_hInstance;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrTitle = szTitle;
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = TEXT("mid");      // (sic)

    // start in the Windows directory unless there is a file to start with
    if( lstrlen( szFile ) == 0 )
        ofn.lpstrInitialDir = szWindowsDir;

    if( GetOpenFileName( &ofn ) )
        lstrcpy( m_szCustomTexture, ofn.lpstrFile );
}




//-----------------------------------------------------------------------------
// Name: SelectCustomEnvironment()
// Desc: (Unlike SelectCustomTexture(), this does not start with the current
//       file.)
//-----------------------------------------------------------------------------
VOID CTextScreensaver::SelectCustomEnvironment( HWND hDlg )
{
    TCHAR szFile[MAX_PATH] = TEXT("\0");
    TCHAR szFilter[200];
    TCHAR szTitle[100];
    TCHAR szWindowsDir[MAX_PATH+1];
    TCHAR* pszFilter;
    OPENFILENAME ofn;

    LoadString( NULL, IDS_CHOOSE_ENVIRONMENT, szTitle, 100 );

    LoadString( NULL, IDS_TEXTURE_FILTER, szFilter, 200 );
    for( pszFilter = szFilter; *pszFilter != TEXT('\0'); pszFilter++ )
    {
        if( *pszFilter == TEXT('#') )
            *pszFilter = TEXT('\0');
    }

    if( GetWindowsDirectory( szWindowsDir, MAX_PATH+1 ) == 0 )
        szWindowsDir[0] = TEXT('\0');

    // (the Windows NT 4.0 size of the structure)
    ZeroMemory( &ofn, OPENFILENAME_SIZE_VERSION_400 );
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner = hDlg;
    ofn.hInstance = m_hInstance;
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrTitle = szTitle;
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = TEXT("mid");      // (sic)

    if( lstrlen( szFile ) == 0 )
        ofn.lpstrInitialDir = szWindowsDir;

    if( GetOpenFileName( &ofn ) )
        lstrcpy( m_szCustomEnvironment, ofn.lpstrFile );
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




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CTextScreensaver::FrameMove()
{
    DWORD dwTick = GetTickCount();
    DWORD dwElapsed = dwTick - m_dwLastTick;

    m_dwLastTick = dwTick;

    if( m_floatrect.xSize == 0.0f )
    {
        // Initialize the floatrect: a square whose size depends on the "Size"
        // setting (25% .. 100% of the smaller side of the desktop) ...
        RECT rcBounds = m_rcRenderTotal;
        DWORD dwWidth = rcBounds.right - rcBounds.left;
        DWORD dwHeight = rcBounds.bottom - rcBounds.top;
        FLOAT fSize = m_dwSize * 0.1f * 0.75f + 0.25f;
        DWORD dwSize = (DWORD)( min( dwWidth, dwHeight ) * fSize );

        if( dwSize > dwWidth )
            dwSize = dwWidth;
        if( dwSize > dwHeight )
            dwSize = dwHeight;

        // ... which starts in the middle of the first monitor
        if( !m_bWindowed )
            rcBounds = m_Monitors[m_RenderUnits[0].iMonitor].rcScreen;

        m_floatrect.xMin = rcBounds.left + (rcBounds.right - dwSize - rcBounds.left) * 0.5f;
        m_floatrect.yMin = rcBounds.top + (rcBounds.bottom - dwSize - rcBounds.top) * 0.5f;
        m_floatrect.xSize = (FLOAT)dwSize;
        m_floatrect.ySize = (FLOAT)dwSize;
        m_floatrect.xVel = dwSize * 0.01f;
        if( rand() % 2 == 0 )
            m_floatrect.xVel = -m_floatrect.xVel;
        m_floatrect.yVel = dwSize * 0.01f;
        if( rand() % 2 == 0 )
            m_floatrect.yVel = -m_floatrect.yVel;
    }

    // Bounce off the edges of the desktop
    FLOAT xMinOld = m_floatrect.xMin;
    FLOAT yMinOld = m_floatrect.yMin;

    m_floatrect.xMin += m_floatrect.xVel * m_fElapsedTime * 20.0f;
    m_floatrect.yMin += m_floatrect.yVel * m_fElapsedTime * 20.0f;
    if( ( m_floatrect.xVel < 0.0f && m_floatrect.xMin < m_rcRenderTotal.left ) ||
        ( m_floatrect.xVel > 0.0f && m_floatrect.xMin + m_floatrect.xSize > m_rcRenderTotal.right ) )
    {
        m_floatrect.xMin = xMinOld;     // undo the move
        m_floatrect.xVel = -m_floatrect.xVel; // change direction
    }
    if( ( m_floatrect.yVel < 0.0f && m_floatrect.yMin < m_rcRenderTotal.top ) ||
        ( m_floatrect.yVel > 0.0f && m_floatrect.yMin + m_floatrect.ySize > m_rcRenderTotal.bottom ) )
    {
        m_floatrect.yMin = yMinOld;
        m_floatrect.yVel = -m_floatrect.yVel;
    }

    UpdateAngles( dwElapsed );

    if( m_bUseTime )
    {
        if( UpdateTimeString( m_szDisplayString ) )
            m_dwMeshUpdateCounter++;    // tells Render() that the mesh is out of date
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
    D3DVIEWPORT8 vp;

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
        return S_OK; // no intersection, so nothing further to render on this screen

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
    FLOAT l,r,b,t;
    FLOAT fCenterX = (rcFloatThisScreen.right + rcFloatThisScreen.left) * 0.5f;
    FLOAT fCenterY = (rcFloatThisScreen.bottom + rcFloatThisScreen.top) * 0.5f;
    l = (rcFloatThisScreenClipped.left - fCenterX) / (rcFloatThisScreen.left - fCenterX) * -0.8f;
    r = (rcFloatThisScreenClipped.right - fCenterX) / (rcFloatThisScreen.right - fCenterX) * 0.8f;
    b = (rcFloatThisScreenClipped.top - fCenterY) / (rcFloatThisScreen.top - fCenterY) * -0.8f;
    t = (rcFloatThisScreenClipped.bottom - fCenterY) / (rcFloatThisScreen.bottom - fCenterY) * 0.8f;
    D3DXMatrixPerspectiveOffCenterLH( &matProj, l, r, b, t, 1.0f, 50.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION , &matProj );

    // Mesh updating happens in Render() instead of FrameMove() since there is
    // one mesh per device
    if( m_pDeviceObjects->m_dwMeshUpdateCounter != m_dwMeshUpdateCounter )
    {
        BuildTextMesh( m_szDisplayString );
        m_pDeviceObjects->m_dwMeshUpdateCounter = m_dwMeshUpdateCounter;
    }

    m_pd3dDevice->BeginScene();

    // Set world matrix: center the text, rotate it, and move it far enough
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
    D3DXVECTOR3 vEye( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 1.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &m_matView, &vEye, &vAt, &vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    // Set up misc. render states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, m_bSpecular );

    // The mesh is built in units of the em square anyway; the height only
    // has to be something sensible while the font is created
    LONG lHeightSave = m_Font.lfHeight;
    m_Font.lfHeight = 100;
    m_hFont = CreateFontIndirect( &m_Font );
    m_Font.lfHeight = lHeightSave;

    if( m_hFont == NULL )
        return E_FAIL;

    // Initialize the display string if it shows the time
    if( m_bUseTime )
        UpdateTimeString( m_szDisplayString );

    if( FAILED( hr = BuildTextMesh( m_szDisplayString ) ) )
        return hr;

    // Rotate around the middle of the text
    m_fTextOffsetX = (m_fTextMaxX + m_fTextMinX) * -0.5f;
    m_fTextOffsetY = (m_fTextMaxY + m_fTextMinY) * -0.5f;

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
                LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCE( IDR_ENVIRONMENT ),
                                         TEXT("JPG"), &m_pDeviceObjects->m_pTexture );
            }
            break;

        case texture:
            if( m_bUseCustomTexture )
                m_pDeviceObjects->m_pTexture = CreateTextureFromFile( m_szCustomTexture );
            if( m_pDeviceObjects->m_pTexture == NULL )
            {
                LoadTextureFromResource( m_pd3dDevice, MAKEINTRESOURCE( IDR_TEXTURE ),
                                         TEXT("JPG"), &m_pDeviceObjects->m_pTexture );
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
// Name: ss_ReadSettings()
// Desc: Reads the settings of the OpenGL version from control.ini, so that
//       they carry over after an upgrade.
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ss_ReadSettings()
{
    int nAttributes;

    if( ss_RegistrySetup( IDS_SAVERNAME, IDS_INIFILE ) )
    {
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
        m_dwSize = (DWORD)( ( (DWORD)ss_GetRegistryInt( IDS_SIZE, 0 ) * 0.01f * 90.0f + 10.0f ) * 0.1f );
        m_dwRotationSpeed = (DWORD)( ( (DWORD)ss_GetRegistryInt( IDS_SPEED, 0 ) * 0.01f * 190.0f + 10.0f ) * 0.1f );

        ss_GetRegistryString( IDS_FONT, TEXT(""), m_Font.lfFaceName, LF_FACESIZE );

        // FontAttributes: bit 0 = bold, bit 1 = italic
        nAttributes = ss_GetRegistryInt( IDS_FONT_ATTRIBUTES, 0 );
        if( nAttributes >= 0 )
        {
            m_Font.lfWeight = (nAttributes & 1) ? FW_BOLD : FW_NORMAL;
            m_Font.lfItalic = (nAttributes & 2) ? TRUE : FALSE;
        }

        m_Font.lfCharSet = (BYTE)ss_GetRegistryInt( IDS_CHARSET, 0 );

        ss_GetRegistryString( IDS_TEXT, TEXT(""), m_szDisplayString, MAX_DISPLAY_STRING );

        // SurfStyle: 0 = solid color, 1 = texture
        m_SurfType = (SurfType)ss_GetRegistryInt( IDS_SURFSTYLE, 0 );
        if( m_SurfType > color )
            m_SurfType = texture;

        ss_GetRegistryString( IDS_TEXTURE, NULL, m_szCustomTexture, MAX_PATH );
        if( lstrlen( m_szCustomTexture ) > 0 )
            m_bUseCustomTexture = TRUE;
    }
}




//-----------------------------------------------------------------------------
// Name: InitItems()
// Desc: Set up the dialog controls from the settings
//-----------------------------------------------------------------------------
VOID CTextScreensaver::InitItems( HWND hDlg )
{
    TCHAR sz[100];

    // Set the limit of the text box
    SendDlgItemMessage( hDlg, IDC_DISPLAYSTRING, EM_SETLIMITTEXT, MAX_DISPLAY_STRING, 0 );

    // Set the text
    SetDlgItemText( hDlg, IDC_DISPLAYSTRING, m_szDisplayString );

    // Time or text
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

    // Surface type
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

    // Specular
    SendDlgItemMessage( hDlg, IDC_SPECULAR, BM_SETCHECK, m_bSpecular ? BST_CHECKED : BST_UNCHECKED, 0 );

    EnableWindow( GetDlgItem( hDlg, IDC_SURFACE_COLOR ), m_SurfType == color );

    SendDlgItemMessage( hDlg, IDC_USE_CUSTOM_COLOR, BM_SETCHECK, m_bUseCustomColor ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessage( hDlg, IDC_USE_CUSTOM_TEXTURE, BM_SETCHECK, m_bUseCustomTexture ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessage( hDlg, IDC_USE_CUSTOM_ENVIRONMENT, BM_SETCHECK, m_bUseCustomEnvironment ? BST_CHECKED : BST_UNCHECKED, 0 );

    EnableTextureWindows( hDlg, m_SurfType );

    // Rotation style
    LoadString( NULL, IDS_ROTATION_NONE, sz, 100 );
    SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadString( NULL, IDS_ROTATION_SPIN, sz, 100 );
    SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadString( NULL, IDS_ROTATION_SEESAW, sz, 100 );
    SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadString( NULL, IDS_ROTATION_WOBBLE, sz, 100 );
    SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    LoadString( NULL, IDS_ROTATION_TUMBLE, sz, 100 );
    SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_ADDSTRING, 0, (LPARAM)sz );
    SendDlgItemMessage( hDlg, IDC_ROTATION_STYLE, CB_SETCURSEL, m_RotType, 0 );

    // Speed, resolution and size sliders
    SendDlgItemMessage( hDlg, IDC_ROTATION_SPEED, TBM_SETRANGE, TRUE, MAKELONG( 1, 20 ) );
    SendDlgItemMessage( hDlg, IDC_ROTATION_SPEED, TBM_SETPOS, TRUE, m_dwRotationSpeed );
    SendDlgItemMessage( hDlg, IDC_RESOLUTION, TBM_SETRANGE, TRUE, MAKELONG( 1, 10 ) );
    SendDlgItemMessage( hDlg, IDC_RESOLUTION, TBM_SETPOS, TRUE, m_dwMeshQuality / 100 );
    SendDlgItemMessage( hDlg, IDC_VIEWPORTSIZE, TBM_SETRANGE, TRUE, MAKELONG( 1, 10 ) );
    SendDlgItemMessage( hDlg, IDC_VIEWPORTSIZE, TBM_SETPOS, TRUE, m_dwSize );
}




//-----------------------------------------------------------------------------
// Name: ReadSettings()
// Desc: Read user preferences from the registry
//-----------------------------------------------------------------------------
VOID CTextScreensaver::ReadSettings()
{
    HKEY hKey;
    DWORD dwType = REG_DWORD;
    DWORD dwLength = sizeof(DWORD);

    // Defaults
    LoadString( NULL, IDS_DEFAULT_TEXT, m_szDisplayString, MAX_DISPLAY_STRING );
    m_Font.lfHeight = 96;
    m_Font.lfWidth = 0;
    m_Font.lfEscapement = 0;
    m_Font.lfOrientation = 0;
    m_Font.lfWeight = 0;
    m_Font.lfItalic = FALSE;
    m_Font.lfUnderline = FALSE;
    m_Font.lfStrikeOut = FALSE;
    m_Font.lfCharSet = DEFAULT_CHARSET;
    m_Font.lfOutPrecision = OUT_DEFAULT_PRECIS;
    m_Font.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    m_Font.lfQuality = DEFAULT_QUALITY;
    m_Font.lfPitchAndFamily = DEFAULT_PITCH;
    LoadString( NULL, IDS_DEFAULT_FONT, m_Font.lfFaceName, LF_FACESIZE );

    m_SurfType = environment;
    m_bSpecular = TRUE;
    m_RotType = spin;
    m_bUseTime = FALSE;
    m_dwMeshQuality = 500;
    m_dwRotationSpeed = 10;
    m_dwSize = 10;
    m_SurfaceColor = RGB( 119, 119, 119 );
    m_bUseCustomColor = FALSE;
    m_bUseCustomTexture = FALSE;
    m_bUseCustomEnvironment = FALSE;
    lstrcpy( m_szCustomTexture, TEXT("") );
    lstrcpy( m_szCustomEnvironment, TEXT("") );

    // Read OpenGL settings first, so OS upgrade cases work
    ss_ReadSettings();

    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath, 0, NULL,
                                         REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL ) )
    {
        // (The BYTE fields of the LOGFONT are read as DWORDs.)
        dwLength = (MAX_DISPLAY_STRING + 1) * sizeof(TCHAR);
        RegQueryValueEx( hKey, TEXT("DisplayString"), NULL, &dwType, (BYTE*)m_szDisplayString, &dwLength );
        dwLength = sizeof(DWORD);
        RegQueryValueEx( hKey, TEXT("FontHeight"), NULL, &dwType, (BYTE*)&m_Font.lfHeight, &dwLength );
        RegQueryValueEx( hKey, TEXT("FontWeight"), NULL, &dwType, (BYTE*)&m_Font.lfWeight, &dwLength );
        RegQueryValueEx( hKey, TEXT("FontItalic"), NULL, &dwType, (BYTE*)&m_Font.lfItalic, &dwLength );
        RegQueryValueEx( hKey, TEXT("FontCharSet"), NULL, &dwType, (BYTE*)&m_Font.lfCharSet, &dwLength );
        RegQueryValueEx( hKey, TEXT("FontPitchFamily"), NULL, &dwType, (BYTE*)&m_Font.lfPitchAndFamily, &dwLength );
        dwLength = sizeof(m_Font.lfFaceName);
        RegQueryValueEx( hKey, TEXT("FontFace"), NULL, &dwType, (BYTE*)m_Font.lfFaceName, &dwLength );
        dwLength = sizeof(DWORD);
        RegQueryValueEx( hKey, TEXT("DisplayTime"), NULL, &dwType, (BYTE*)&m_bUseTime, &dwLength );
        RegQueryValueEx( hKey, TEXT("MeshQuality"), NULL, &dwType, (BYTE*)&m_dwMeshQuality, &dwLength );
        if( m_dwMeshQuality > 1000 )
            m_dwMeshQuality = 1000;
        RegQueryValueEx( hKey, TEXT("SurfaceType"), NULL, &dwType, (BYTE*)&m_SurfType, &dwLength );
        RegQueryValueEx( hKey, TEXT("Specular"), NULL, &dwType, (BYTE*)&m_bSpecular, &dwLength );
        RegQueryValueEx( hKey, TEXT("RotationSpeed"), NULL, &dwType, (BYTE*)&m_dwRotationSpeed, &dwLength );
        RegQueryValueEx( hKey, TEXT("Size"), NULL, &dwType, (BYTE*)&m_dwSize, &dwLength );
        if( m_dwSize < 1 )
            m_dwSize = 1;
        if( m_dwSize > 10 )
            m_dwSize = 10;
        RegQueryValueEx( hKey, TEXT("RotationStyle"), NULL, &dwType, (BYTE*)&m_RotType, &dwLength );
        RegQueryValueEx( hKey, TEXT("SurfaceColor"), NULL, &dwType, (BYTE*)&m_SurfaceColor, &dwLength );
        RegQueryValueEx( hKey, TEXT("UseCustomColor"), NULL, &dwType, (BYTE*)&m_bUseCustomColor, &dwLength );
        RegQueryValueEx( hKey, TEXT("UseCustomTexture"), NULL, &dwType, (BYTE*)&m_bUseCustomTexture, &dwLength );
        RegQueryValueEx( hKey, TEXT("UseCustomEnvironment"), NULL, &dwType, (BYTE*)&m_bUseCustomEnvironment, &dwLength );
        dwLength = sizeof(m_szCustomTexture);
        RegQueryValueEx( hKey, TEXT("CustomTexture"), NULL, &dwType, (BYTE*)m_szCustomTexture, &dwLength );
        dwLength = sizeof(m_szCustomEnvironment);
        RegQueryValueEx( hKey, TEXT("CustomEnvironment"), NULL, &dwType, (BYTE*)m_szCustomEnvironment, &dwLength );

        ReadScreenSettings( hKey );

        RegCloseKey( hKey );
    }
}




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc:
//-----------------------------------------------------------------------------
VOID CTextScreensaver::DoConfig()
{
    ReadSettings();
    DialogBox( NULL, MAKEINTRESOURCE( IDD_SETTINGS ), m_hWndParent, SettingsDialogProcStub );
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
                        SelectSurfaceColor( hDlg );
                        break;

                    case IDC_BROWSE_TEXTURE:
                        SelectCustomTexture( hDlg );
                        break;

                    case IDC_BROWSE_ENVIRONMENT:
                        SelectCustomEnvironment( hDlg );
                        break;

                    case IDC_CHOOSEFONT:
                        SelectFont( hDlg );
                        break;
                }
            }
            else if( HIWORD( wParam ) == EN_CHANGE && LOWORD( wParam ) == IDC_DISPLAYSTRING )
            {
                GetDlgItemText( hDlg, IDC_DISPLAYSTRING, m_szDisplayString, MAX_DISPLAY_STRING );
            }
            break;
    }

    return FALSE;
}
