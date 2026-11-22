//-----------------------------------------------------------------------------
// File: ssmypics.cpp
//
// Desc: My Pictures Slideshow screen saver (ssmypics.scr) of Windows XP and
//       Windows Server 2003: shows the pictures of the My Pictures folder
//       (or another one), with transitions, on all monitors.  Its window,
//       command line and password handling come from the screen saver
//       library (scrnsavw.lib); this file has the entry points it calls:
//       ScreenSaverProc, ScreenSaverConfigureDialog, RegisterDialogClasses.
//
//       The binary (x64) has no symbol file: the names of this
//       reconstruction are descriptive, not the original ones.  Its manifest
//       names it Microsoft.Windows.PrintingAndImaging.ssmypics, and the
//       help of its dialog is in camera.hlp, the help file of the imaging
//       components.
//
//       Memory comes from LocalAlloc, through operator new, which returns
//       NULL when there is none (the code checks for it).
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <scrnsave.h>
#include <new>
#include "mypics.h"
#include "settings.h"
#include "resource.h"

// Private messages
#define PWM_SETTINGSCHANGED     0x0914      // posted to the owner of the settings dialog
#define PWM_FOUNDFILE           0x0915      // from the search thread: lParam a CStr*, or NULL at its end

// Timers
#define TIMER_PAINT             1
#define TIMER_CHANGE            2
#define TIMER_START             3

HINSTANCE g_hInstance;




//-----------------------------------------------------------------------------
// Name: operator new, operator delete
// Desc: Zeroed memory from the local heap
//-----------------------------------------------------------------------------
void* operator new( size_t cb )
{
    if( cb == 0 )
        return NULL;
    return LocalAlloc( LPTR, cb );
}

void operator delete( void* pv )
{
    if( pv != NULL )
        LocalFree( pv );
}

void operator delete( void* pv, size_t )
{
    if( pv != NULL )
        LocalFree( pv );
}




