//-----------------------------------------------------------------------------
// File: pipes.cpp
//
// Desc: 3D Pipes screen saver (sspipes.scr), Direct3D 8 version: the screen
//       saver class, its settings and the settings dialog.
//
//       Reconstructed from pipes.obj:
//           d:\nt\shell\osshell\control\scrnsave\pipes\obj\i386\pipes.obj
//-----------------------------------------------------------------------------
#include "stdafx.h"

#ifndef NO_D3D8RGB
// Software rasterizer of the DirectX 8 DDK, linked from d3d8rgb.lib (a
// library internal to the Windows source tree: public\internal\multimedia).
extern "C" void* D3D8RGBRasterizer;
#endif

#define BUF_SIZE        255

CPipesScreensaver*  g_pMyPipesScreensaver = NULL;

// Where the OpenGL version of this screen saver kept its settings
TCHAR               g_szSectName[BUF_SIZE+1];
TCHAR               g_szFname[BUF_SIZE+1];




//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow )
{
    HRESULT hr;
    CPipesScreensaver pipesSS;

    if( FAILED( hr = pipesSS.Create( hInst ) ) )
    {
        pipesSS.DisplayErrorMsg( hr );
        return 0;
    }

    return pipesSS.Run();
}




//-----------------------------------------------------------------------------
// Name: CPipesScreensaver()
// Desc: Constructor
//-----------------------------------------------------------------------------
CPipesScreensaver::CPipesScreensaver()
{
    g_pMyPipesScreensaver = this;

    InitCommonControls();

    ZeroMemory( m_DeviceObjectsArray, sizeof(m_DeviceObjectsArray) );

    LoadString( NULL, IDS_DESCRIPTION, m_strWindowTitle, 200 );
    m_bUseDepthBuffer = TRUE;
    m_SwapEffectFullscreen = D3DSWAPEFFECT_COPY;
    m_SwapEffectWindowed = D3DSWAPEFFECT_COPY_VSYNC;
    m_pDeviceObjects = NULL;

    lstrcpy( m_strRegPath, TEXT("Software\\Microsoft\\ScreenSavers\\Pipes") );

    m_config.bMultiPipes = FALSE;
    m_config.nJointType = JOINT_ELBOW;
    m_config.bTextured = FALSE;
    m_config.bDefaultTexture = TRUE;
    lstrcpy( m_config.strTextureName[0], TEXT("") );
    m_config.bFlexMode = FALSE;
    m_config.dwTesselFactor = 0;
    m_config.dwSpeed = 16;

    m_iDevice = 0;

    RandInit();
}




