//-----------------------------------------------------------------------------
// File: bubblesconfig.cpp
//
// Desc: Settings of the Bubbles screen saver.
//
//       Like Aurora, Bubbles once had a real settings dialog (the
//       IDD_BUBBLES_SETTINGS template still ships in Bubbles.scr.mui), but the
//       released build only shows a task dialog that says there is nothing to
//       configure.  The values are still read from the registry.
//
//       Original object (939 bytes of code):
//           shell\osshell\control\scrnsave\bubbles\objfre\amd64\bubblesconfig.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include "d3dsaver9.h"
#include "bubbles.h"
#include "resource.h"




//-----------------------------------------------------------------------------
// Name: DoConfig()
// Desc: "This screen saver has no options that you can set."
//-----------------------------------------------------------------------------
VOID BubblesScreensaver::DoConfig()
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
// Desc: Read user preferences from the registry.  The float settings are
//       stored as the raw bits of a float in a REG_DWORD value.
//-----------------------------------------------------------------------------
VOID BubblesScreensaver::ReadSettings()
{
    HKEY  hkey;
    DWORD dwType = REG_DWORD;
    DWORD dwLength;
    DWORD dwSpanMultiMon = 0;
    DWORD dwShowBubbles = 0;
    DWORD dwShowShadows = 0;
    DWORD dwMaterialGlass = 0;

    if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_CURRENT_USER, m_strRegPath, 0, KEY_READ, &hkey ) )
    {
        dwLength = sizeof(DWORD);
        if( ERROR_SUCCESS == RegQueryValueEx( hkey, TEXT("SpanMultiMon"), NULL, &dwType,
                                              (BYTE*)&dwSpanMultiMon, &dwLength ) )
        {
            m_bSpanMultiMon = ( dwSpanMultiMon != 0 );
        }

        RegQueryValueEx( hkey, TEXT("SphereDensity"), NULL, &dwType, (BYTE*)&m_fSphereDensity, &dwLength );
        RegQueryValueEx( hkey, TEXT("Radius"), NULL, &dwType, (BYTE*)&m_fRadius, &dwLength );
        RegQueryValueEx( hkey, TEXT("TurbulenceNumOctaves"), NULL, &dwType, (BYTE*)&m_nTurbulenceNumOctaves, &dwLength );
        RegQueryValueEx( hkey, TEXT("TurbulenceSpeed"), NULL, &dwType, (BYTE*)&m_fTurbulenceSpeed, &dwLength );
        RegQueryValueEx( hkey, TEXT("TurbulenceForce"), NULL, &dwType, (BYTE*)&m_fTurbulenceForce, &dwLength );

        dwLength = sizeof(DWORD);
        if( ERROR_SUCCESS == RegQueryValueEx( hkey, TEXT("ShowBubbles"), NULL, &dwType,
                                              (BYTE*)&dwShowBubbles, &dwLength ) )
        {
            m_bShowBubbles = ( dwShowBubbles != 0 );
        }

        dwLength = sizeof(DWORD);
        if( ERROR_SUCCESS == RegQueryValueEx( hkey, TEXT("MaterialGlass"), NULL, &dwType,
                                              (BYTE*)&dwMaterialGlass, &dwLength ) )
        {
            m_bMaterialGlass = ( dwMaterialGlass != 0 );
        }

        dwLength = sizeof(DWORD);
        if( ERROR_SUCCESS == RegQueryValueEx( hkey, TEXT("ShowShadows"), NULL, &dwType,
                                              (BYTE*)&dwShowShadows, &dwLength ) )
        {
            m_bShowShadows = ( dwShowShadows != 0 );
        }

        ValidateSettings();

        ReadScreenSettings( hkey );

        RegCloseKey( hkey );
    }
}




//-----------------------------------------------------------------------------
// Name: ValidateSettings()
// Desc: Clamp everything that came out of the registry to a sane range.
//-----------------------------------------------------------------------------
VOID BubblesScreensaver::ValidateSettings()
{
    if( m_fSphereDensity < 0.001f )
        m_fSphereDensity = 0.001f;
    else if( m_fSphereDensity > 1.0f )
        m_fSphereDensity = 1.0f;

    if( m_fRadius < 20.0f )
        m_fRadius = 20.0f;
    else if( m_fRadius > 200.0f )
        m_fRadius = 200.0f;

    if( m_nTurbulenceNumOctaves < 1 )
        m_nTurbulenceNumOctaves = 1;
    else if( m_nTurbulenceNumOctaves > 4 )
        m_nTurbulenceNumOctaves = 4;

    if( m_fTurbulenceSpeed < 0.005f )
        m_fTurbulenceSpeed = 0.005f;
    else if( m_fTurbulenceSpeed > 0.1f )
        m_fTurbulenceSpeed = 0.1f;

    if( m_fTurbulenceForce < 0.0f )
        m_fTurbulenceForce = 0.0f;
    else if( m_fTurbulenceForce > 2000.0f )
        m_fTurbulenceForce = 2000.0f;
}