//-----------------------------------------------------------------------------
// Name: ScreenSaverProc()
// Desc: The slide show starts with the first WM_ERASEBKGND.  Keys that
//       control it do not end the screen saver.
//-----------------------------------------------------------------------------
LRESULT WINAPI ScreenSaverProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    CMyPicturesSaver* pSaver = (CMyPicturesSaver*)GetWindowLongPtrW( hWnd, GWLP_USERDATA );

    switch( uMsg )
    {
    case WM_CREATE:
        if( FAILED( CoInitialize( NULL ) ) )
            return -1;
        g_hInstance = ((LPCREATESTRUCTW)lParam)->hInstance;
        SetErrorMode( SEM_FAILCRITICALERRORS );
        break;

    case WM_DESTROY:
        if( pSaver != NULL )
            delete pSaver;
        CoUninitialize();
        SetWindowLongPtrW( hWnd, GWLP_USERDATA, 0 );
        break;

    case WM_PAINT:
        if( pSaver != NULL )
            pSaver->OnPaint();
        break;

    case WM_ERASEBKGND:
        if( pSaver == NULL )
        {
            pSaver = new CMyPicturesSaver( hWnd, PWM_FOUNDFILE, TIMER_PAINT, TIMER_CHANGE,
                                           TIMER_START, REGSTR_PATH_SLIDESHOW, g_hInstance );
            if( pSaver != NULL )
                pSaver->Initialize();
        }
        SetWindowLongPtrW( hWnd, GWLP_USERDATA, (LONG_PTR)pSaver );
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
        if( pSaver != NULL && pSaver->OnKey( uMsg, wParam ) )
            return 0;
        break;

    case WM_TIMER:
        if( pSaver != NULL )
            pSaver->OnTimer( wParam );
        break;

    case PWM_FOUNDFILE:
        if( pSaver != NULL )
            pSaver->OnFileFound( (CStr*)lParam );
        break;
    }

    return DefScreenSaverProc( hWnd, uMsg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: RegisterDialogClasses()
// Desc: The trackbars
//-----------------------------------------------------------------------------
BOOL WINAPI RegisterDialogClasses( HANDLE hInstance )
{
    InitCommonControls();
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: FormatInterval()
// Desc: "1 minute and 30 seconds" and the like
//-----------------------------------------------------------------------------
static CStr FormatInterval( HINSTANCE hInstance, UINT nInterval )
{
    CStr str( L"" );
    UINT nMinutes = nInterval / 60;
    UINT nSeconds = nInterval % 60;

    if( nMinutes == 0 )
    {
        if( nSeconds == 1 )
            str.Format( IDS_SECOND, hInstance, 1 );
        else
            str.Format( IDS_SECONDS, hInstance, nSeconds );
    }
    else if( nSeconds == 0 )
    {
        if( nMinutes == 1 )
            str.Format( IDS_MINUTE, hInstance, 1 );
        else
            str.Format( IDS_MINUTES, hInstance, nMinutes );
    }
    else
    {
        UINT nFormat;
        if( nMinutes == 1 )
            nFormat = (nSeconds == 1) ? IDS_MINUTE_SECOND : IDS_MINUTE_SECONDS;
        else
            nFormat = (nSeconds == 1) ? IDS_MINUTES_SECOND : IDS_MINUTES_SECONDS;
        str.Format( nFormat, hInstance, nMinutes, nSeconds );
    }

    return str;
}




//-----------------------------------------------------------------------------
// Settings dialog
//-----------------------------------------------------------------------------
static const DWORD s_adwHelpIds[] =
{
    IDC_MYPICS_ICON,                   (DWORD)-1,
    IDC_TITLE,                  (DWORD)-1,
    IDC_SEPARATOR,              (DWORD)-1,
    IDC_CHANGE_INTERVAL_LABEL,  IDH_MYPICS_CHANGE_INTERVAL,
    IDC_CHANGE_INTERVAL,        IDH_MYPICS_CHANGE_INTERVAL,
    IDC_CHANGE_INTERVAL_TEXT,   IDH_MYPICS_CHANGE_INTERVAL,
    IDC_MORE_OFTEN,             IDH_MYPICS_CHANGE_INTERVAL,
    IDC_LESS_OFTEN,             IDH_MYPICS_CHANGE_INTERVAL,
    IDC_SCREEN_PERCENT_LABEL,   IDH_MYPICS_SCREEN_PERCENT,
    IDC_SMALLER,                IDH_MYPICS_SCREEN_PERCENT,
    IDC_LARGER,                 IDH_MYPICS_SCREEN_PERCENT,
    IDC_SCREEN_PERCENT,         IDH_MYPICS_SCREEN_PERCENT,
    IDC_SCREEN_PERCENT_TEXT,    IDH_MYPICS_SCREEN_PERCENT,
    IDC_DIRECTORY,              IDH_MYPICS_DIRECTORY,
    IDC_STRETCH_SMALL,          IDH_MYPICS_STRETCH_SMALL,
    IDC_SHOW_FILE_NAMES,        IDH_MYPICS_SHOW_FILE_NAMES,
    IDC_USE_TRANSITIONS,        IDH_MYPICS_USE_TRANSITIONS,
    IDC_ALLOW_KEYBOARD,         IDH_MYPICS_ALLOW_KEYBOARD,
    IDOK,                       IDH_OK,
    IDCANCEL,                   IDH_CANCEL,
    IDC_BROWSE,                 IDH_MYPICS_BROWSE,
    0, 0
};

// The trackbar ranges
#define MIN_CHANGE_INTERVAL     6       // seconds
#define MAX_CHANGE_INTERVAL     180
#define MIN_SCREEN_PERCENT      25
#define MAX_SCREEN_PERCENT      100




//-----------------------------------------------------------------------------
// Name: OnInitDialog()
// Desc:
//-----------------------------------------------------------------------------
static void OnInitDialog( HWND hDlg )
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtrW( hDlg, GWLP_HINSTANCE );
    SetDialogIcons( hDlg, hInstance, IDI_MYPICS );

    CSlideshowSettings settings( HKEY_CURRENT_USER, CStr( REGSTR_PATH_SLIDESHOW ) );

    SendDlgItemMessageW( hDlg, IDC_DIRECTORY, WM_SETTEXT, 0,
                         (LPARAM)settings.GetImageDirectory().String() );

    SendDlgItemMessageW( hDlg, IDC_CHANGE_INTERVAL, TBM_SETRANGE, TRUE,
                         MAKELONG( MIN_CHANGE_INTERVAL, MAX_CHANGE_INTERVAL ) );
    SendDlgItemMessageW( hDlg, IDC_CHANGE_INTERVAL, TBM_SETPOS, TRUE,
                         settings.GetChangeInterval() / 1000 );
    SendDlgItemMessageW( hDlg, IDC_CHANGE_INTERVAL_TEXT, WM_SETTEXT, 0,
                         (LPARAM)FormatInterval( (HINSTANCE)GetWindowLongPtrW( hDlg, GWLP_HINSTANCE ),
                                                 settings.GetChangeInterval() / 1000 ).String() );

    SendDlgItemMessageW( hDlg, IDC_SCREEN_PERCENT, TBM_SETRANGE, TRUE,
                         MAKELONG( MIN_SCREEN_PERCENT, MAX_SCREEN_PERCENT ) );
    SendDlgItemMessageW( hDlg, IDC_SCREEN_PERCENT, TBM_SETPOS, TRUE, settings.GetMaxScreenPercent() );

    CStr strPercent;
    strPercent.Format( IDS_SCREEN_PERCENT, (HINSTANCE)GetWindowLongPtrW( hDlg, GWLP_HINSTANCE ),
                       settings.GetMaxScreenPercent() );
    SendDlgItemMessageW( hDlg, IDC_SCREEN_PERCENT_TEXT, WM_SETTEXT, 0, (LPARAM)strPercent.String() );

    SendDlgItemMessageW( hDlg, IDC_SHOW_FILE_NAMES, BM_SETCHECK,
                         settings.GetDisplayFilename() ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessageW( hDlg, IDC_USE_TRANSITIONS, BM_SETCHECK,
                         !settings.GetDisableTransitions() ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessageW( hDlg, IDC_STRETCH_SMALL, BM_SETCHECK,
                         settings.GetAllowStretching() ? BST_CHECKED : BST_UNCHECKED, 0 );
    SendDlgItemMessageW( hDlg, IDC_ALLOW_KEYBOARD, BM_SETCHECK,
                         settings.GetAllowKeyboardControl() ? BST_CHECKED : BST_UNCHECKED, 0 );
}




//-----------------------------------------------------------------------------
// Name: OnOK()
// Desc: Saves the settings.  The My Pictures folder is saved as no folder,
//       so that the default applies.
//-----------------------------------------------------------------------------
static void OnOK( HWND hDlg )
{
    CSlideshowSettings settings( HKEY_CURRENT_USER, CStr( REGSTR_PATH_SLIDESHOW ) );
    WCHAR szDirectory[MAX_PATH];

    if( SendDlgItemMessageW( hDlg, IDC_DIRECTORY, WM_GETTEXT, ARRAYSIZE(szDirectory),
                             (LPARAM)szDirectory ) != 0 )
    {
        CStr strDirectory( szDirectory );

        CShellIdList pidlMyPictures;
        pidlMyPictures.GetSpecialFolder( hDlg, CSIDL_MYPICTURES );
        bool bMyPictures = pidlMyPictures.GetPath().IsEqual( strDirectory );
        pidlMyPictures.Free();

        if( bMyPictures )
            settings.SetImageDirectory( CStr( L"" ) );
        else
            settings.SetImageDirectory( strDirectory );
    }

    settings.SetChangeInterval( (UINT)SendDlgItemMessageW( hDlg, IDC_CHANGE_INTERVAL, TBM_GETPOS, 0, 0 ) * 1000 );
    settings.SetMaxScreenPercent( (int)SendDlgItemMessageW( hDlg, IDC_SCREEN_PERCENT, TBM_GETPOS, 0, 0 ) );
    settings.SetDisplayFilename( SendDlgItemMessageW( hDlg, IDC_SHOW_FILE_NAMES, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
    settings.SetDisableTransitions( SendDlgItemMessageW( hDlg, IDC_USE_TRANSITIONS, BM_GETCHECK, 0, 0 ) != BST_CHECKED );
    settings.SetAllowStretching( SendDlgItemMessageW( hDlg, IDC_STRETCH_SMALL, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
    settings.SetAllowKeyboardControl( SendDlgItemMessageW( hDlg, IDC_ALLOW_KEYBOARD, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
    settings.Write();

    HWND hWndOwner = (HWND)GetWindowLongPtrW( hDlg, GWLP_HWNDPARENT );
    if( hWndOwner != NULL )
        PostMessageW( hWndOwner, PWM_SETTINGSCHANGED, 0, 0 );

    EndDialog( hDlg, IDOK );
}




//-----------------------------------------------------------------------------
// Name: ScreenSaverConfigureDialog()
// Desc:
//-----------------------------------------------------------------------------
BOOL WINAPI ScreenSaverConfigureDialog( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
    case WM_HELP:
        HandleHelp( wParam, lParam, s_adwHelpIds );
        return TRUE;

    case WM_CONTEXTMENU:
        HandleContextMenu( wParam, lParam, s_adwHelpIds );
        return TRUE;

    case WM_INITDIALOG:
        OnInitDialog( hDlg );
        return TRUE;

    case WM_HSCROLL:
        if( (HWND)lParam == GetDlgItem( hDlg, IDC_CHANGE_INTERVAL ) )
        {
            UINT nInterval = (UINT)SendDlgItemMessageW( hDlg, IDC_CHANGE_INTERVAL, TBM_GETPOS, 0, 0 );
            SendDlgItemMessageW( hDlg, IDC_CHANGE_INTERVAL_TEXT, WM_SETTEXT, 0,
                                 (LPARAM)FormatInterval( (HINSTANCE)GetWindowLongPtrW( hDlg, GWLP_HINSTANCE ),
                                                         nInterval ).String() );
        }
        if( (HWND)lParam == GetDlgItem( hDlg, IDC_SCREEN_PERCENT ) )
        {
            int nPercent = (int)SendDlgItemMessageW( hDlg, IDC_SCREEN_PERCENT, TBM_GETPOS, 0, 0 );
            CStr strPercent( L"" );
            strPercent.Format( IDS_SCREEN_PERCENT, (HINSTANCE)GetWindowLongPtrW( hDlg, GWLP_HINSTANCE ),
                               nPercent );
            SendDlgItemMessageW( hDlg, IDC_SCREEN_PERCENT_TEXT, WM_SETTEXT, 0,
                                 (LPARAM)strPercent.String() );
            return TRUE;
        }
        break;

    case WM_COMMAND:
        switch( LOWORD( wParam ) )
        {
        case IDOK:
            OnOK( hDlg );
            return TRUE;

        case IDCANCEL:
            EndDialog( hDlg, IDCANCEL );
            return TRUE;

        case IDC_BROWSE:
            {
                WCHAR szDirectory[MAX_PATH];
                SendDlgItemMessageW( hDlg, IDC_DIRECTORY, WM_GETTEXT, ARRAYSIZE(szDirectory),
                                     (LPARAM)szDirectory );

                CStr strTitle( L"" );
                strTitle.LoadString( IDS_BROWSE_TITLE,
                                     (HINSTANCE)GetWindowLongPtrW( hDlg, GWLP_HINSTANCE ) );
                if( BrowseForFolder( hDlg, strTitle, szDirectory ) )
                    SendDlgItemMessageW( hDlg, IDC_DIRECTORY, WM_SETTEXT, 0, (LPARAM)szDirectory );
            }
            return TRUE;
        }
        break;
    }

    return FALSE;
}
