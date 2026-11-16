//-----------------------------------------------------------------------------
// File: auroraconfig.cpp
//
// Desc: Settings of the Aurora screen saver.
//
//       Aurora once had a real settings dialog (the IDD_AURORA_SETTINGS
//       template - layers, brightness, speed, amplitude, four presets and
//       "Screensaver by Stephen Coy" - still ships in Aurora.scr.mui), but in
//       the released build the Settings button only shows a task dialog that
//       says there is nothing to configure.  The values themselves are still
//       read from the registry, so they can be changed by hand.
//
//       Original object (625 bytes of code):
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\auroraconfig.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include "d3dsaver9.h"
#include "aurora.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc: "This screen saver has no options that you can set."
//-----------------------------------------------------------------------------
VOID AuroraScreensaver::DoConfig()
{
    ReadSettings();

    TaskDialog( m_hWndParent, NULL,
                MAKEINTRESOURCEW( IDS_CONFIG_TITLE ),
                MAKEINTRESOURCEW( IDS_CONFIG_NOOPTIONS ),
                MAKEINTRESOURCEW( IDS_CONFIG_NOOPTIONS_TEXT ),
                TDCBF_OK_BUTTON, TD_INFORMATION_ICON, NULL );
}




//-----------------------------------------------------------------------------
// Name: ReadSettings()
// Desc: Read user preferences from the registry.  The three float settings
//       are stored as the raw bits of a float in a REG_DWORD value.
//-----------------------------------------------------------------------------
VOID AuroraScreensaver::ReadSettings()
{
    HKEY  hkey;
    DWORD dwType = REG_DWORD;
    DWORD dwLength = sizeof(DWORD);
    DWORD dwSpanMultiMon = 0;

    if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_CURRENT_USER, m_strRegPath, 0, KEY_READ, &hkey ) )
    {
        if( ERROR_SUCCESS == RegQueryValueEx( hkey, TEXT("SpanMultiMon"), NULL, &dwType,
                                              (BYTE*)&dwSpanMultiMon, &dwLength ) )
        {
            m_bSpanMultiMon = ( dwSpanMultiMon != 0 );
        }
        RegQueryValueEx( hkey, TEXT("NumLayers"), NULL, &dwType, (BYTE*)&m_nNumLayers, &dwLength );
        RegQueryValueEx( hkey, TEXT("Brightness"), NULL, &dwType, (BYTE*)&m_fBrightness, &dwLength );
        RegQueryValueEx( hkey, TEXT("Speed"), NULL, &dwType, (BYTE*)&m_fSpeed, &dwLength );
        RegQueryValueEx( hkey, TEXT("Amplitude"), NULL, &dwType, (BYTE*)&m_fAmplitude, &dwLength );

        ValidateSettings();

        ReadScreenSettings( hkey );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: ValidateSettings()
// Desc: Clamp everything that came out of the registry to a sane range.
//-----------------------------------------------------------------------------
VOID AuroraScreensaver::ValidateSettings()
{
    if( m_nNumLayers < 1 )
        m_nNumLayers = 1;
    else if( m_nNumLayers > AURORA_MAX_LAYERS )
        m_nNumLayers = AURORA_MAX_LAYERS;

    if( m_fBrightness < 0.01f )
        m_fBrightness = 0.01f;
    else if( m_fBrightness > 1.0f )
        m_fBrightness = 1.0f;

    if( m_fSpeed < 0.001f )
        m_fSpeed = 0.001f;
    else if( m_fSpeed > 0.05f )
        m_fSpeed = 0.05f;

    if( m_fAmplitude < 0.0f )
        m_fAmplitude = 0.0f;
    else if( m_fAmplitude > 10.0f )
        m_fAmplitude = 10.0f;
}
