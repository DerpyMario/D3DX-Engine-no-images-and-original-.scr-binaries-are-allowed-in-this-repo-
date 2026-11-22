//-----------------------------------------------------------------------------
// File: settings.cpp
//
// Desc: Reading and writing the settings of the My Pictures Slideshow
//       screen saver
//-----------------------------------------------------------------------------
#include <windows.h>
#include <shlobj.h>
#include "settings.h"




//-----------------------------------------------------------------------------
// Name: CSlideshowSettings::CSlideshowSettings()
// Desc: The defaults, and what the registry has
//-----------------------------------------------------------------------------
CSlideshowSettings::CSlideshowSettings( HKEY hRoot, const CStr& strKey )
    : m_hRoot(hRoot), m_strKey(strKey), m_strImageDirectory(L"")
{
    m_pszImageDirectory         = L"ImageDirectory";
    m_pszPaintInterval          = L"PaintInterval";
    m_nPaintInterval            = 0;
    m_pszChangeInterval         = L"ChangeInterval";
    m_nChangeInterval           = 0;
    m_pszDisplayFilename        = L"DisplayFilename";
    m_bDisplayFilename          = false;
    m_pszMaxScreenPercent       = L"MaxScreenPercent";
    m_nMaxScreenPercent         = 0;
    m_pszDisableTransitions     = L"DisableTransitions";
    m_bDisableTransitions       = false;
    m_pszAllowStretching        = L"AllowStretching";
    m_bAllowStretching          = false;
    m_pszAllowKeyboardControl   = L"AllowKeyboardControl";
    m_bAllowKeyboardControl     = true;
    m_pszMaxFailedFiles         = L"MaxFailedFiles";
    m_nMaxFailedFiles           = 0;
    m_pszMaxSuccessfulFiles     = L"MaxSuccessfulFiles";
    m_nMaxSuccessfulFiles       = 0;
    m_pszMaxDirectories         = L"MaxDirectories";
    m_nMaxDirectories           = 0;

    Read();
}




//-----------------------------------------------------------------------------
// Name: FolderPath()
// Desc: The path of a special folder, if it has one
//-----------------------------------------------------------------------------
static void FolderPath( IMalloc* pMalloc, int nFolder, CStr& strPath )
{
    LPITEMIDLIST pidl;
    WCHAR szPath[MAX_PATH];

    if( SUCCEEDED( SHGetSpecialFolderLocation( NULL, nFolder, &pidl ) ) )
    {
        if( SHGetPathFromIDListW( pidl, szPath ) && lstrlenW( szPath ) != 0 )
            strPath = szPath;
        pMalloc->Free( pidl );
    }
}




//-----------------------------------------------------------------------------
// Name: CSlideshowSettings::DefaultImageDirectory()
// Desc: My Pictures, else My Documents, else the Windows directory
//-----------------------------------------------------------------------------
CStr CSlideshowSettings::DefaultImageDirectory()
{
    CStr strDirectory( L"" );
    IMalloc* pMalloc;

    if( SUCCEEDED( SHGetMalloc( &pMalloc ) ) )
    {
        FolderPath( pMalloc, CSIDL_MYPICTURES, strDirectory );
        if( strDirectory.Length() == 0 )
            FolderPath( pMalloc, CSIDL_PERSONAL, strDirectory );
        if( strDirectory.Length() == 0 )
            FolderPath( pMalloc, CSIDL_WINDOWS, strDirectory );
        pMalloc->Release();
    }

    return strDirectory;
}




//-----------------------------------------------------------------------------
// Name: CSlideshowSettings::Read()
// Desc:
//-----------------------------------------------------------------------------
void CSlideshowSettings::Read()
{
    CRegistryKey key( m_hRoot, m_strKey, false, KEY_READ );

    m_strImageDirectory     = key.QueryString( CStr( m_pszImageDirectory ), DefaultImageDirectory() );
    m_nPaintInterval        = key.QueryDword( CStr( m_pszPaintInterval ), 0 );
    m_nChangeInterval       = key.QueryDword( CStr( m_pszChangeInterval ), 6000 );
    m_bDisplayFilename      = key.QueryDword( CStr( m_pszDisplayFilename ), FALSE ) != 0;
    m_nMaxScreenPercent     = key.QueryDword( CStr( m_pszMaxScreenPercent ), 90 );
    m_bDisableTransitions   = key.QueryDword( CStr( m_pszDisableTransitions ), FALSE ) != 0;
    m_bAllowStretching      = key.QueryDword( CStr( m_pszAllowStretching ), FALSE ) != 0;
    m_bAllowKeyboardControl = key.QueryDword( CStr( m_pszAllowKeyboardControl ), TRUE ) != 0;
    m_nMaxFailedFiles       = key.QueryDword( CStr( m_pszMaxFailedFiles ), 1000 );
    m_nMaxSuccessfulFiles   = key.QueryDword( CStr( m_pszMaxSuccessfulFiles ), 65536 );
    m_nMaxDirectories       = key.QueryDword( CStr( m_pszMaxDirectories ), 200 );
}




//-----------------------------------------------------------------------------
// Name: CSlideshowSettings::Write()
// Desc: An empty directory deletes the value, so that the default applies
//-----------------------------------------------------------------------------
void CSlideshowSettings::Write()
{
    CRegistryKey key( m_hRoot, m_strKey, true, KEY_WRITE );

    if( m_strImageDirectory.Length() != 0 )
        key.SetString( CStr( m_pszImageDirectory ), m_strImageDirectory, REG_SZ );
    else
        key.DeleteValue( CStr( m_pszImageDirectory ) );

    key.SetDword( CStr( m_pszPaintInterval ), m_nPaintInterval );
    key.SetDword( CStr( m_pszChangeInterval ), m_nChangeInterval );
    key.SetDword( CStr( m_pszDisplayFilename ), m_bDisplayFilename );
    key.SetDword( CStr( m_pszMaxScreenPercent ), m_nMaxScreenPercent );
    key.SetDword( CStr( m_pszDisableTransitions ), m_bDisableTransitions );
    key.SetDword( CStr( m_pszAllowStretching ), m_bAllowStretching );
    key.SetDword( CStr( m_pszAllowKeyboardControl ), m_bAllowKeyboardControl );
    key.SetDword( CStr( m_pszMaxFailedFiles ), m_nMaxFailedFiles );
    key.SetDword( CStr( m_pszMaxSuccessfulFiles ), m_nMaxSuccessfulFiles );
    key.SetDword( CStr( m_pszMaxDirectories ), m_nMaxDirectories );
}