//-----------------------------------------------------------------------------
// Name: RegisterSoftwareDevice()
// Desc: This can register the D3D8RGBRasterizer or any other
//       pluggable software rasterizer.
//-----------------------------------------------------------------------------
HRESULT CPipesScreensaver::RegisterSoftwareDevice()
{
#ifndef NO_D3D8RGB
    m_pD3D->RegisterSoftwareDevice( D3D8RGBRasterizer );
#endif

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDevice()
// Desc:
//-----------------------------------------------------------------------------
VOID CPipesScreensaver::SetDevice( UINT iDevice )
{
    m_pDeviceObjects = &m_DeviceObjectsArray[iDevice];
    m_iDevice = iDevice;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CPipesScreensaver::FrameMove()
{
    m_pDeviceObjects->m_pState->FrameMove( m_fElapsedTime );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DoPaint()
// Desc: The window needs painting: the pipes have to be drawn again.
//-----------------------------------------------------------------------------
VOID CPipesScreensaver::DoPaint( HWND hwnd, HDC hdc )
{
    CD3DScreensaver::DoPaint( hwnd, hdc );

    for( int i = 0; i < PIPES_MAX_DEVICES; i++ )
    {
        if( m_DeviceObjectsArray[i].m_pState != NULL )
            m_DeviceObjectsArray[i].m_pState->Repaint();
    }
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering.  The pipes grow by one segment per frame; the speed
//       setting sets the time between two segments.
//-----------------------------------------------------------------------------
HRESULT CPipesScreensaver::Render()
{
    if( m_config.dwSpeed != 20 )
    {
        FLOAT fTime = DXUtil_Timer( TIMER_GETAPPTIME );
        FLOAT fElapsed = fTime - m_pDeviceObjects->m_pState->m_fLastTime;
        if( fElapsed <= (20 - m_config.dwSpeed) * 0.005f )
        {
            // Not yet: give the processor away (once per frame)
            if( m_iDevice == 0 )
                Sleep( 10 );
            return S_OK;
        }
        m_pDeviceObjects->m_pState->m_fLastTime = fTime;
    }

    if( SUCCEEDED( m_pd3dDevice->BeginScene() ) )
    {
        m_pDeviceObjects->m_pState->Render();
        m_pd3dDevice->EndScene();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CPipesScreensaver::RestoreDeviceObjects()
{
    if( m_pd3dDevice == NULL )
        return S_OK;

    m_pDeviceObjects->m_pState = new STATE( &m_config );
    m_pDeviceObjects->m_pState->InitDeviceObjects( m_pd3dDevice );
    m_pDeviceObjects->m_pState->RestoreDeviceObjects();

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0L );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CPipesScreensaver::InvalidateDeviceObjects()
{
    m_pDeviceObjects->m_pState->InvalidateDeviceObjects();
    m_pDeviceObjects->m_pState->DeleteDeviceObjects();
    SAFE_DELETE( m_pDeviceObjects->m_pState );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: iRand(), iRand2(), fRand(), RandInit()
// Desc: Random numbers: 0 .. max-1, min .. max, min .. max
//-----------------------------------------------------------------------------
int CPipesScreensaver::iRand( int max )
{
    return (int)( rand() * (1.0f / (RAND_MAX + 1)) * max );
}

int CPipesScreensaver::iRand2( int min, int max )
{
    if( min == max )
        return min;

    if( max < min )
    {
        int t = max;
        max = min;
        min = t;
    }

    return min + (int)( rand() * (1.0f / (RAND_MAX + 1)) * (max - min + 1) );
}

FLOAT CPipesScreensaver::fRand( FLOAT min, FLOAT max )
{
    return rand() * (1.0f / RAND_MAX) * (max - min) + min;
}

VOID CPipesScreensaver::RandInit()
{
    struct _timeb time;

    _ftime( &time );
    srand( time.millitm );

    for( int i = 0; i < 10; i++ )
        rand();
}




//-----------------------------------------------------------------------------
// Name: SelectTextureFile()
// Desc: Lets the user choose the texture.  Returns TRUE if one was chosen.
//-----------------------------------------------------------------------------
BOOL CPipesScreensaver::SelectTextureFile( HWND hDlg, TCHAR* strFile )
{
    TCHAR strFilter[MAX_PATH];
    TCHAR strTitle[MAX_PATH];
    TCHAR strWindowsDir[MAX_PATH + 1];
    TCHAR strName[MAX_PATH];
    TCHAR* pch;
    OPENFILENAME ofn;

    // The filter string is stored with '#' in place of the embedded nulls
    LoadString( NULL, IDS_TEXTUREFILTER, strFilter, MAX_PATH );
    for( pch = strFilter; *pch != TEXT('\0'); pch++ )
    {
        if( *pch == TEXT('#') )
            *pch = TEXT('\0');
    }
    LoadString( NULL, IDS_TEXTUREDIALOGTITLE, strTitle, MAX_PATH );
    LoadString( NULL, IDS_SAVERNAME, strName, MAX_PATH );   // (not used)

    if( GetWindowsDirectory( strWindowsDir, MAX_PATH + 1 ) == 0 )
        strWindowsDir[0] = TEXT('\0');

    // (the Windows NT 4.0 size of the structure)
    ZeroMemory( &ofn, OPENFILENAME_SIZE_VERSION_400 );
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner = hDlg;
    ofn.hInstance = m_hInstance;
    ofn.lpstrFilter = strFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = strFile;
    ofn.nMaxFile = MAX_PATH * sizeof(TCHAR);
    ofn.lpstrTitle = strTitle;
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = TEXT("BMP");

    // start in the Windows directory unless there is a file to start with
    if( lstrlen( strFile ) == 0 )
        ofn.lpstrInitialDir = strWindowsDir;

    if( GetOpenFileName( &ofn ) )
    {
        lstrcpy( strFile, ofn.lpstrFile );
        return TRUE;
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ss_RegistrySetup()
// Desc: Looks up the control.ini section of the OpenGL version of the
//       screen saver.  Returns TRUE if it exists.
//-----------------------------------------------------------------------------
BOOL CPipesScreensaver::ss_RegistrySetup( int section, int file )
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
int CPipesScreensaver::ss_GetRegistryInt( int name, int iDefault )
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
VOID CPipesScreensaver::ss_GetRegistryString( int name, TCHAR* lpDefault, TCHAR* lpDest, int bufSize )
{
    TCHAR szName[BUF_SIZE+1];

    if( LoadString( m_hInstance, name, szName, BUF_SIZE ) )
        GetPrivateProfileString( g_szSectName, szName, lpDefault, lpDest, bufSize, g_szFname );
}




//-----------------------------------------------------------------------------
// Name: ss_ReadSettings()
// Desc: Reads the settings of the OpenGL version from control.ini, so that
//       they carry over after an upgrade.
//-----------------------------------------------------------------------------
VOID CPipesScreensaver::ss_ReadSettings()
{
    if( ss_RegistrySetup( IDS_SECTION, IDS_INIFILE ) )
    {
        m_config.nJointType = ss_GetRegistryInt( IDS_JOINT_TYPE, 0 );

        // SurfStyle: 0 = solid, 1 = textured
        m_config.bTextured = ( ss_GetRegistryInt( IDS_SURFSTYLE, 0 ) == 1 );
        if( m_config.bTextured )
            m_config.bDefaultTexture = TRUE;

        m_config.bMultiPipes = ss_GetRegistryInt( IDS_MULTIPIPES, 0 );

        ss_GetRegistryString( IDS_TEXTURE, NULL, m_config.strTextureName[0], MAX_PATH );
        if( lstrlen( m_config.strTextureName[0] ) > 0 )
            m_config.bDefaultTexture = FALSE;
    }
}




//-----------------------------------------------------------------------------
// Name: ReadSettings()
// Desc: Read user preferences from the registry
//-----------------------------------------------------------------------------
VOID CPipesScreensaver::ReadSettings()
{
    HKEY hkey;

    // Read the OpenGL settings first, so OS upgrade cases work
    ss_ReadSettings();

    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ) )
    {
        ReadScreenSettings( hkey );

        DXUtil_ReadBoolRegKey( hkey, TEXT("MultiPipes"), &m_config.bMultiPipes, m_config.bMultiPipes );
        DXUtil_ReadBoolRegKey( hkey, TEXT("Textured"), &m_config.bTextured, m_config.bTextured );
        DXUtil_ReadBoolRegKey( hkey, TEXT("Default Texture"), &m_config.bDefaultTexture, m_config.bDefaultTexture );
        DXUtil_ReadIntRegKey( hkey, TEXT("Tessel Factor"), &m_config.dwTesselFactor, m_config.dwTesselFactor );
        if( m_config.dwTesselFactor > 3 )
            m_config.dwTesselFactor = 4;
        DXUtil_ReadIntRegKey( hkey, TEXT("Joint Type"), (DWORD*)&m_config.nJointType, m_config.nJointType );
        if( (DWORD)m_config.nJointType > 3 )
            m_config.nJointType = JOINT_ELBOW;
        DXUtil_ReadIntRegKey( hkey, TEXT("Speed"), &m_config.dwSpeed, m_config.dwSpeed );
        DXUtil_ReadStringRegKey( hkey, TEXT("Texture Name"), m_config.strTextureName[0], MAX_PATH,
                                 m_config.strTextureName[0] );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: WriteSettings()
// Desc: Write user preferences to the registry
//-----------------------------------------------------------------------------
VOID CPipesScreensaver::WriteSettings()
{
    HKEY hkey;

    if( ERROR_SUCCESS == RegCreateKeyEx( HKEY_CURRENT_USER, m_strRegPath,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL ) )
    {
        WriteScreenSettings( hkey );

        DXUtil_WriteBoolRegKey( hkey, TEXT("MultiPipes"), m_config.bMultiPipes );
        DXUtil_WriteBoolRegKey( hkey, TEXT("Textured"), m_config.bTextured );
        DXUtil_WriteBoolRegKey( hkey, TEXT("Default Texture"), m_config.bDefaultTexture );
        DXUtil_WriteIntRegKey( hkey, TEXT("Tessel Factor"), m_config.dwTesselFactor );
        DXUtil_WriteIntRegKey( hkey, TEXT("Joint Type"), m_config.nJointType );
        DXUtil_WriteStringRegKey( hkey, TEXT("Texture Name"), m_config.strTextureName[0] );
        DXUtil_WriteIntRegKey( hkey, TEXT("Speed"), m_config.dwSpeed );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: ConfigureDialogProcHelper()
// Desc:
//-----------------------------------------------------------------------------
INT_PTR WINAPI ConfigureDialogProcHelper( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    return g_pMyPipesScreensaver->ConfigureDialogProc( hDlg, uMsg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc:
//-----------------------------------------------------------------------------
VOID CPipesScreensaver::DoConfig()
{
    DialogBox( NULL, MAKEINTRESOURCE( IDD_CONFIGURE ), m_hWndParent, ConfigureDialogProcHelper );
}




//-----------------------------------------------------------------------------
// Name: ConfigureDialogProc()
// Desc: The settings dialog.  The settings are edited in a copy that is
//       kept (and saved) when the dialog is closed with OK.
//-----------------------------------------------------------------------------
INT_PTR WINAPI CPipesScreensaver::ConfigureDialogProc( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    static CONFIG s_config;

    switch( uMsg )
    {
    case WM_INITDIALOG:
        {
            HWND hCombo;
            TCHAR szJointType[4][MAX_PATH];
            int i;

            ReadSettings();

            s_config = m_config;
            lstrcpy( s_config.strTextureName[0], m_config.strTextureName[0] );

            CheckRadioButton( hDlg, IDC_RADIO_SINGLE_PIPE, IDC_RADIO_MULTIPLE_PIPES,
                s_config.bMultiPipes ? IDC_RADIO_MULTIPLE_PIPES : IDC_RADIO_SINGLE_PIPE );

            hCombo = GetDlgItem( hDlg, IDC_COMBO_JOINT_TYPE );
            SendMessage( hCombo, CB_RESETCONTENT, 0, 0 );
            for( i = 0; i < 4; i++ )
            {
                LoadString( NULL, IDS_JOINT_ELBOW + i, szJointType[i], MAX_PATH );
                SendMessage( hCombo, CB_ADDSTRING, 0, (LPARAM)szJointType[i] );
            }
            SendMessage( hCombo, CB_SETCURSEL, s_config.nJointType, 0 );

            if( s_config.bTextured )
                CheckRadioButton( hDlg, IDC_RADIO_SOLID, IDC_RADIO_TEX, IDC_RADIO_TEX );
            else
                CheckRadioButton( hDlg, IDC_RADIO_SOLID, IDC_RADIO_TEX, IDC_RADIO_SOLID );
            EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_TEXTURE ), s_config.bTextured );

            SendDlgItemMessage( hDlg, IDC_SPEED_SLIDER, TBM_SETRANGE, TRUE, MAKELONG( 0, 20 ) );
            SendDlgItemMessage( hDlg, IDC_SPEED_SLIDER, TBM_SETPOS, TRUE, m_config.dwSpeed );
            SendDlgItemMessage( hDlg, IDC_SPEED_SLIDER, TBM_SETPAGESIZE, 0, 5 );
            SendDlgItemMessage( hDlg, IDC_SPEED_SLIDER, TBM_SETLINESIZE, 0, 1 );
        }
        break;

    case WM_COMMAND:
        switch( LOWORD( wParam ) )
        {
        case IDOK:
            s_config.bMultiPipes = IsDlgButtonChecked( hDlg, IDC_RADIO_MULTIPLE_PIPES );
            s_config.nJointType = (int)SendMessage( GetDlgItem( hDlg, IDC_COMBO_JOINT_TYPE ),
                                                    CB_GETCURSEL, 0, 0 );
            s_config.bTextured = IsDlgButtonChecked( hDlg, IDC_RADIO_TEX );
            s_config.dwSpeed = (DWORD)SendDlgItemMessage( hDlg, IDC_SPEED_SLIDER, TBM_GETPOS, 0, 0 );

            m_config = s_config;
            lstrcpy( m_config.strTextureName[0], s_config.strTextureName[0] );

            WriteSettings();
            EndDialog( hDlg, IDOK );
            break;

        case IDCANCEL:
            EndDialog( hDlg, IDCANCEL );
            break;

        case IDC_RADIO_SOLID:
            EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_TEXTURE ), FALSE );
            break;

        case IDC_RADIO_TEX:
            EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_TEXTURE ), TRUE );
            break;

        case IDC_BUTTON_TEXTURE:
            if( SelectTextureFile( hDlg, s_config.strTextureName[0] ) )
            {
                CheckRadioButton( hDlg, IDC_RADIO_SOLID, IDC_RADIO_TEX, IDC_RADIO_TEX );
                s_config.bDefaultTexture = FALSE;
            }
            else
            {
                s_config.bDefaultTexture = TRUE;
            }
            break;

        case IDC_SCREENSETTINGS:
            DoScreenSettingsDialog( hDlg );
            break;
        }
        break;
    }

    return FALSE;
}
