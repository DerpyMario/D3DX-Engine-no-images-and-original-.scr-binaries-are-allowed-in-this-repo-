//-----------------------------------------------------------------------------
// File: util.cpp
//
// Desc: Helper classes of the My Pictures Slideshow screen saver: the
//       string, the registry key, the device context, the tokenizer, the
//       item ID list, and the helpers of the settings dialog (icons, folder
//       browser, context help).
//-----------------------------------------------------------------------------
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdarg.h>
#include "util.h"
#include "resource.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)    (sizeof(a) / sizeof((a)[0]))
#endif




//-----------------------------------------------------------------------------
// Name: CStr::CStr()
// Desc:
//-----------------------------------------------------------------------------
CStr::CStr()
    : m_psz(m_szBuiltIn), m_nMaxLength(c_nBuiltInLength), m_nGrowBy(c_nDefaultGrowBy)
{
    m_szBuiltIn[0] = L'\0';
}

CStr::CStr( LPCWSTR psz )
    : m_psz(m_szBuiltIn), m_nMaxLength(c_nBuiltInLength), m_nGrowBy(c_nDefaultGrowBy)
{
    m_szBuiltIn[0] = L'\0';
    Assign( psz );
}

CStr::CStr( const CStr& str )
    : m_psz(m_szBuiltIn), m_nMaxLength(c_nBuiltInLength), m_nGrowBy(c_nDefaultGrowBy)
{
    m_szBuiltIn[0] = L'\0';
    Assign( str.m_psz );
}

CStr::~CStr()
{
    Destroy();
}

void CStr::Destroy()
{
    if( m_psz != NULL && m_psz != m_szBuiltIn )
        delete[] m_psz;
    m_psz = NULL;
    m_nMaxLength = 0;
}




//-----------------------------------------------------------------------------
// Name: CStr::Reserve()
// Desc: Make room for nLength characters (the terminator included), keeping
//       the string
//-----------------------------------------------------------------------------
bool CStr::Reserve( UINT nLength )
{
    if( nLength <= m_nMaxLength )
        return true;

    UINT nMaxLength = nLength + m_nGrowBy;
    LPWSTR psz = (nMaxLength != 0) ? new WCHAR[nMaxLength] : NULL;
    if( psz == NULL )
        return false;

    if( m_psz != NULL )
    {
        memcpy( psz, m_psz, (lstrlenW( m_psz ) + 1) * sizeof(WCHAR) );
        if( m_psz != m_szBuiltIn )
            delete[] m_psz;
        m_psz = NULL;
    }

    m_nMaxLength = nMaxLength;
    m_psz = psz;
    return true;
}




//-----------------------------------------------------------------------------
// Name: CStr::Assign()
// Desc: Copies a string.  Without a string (or without memory for it) the
//       result is empty.
//-----------------------------------------------------------------------------
bool CStr::Assign( LPCWSTR psz )
{
    if( psz != NULL && Reserve( lstrlenW( psz ) + 1 ) )
    {
        memcpy( m_psz, psz, (lstrlenW( psz ) + 1) * sizeof(WCHAR) );
        return m_psz != NULL;
    }

    if( m_nMaxLength == 0 )
    {
        UINT nMaxLength = m_nGrowBy + 1;
        LPWSTR pszNew = (nMaxLength != 0) ? new WCHAR[nMaxLength] : NULL;
        if( pszNew == NULL )
        {
            Destroy();
            return false;
        }
        m_nMaxLength = nMaxLength;
        m_psz = pszNew;
    }

    *m_psz = L'\0';
    return m_psz != NULL;
}




//-----------------------------------------------------------------------------
// Name: CStr::Append()
// Desc:
//-----------------------------------------------------------------------------
bool CStr::Append( const CStr& str )
{
    if( str.m_psz == NULL )
        return true;

    if( !Reserve( Length() + str.Length() + 1 ) )
        return false;

    memcpy( m_psz + Length(), str.m_psz, (str.Length() + 1) * sizeof(WCHAR) );
    return true;
}




//-----------------------------------------------------------------------------
// Name: CStr::LoadString()
// Desc: A string resource (of the program if hInstance is NULL)
//-----------------------------------------------------------------------------
bool CStr::LoadString( UINT nId, HINSTANCE hInstance )
{
    if( hInstance == NULL )
        hInstance = GetModuleHandleW( NULL );

    WCHAR szString[1024];
    ZeroMemory( szString, sizeof(szString) );

    int nLength = ::LoadStringW( hInstance, nId, szString, ARRAYSIZE(szString) );
    Assign( szString );
    return nLength > 0;
}




//-----------------------------------------------------------------------------
// Name: CStr::Format()
// Desc: printf with a string resource as the format
//-----------------------------------------------------------------------------
CStr& CStr::Format( UINT nFormatId, HINSTANCE hInstance, ... )
{
    CStr strFormat;

    if( !strFormat.LoadString( nFormatId, hInstance ) )
    {
        Assign( NULL );
    }
    else
    {
        WCHAR szString[1024];
        ZeroMemory( szString, sizeof(szString) );

        va_list args;
        va_start( args, hInstance );
        _vsnwprintf( szString, ARRAYSIZE(szString) - 1, strFormat, args );
        va_end( args );

        Assign( szString );
    }

    return *this;
}




//-----------------------------------------------------------------------------
// Name: CStr::Length(), Find(), ReverseFind(), IsEqual()
// Desc:
//-----------------------------------------------------------------------------
int CStr::Length() const
{
    if( m_psz == NULL )
        return 0;
    return lstrlenW( m_psz );
}

int CStr::Find( WCHAR ch ) const
{
    for( int i = 0; i < Length(); i++ )
    {
        if( m_psz[i] == ch )
            return i;
    }
    return -1;
}

int CStr::ReverseFind( WCHAR ch ) const
{
    for( int i = Length() - 1; i >= 0; i-- )
    {
        if( m_psz[i] == ch )
            return i;
    }
    return -1;
}

// (letter case counts)
bool CStr::IsEqual( const CStr& str ) const
{
    if( m_psz == NULL && str.m_psz == NULL )
        return true;
    if( m_psz == NULL || str.m_psz == NULL )
        return false;

    return CompareStringW( LOCALE_USER_DEFAULT, 0, m_psz, -1, str.m_psz, -1 ) == CSTR_EQUAL;
}




//-----------------------------------------------------------------------------
// Name: CStr::TrimRight()
// Desc: Removes the blanks (space, tab, line feed, carriage return) at the end
//-----------------------------------------------------------------------------
void CStr::TrimRight()
{
    LPWSTR pszBlanks = NULL;

    if( m_psz == NULL )
        return;

    for( LPWSTR p = m_psz; *p != L'\0'; p++ )
    {
        if( *p == L' ' || *p == L'\t' || *p == L'\n' || *p == L'\r' )
        {
            if( pszBlanks == NULL )
                pszBlanks = p;
        }
        else
        {
            pszBlanks = NULL;
        }
    }

    if( pszBlanks != NULL )
        *pszBlanks = L'\0';
}




//-----------------------------------------------------------------------------
// Name: AddBackslash()
// Desc: The path with a backslash at its end
//-----------------------------------------------------------------------------
CStr AddBackslash( const CStr& strPath )
{
    if( strPath.Length() != 0 )
    {
        int i = strPath.ReverseFind( L'\\' );
        if( i >= 0 && i == strPath.Length() - 1 )
            return strPath;
    }

    CStr str( strPath );
    str += CStr( L"\\" );
    return str;
}




//-----------------------------------------------------------------------------
// Name: CRegistryKey::CRegistryKey()
// Desc: Opens (or creates) the key
//-----------------------------------------------------------------------------
CRegistryKey::CRegistryKey( HKEY hRoot, const CStr& strKey, bool bCreate, REGSAM samDesired,
                            LPSECURITY_ATTRIBUTES psa )
    : m_strKey(strKey), m_hRoot(hRoot), m_hKey(NULL), m_bCreate(bCreate), m_psa(psa),
      m_samDesired(samDesired)
{
    HKEY hKey = NULL;
    DWORD dwDisposition = 0;
    LONG lResult;

    Close();

    if( !m_bCreate )
    {
        lResult = RegOpenKeyExW( m_hRoot, m_strKey, 0,
                                 m_samDesired ? m_samDesired : KEY_ALL_ACCESS, &hKey );
    }
    else
    {
        lResult = RegCreateKeyExW( m_hRoot, m_strKey, 0, (LPWSTR)L"", REG_OPTION_NON_VOLATILE,
                                   m_samDesired ? m_samDesired : KEY_ALL_ACCESS, m_psa,
                                   &hKey, &dwDisposition );
    }

    if( lResult == ERROR_SUCCESS )
        m_hKey = hKey;
}

CRegistryKey::~CRegistryKey()
{
    Close();
    m_hRoot = NULL;
    m_psa = NULL;
}

bool CRegistryKey::Close()
{
    if( IsOpen() )
        RegCloseKey( m_hKey );
    m_hKey = NULL;
    return true;
}




//-----------------------------------------------------------------------------
// Name: CRegistryKey::ValueType(), ValueSize()
// Desc: REG_NONE / 0 if there is no such value
//-----------------------------------------------------------------------------
DWORD CRegistryKey::ValueType( const CStr& strName )
{
    DWORD dwType = REG_NONE, cbData = 0;

    if( !IsOpen() ||
        RegQueryValueExW( m_hKey, strName, NULL, &dwType, NULL, &cbData ) != ERROR_SUCCESS )
    {
        return REG_NONE;
    }
    return dwType;
}

DWORD CRegistryKey::ValueSize( const CStr& strName )
{
    DWORD dwType = REG_NONE, cbData = 0;

    if( !IsOpen() ||
        RegQueryValueExW( m_hKey, strName, NULL, &dwType, NULL, &cbData ) != ERROR_SUCCESS )
    {
        return 0;
    }
    return cbData;
}

static bool IsStringType( DWORD dwType )
{
    return dwType == REG_SZ || dwType == REG_EXPAND_SZ || dwType == REG_MULTI_SZ ||
           dwType == REG_LINK || dwType == REG_RESOURCE_LIST;
}




//-----------------------------------------------------------------------------
// Name: CRegistryKey::QueryDword()
// Desc:
//-----------------------------------------------------------------------------
DWORD CRegistryKey::QueryDword( const CStr& strName, DWORD dwDefault )
{
    if( ValueType( strName ) == REG_DWORD && ValueSize( strName ) == sizeof(DWORD) )
    {
        DWORD dwType, dwValue, cbData = sizeof(DWORD);

        if( RegQueryValueExW( m_hKey, strName, NULL, &dwType, (LPBYTE)&dwValue,
                              &cbData ) == ERROR_SUCCESS )
        {
            dwDefault = dwValue;
        }
    }
    return dwDefault;
}




//-----------------------------------------------------------------------------
// Name: CRegistryKey::QueryString()
// Desc: Into a buffer of cchValue characters; returns the buffer (NULL if it
//       has no room at all)
//-----------------------------------------------------------------------------
LPWSTR CRegistryKey::QueryString( const CStr& strName, const CStr& strDefault, LPWSTR pszValue,
                                  DWORD cchValue )
{
    DWORD dwType, cbData;

    if( cchValue == 0 )
        return NULL;

    *pszValue = L'\0';

    if( !IsOpen() ||
        RegQueryValueExW( m_hKey, strName, NULL, &dwType, NULL, &cbData ) != ERROR_SUCCESS ||
        !IsStringType( dwType ) )
    {
        if( strDefault.String() != NULL )
            lstrcpynW( pszValue, strDefault, cchValue );
    }
    else
    {
        cbData = cchValue * sizeof(WCHAR);
        if( RegQueryValueExW( m_hKey, strName, NULL, &dwType, (LPBYTE)pszValue,
                              &cbData ) != ERROR_SUCCESS )
        {
            lstrcpynW( pszValue, strDefault, cchValue );
            pszValue[cchValue - 1] = L'\0';
        }
    }

    return pszValue;
}

CStr CRegistryKey::QueryString( const CStr& strName, const CStr& strDefault )
{
    if( IsOpen() && IsStringType( ValueType( strName ) ) )
    {
        DWORD cchValue = ValueSize( strName ) / sizeof(WCHAR);
        LPWSTR pszValue = (cchValue != 0) ? new WCHAR[cchValue + 1] : NULL;
        CStr str( L"" );

        if( pszValue != NULL )
        {
            QueryString( strName, strDefault, pszValue, cchValue );
            pszValue[cchValue] = L'\0';
            str = pszValue;
            delete[] pszValue;
        }
        return str;
    }

    return strDefault;
}




//-----------------------------------------------------------------------------
// Name: CRegistryKey::SetDword(), SetString(), DeleteValue()
// Desc:
//-----------------------------------------------------------------------------
bool CRegistryKey::SetDword( const CStr& strName, DWORD dwValue )
{
    if( !IsOpen() )
        return false;

    return RegSetValueExW( m_hKey, strName, 0, REG_DWORD, (const BYTE*)&dwValue,
                           sizeof(DWORD) ) == ERROR_SUCCESS;
}

bool CRegistryKey::SetString( const CStr& strName, const CStr& strValue, DWORD dwType )
{
    if( !IsOpen() )
        return false;

    return RegSetValueExW( m_hKey, strName, 0, dwType, (const BYTE*)strValue.String(),
                           (strValue.Length() + 1) * sizeof(WCHAR) ) == ERROR_SUCCESS;
}

bool CRegistryKey::DeleteValue( const CStr& strName )
{
    if( !IsOpen() )
        return false;

    return RegDeleteValueW( m_hKey, strName ) == ERROR_SUCCESS;
}




//-----------------------------------------------------------------------------
// Name: CScopedDC
// Desc:
//-----------------------------------------------------------------------------
CScopedDC::CScopedDC()
    : m_hDC(NULL), m_hWnd(NULL), m_nType(NoDC), m_hOldBitmap(NULL), m_hOldBrush(NULL),
      m_hOldFont(NULL), m_hOldPen(NULL), m_hOldPalette(NULL)
{
    ZeroMemory( &m_ps, sizeof(m_ps) );
}

CScopedDC::~CScopedDC()
{
    Release();
}

bool CScopedDC::GetDC( HWND hWnd )
{
    Release();
    m_hDC = ::GetDC( hWnd );
    if( m_hDC != NULL )
    {
        m_nType = ClientDC;
        m_hWnd = hWnd;
        SelectDefaults();
    }
    return m_hDC != NULL;
}

bool CScopedDC::BeginPaint( HWND hWnd )
{
    Release();
    m_hDC = ::BeginPaint( hWnd, &m_ps );
    if( m_hDC != NULL )
    {
        m_nType = PaintDC;
        m_hWnd = hWnd;
        SelectDefaults();
    }
    return m_hDC != NULL;
}

bool CScopedDC::CreateCompatibleDC( HDC hdc )
{
    Release();
    m_hDC = ::CreateCompatibleDC( hdc );
    if( m_hDC != NULL )
    {
        m_nType = MemoryDC;
        SelectDefaults();
    }
    return m_hDC != NULL;
}




//-----------------------------------------------------------------------------
// Name: CScopedDC::SelectDefaults()
// Desc: Selects the stock objects, remembering what was selected.  There is
//       no way to ask a DC for its bitmap: selecting one returns it, and it
//       is put back at once (IDB_PLACEHOLDER is a 1x1 bitmap for that).
//-----------------------------------------------------------------------------
void CScopedDC::SelectDefaults()
{
    if( m_hDC == NULL )
        return;

    HBITMAP hbm = LoadBitmapW( g_hInstance, MAKEINTRESOURCEW( IDB_PLACEHOLDER ) );
    if( hbm != NULL )
    {
        m_hOldBitmap = SelectObject( m_hDC, hbm );
        if( m_hOldBitmap != NULL )
            SelectObject( m_hDC, m_hOldBitmap );
        DeleteObject( hbm );
    }

    m_hOldBrush = SelectObject( m_hDC, GetStockObject( NULL_BRUSH ) );
    m_hOldFont = SelectObject( m_hDC, GetStockObject( SYSTEM_FONT ) );
    m_hOldPen = SelectObject( m_hDC, GetStockObject( NULL_PEN ) );
    m_hOldPalette = SelectPalette( m_hDC, (HPALETTE)GetStockObject( DEFAULT_PALETTE ), TRUE );
}




//-----------------------------------------------------------------------------
// Name: CScopedDC::Release()
// Desc: Puts the objects back and releases the DC.  (The remembered objects
//       are not cleared, as in the original.)
//-----------------------------------------------------------------------------
void CScopedDC::Release()
{
    if( m_hDC != NULL )
    {
        if( m_hOldBitmap != NULL )
            SelectObject( m_hDC, m_hOldBitmap );
        if( m_hOldBrush != NULL )
            SelectObject( m_hDC, m_hOldBrush );
        if( m_hOldFont != NULL )
            SelectObject( m_hDC, m_hOldFont );
        if( m_hOldPen != NULL )
            SelectObject( m_hDC, m_hOldPen );
        if( m_hOldPalette != NULL )
            SelectPalette( m_hDC, m_hOldPalette, TRUE );

        switch( m_nType )
        {
        case WindowDC:
        case ClientDC:
            ReleaseDC( m_hWnd, m_hDC );
            break;
        case PaintDC:
            EndPaint( m_hWnd, &m_ps );
            break;
        case MemoryDC:
        case CreatedDC:
            DeleteDC( m_hDC );
            break;
        }
    }

    ZeroMemory( &m_ps, sizeof(m_ps) );
    m_hDC = NULL;
    m_hWnd = NULL;
    m_nType = NoDC;
}




//-----------------------------------------------------------------------------
// Name: CTokenizer::NextToken()
// Desc: The next run of characters that are not delimiters (empty at the end)
//-----------------------------------------------------------------------------
CStr CTokenizer::NextToken( const CStr& strDelimiters )
{
    CStr strToken( L"" );

    while( m_nPos < m_str.Length() && strDelimiters.Find( m_str[m_nPos] ) >= 0 )
        m_nPos++;

    while( m_nPos < m_str.Length() && strDelimiters.Find( m_str[m_nPos] ) < 0 )
    {
        WCHAR szChar[2] = { m_str[m_nPos], L'\0' };
        strToken += CStr( szChar );
        m_nPos++;
    }

    return strToken;
}




//-----------------------------------------------------------------------------
// Name: CShellIdList
// Desc:
//-----------------------------------------------------------------------------
void CShellIdList::Free()
{
    if( m_pidl != NULL )
    {
        IMalloc* pMalloc = NULL;
        if( SUCCEEDED( SHGetMalloc( &pMalloc ) ) )
        {
            pMalloc->Free( m_pidl );
            pMalloc->Release();
        }
        m_pidl = NULL;
    }
}

bool CShellIdList::GetSpecialFolder( HWND hWnd, int nFolder )
{
    Free();

    if( SHGetSpecialFolderLocation( hWnd, nFolder, &m_pidl ) != S_OK && m_pidl != NULL )
        Free();

    return m_pidl != NULL;
}

CStr CShellIdList::GetPath() const
{
    WCHAR szPath[MAX_PATH] = L"";
    CStr strPath( L"" );

    if( m_pidl != NULL )
    {
        SHGetPathFromIDListW( m_pidl, szPath );
        strPath = szPath;
    }
    return strPath;
}




//-----------------------------------------------------------------------------
// Name: SelectAndRealizePalette()
// Desc:
//-----------------------------------------------------------------------------
HPALETTE SelectAndRealizePalette( HDC hdc, HPALETTE hPalette, BOOL bForceBackground )
{
    if( hdc == NULL || hPalette == NULL )
        return NULL;

    HPALETTE hOldPalette = SelectPalette( hdc, hPalette, bForceBackground );
    RealizePalette( hdc );
    SetBrushOrgEx( hdc, 0, 0, NULL );
    return hOldPalette;
}




//-----------------------------------------------------------------------------
// Name: NormalizeRect()
// Desc: left <= right, top <= bottom
//-----------------------------------------------------------------------------
void NormalizeRect( RECT* prc )
{
    if( prc->right < prc->left )
    {
        LONG n = prc->left;
        prc->left = prc->right;
        prc->right = n;
    }
    if( prc->bottom < prc->top )
    {
        LONG n = prc->top;
        prc->top = prc->bottom;
        prc->bottom = n;
    }
}




//-----------------------------------------------------------------------------
// Name: FillUncovered()
// Desc: Fills the parts of the old rectangle that the new one does not
//       cover: the bands above, left of, right of and below it
//-----------------------------------------------------------------------------
void FillUncovered( HDC hdc, const RECT* prcOld, const RECT* prcNew, HBRUSH hbr )
{
    RECT rc;

    rc.left = prcOld->left;
    rc.top = prcOld->top;
    rc.right = prcOld->right;
    rc.bottom = prcNew->top;
    if( rc.left < rc.right && rc.top < rc.bottom )
        FillRect( hdc, &rc, hbr );

    rc.left = prcOld->left;
    rc.top = prcNew->top;
    rc.right = prcNew->left;
    rc.bottom = prcNew->bottom;
    if( rc.left < rc.right && rc.top < rc.bottom )
        FillRect( hdc, &rc, hbr );

    rc.left = prcNew->right;
    rc.top = prcNew->top;
    rc.right = prcOld->right;
    rc.bottom = prcNew->bottom;
    if( rc.left < rc.right && rc.top < rc.bottom )
        FillRect( hdc, &rc, hbr );

    rc.left = prcOld->left;
    rc.top = prcNew->bottom;
    rc.right = prcOld->right;
    rc.bottom = prcOld->bottom;
    if( rc.left < rc.right && rc.top < rc.bottom )
        FillRect( hdc, &rc, hbr );
}




//-----------------------------------------------------------------------------
// Name: RoundUp()
// Desc: The next multiple of nMultiple
//-----------------------------------------------------------------------------
int RoundUp( int nValue, int nMultiple )
{
    int n = nValue / nMultiple;
    if( nValue % nMultiple != 0 )
        n++;
    return n * nMultiple;
}




//-----------------------------------------------------------------------------
// Name: SetDialogIcons()
// Desc: The small and the large icon of a window
//-----------------------------------------------------------------------------
bool SetDialogIcons( HWND hWnd, HINSTANCE hInstance, UINT nIconId )
{
    HANDLE hSmallIcon = LoadImageW( hInstance, MAKEINTRESOURCEW( nIconId ), IMAGE_ICON,
                                    GetSystemMetrics( SM_CXSMICON ),
                                    GetSystemMetrics( SM_CYSMICON ), 0 );
    HANDLE hLargeIcon = LoadImageW( hInstance, MAKEINTRESOURCEW( nIconId ), IMAGE_ICON,
                                    GetSystemMetrics( SM_CXICON ),
                                    GetSystemMetrics( SM_CYICON ), 0 );

    if( hSmallIcon != NULL )
        SendMessageW( hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon );
    if( hLargeIcon != NULL )
        SendMessageW( hWnd, WM_SETICON, ICON_BIG, (LPARAM)hLargeIcon );

    return hSmallIcon != NULL && hLargeIcon != NULL;
}




//-----------------------------------------------------------------------------
// Name: BrowseForFolder()
// Desc: Lets the user choose a folder, starting at pszPath (MAX_PATH
//       characters), which gets the choice
//-----------------------------------------------------------------------------
static int CALLBACK BrowseCallbackProc( HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
{
    if( uMsg == BFFM_INITIALIZED )
        SendMessageW( hWnd, BFFM_SETSELECTIONW, TRUE, lpData );
    return 0;
}

bool BrowseForFolder( HWND hWndOwner, LPCWSTR pszTitle, LPWSTR pszPath )
{
    bool bResult = false;
    IMalloc* pMalloc;

    if( SUCCEEDED( SHGetMalloc( &pMalloc ) ) )
    {
        WCHAR szDisplayName[MAX_PATH];
        WCHAR szPath[MAX_PATH];
        BROWSEINFOW bi;

        ZeroMemory( &bi, sizeof(bi) );
        bi.hwndOwner = hWndOwner;
        bi.pidlRoot = NULL;
        bi.pszDisplayName = szDisplayName;
        bi.lpszTitle = pszTitle;
        bi.ulFlags = BIF_RETURNONLYFSDIRS;
        bi.lpfn = BrowseCallbackProc;
        bi.lParam = (LPARAM)pszPath;

        LPITEMIDLIST pidl = SHBrowseForFolderW( &bi );
        if( pidl != NULL )
        {
            if( SHGetPathFromIDListW( pidl, szPath ) )
            {
                lstrcpyW( pszPath, szPath );
                bResult = true;
            }
            pMalloc->Free( pidl );
        }
        pMalloc->Release();
    }

    return bResult;
}




//-----------------------------------------------------------------------------
// Name: HelpFileOf()
// Desc: The help of the shared controls (OK, Cancel: IDs from 20000 on) is
//       in windows.hlp, the rest in camera.hlp
//-----------------------------------------------------------------------------
static LPCWSTR HelpFileOf( HWND hWnd, const DWORD* pdwHelpIds )
{
    if( pdwHelpIds != NULL && hWnd != NULL && IsWindow( hWnd ) )
    {
        DWORD dwId = (DWORD)GetWindowLongW( hWnd, GWL_ID );
        if( dwId != 0 )
        {
            for( const DWORD* p = pdwHelpIds; p[0] != 0; p += 2 )
            {
                if( p[0] == dwId )
                    return (p[1] >= 20000) ? L"windows.hlp" : L"camera.hlp";
            }
        }
    }
    return L"camera.hlp";
}

LRESULT HandleHelp( WPARAM wParam, LPARAM lParam, const DWORD* pdwHelpIds )
{
    LPHELPINFO phi = (LPHELPINFO)lParam;

    if( pdwHelpIds != NULL && phi != NULL && phi->iContextType == HELPINFO_WINDOW )
    {
        HWND hWnd = (HWND)phi->hItemHandle;
        WinHelpW( hWnd, HelpFileOf( hWnd, pdwHelpIds ), HELP_WM_HELP, (ULONG_PTR)pdwHelpIds );
    }
    return 0;
}

LRESULT HandleContextMenu( WPARAM wParam, LPARAM lParam, const DWORD* pdwHelpIds )
{
    HWND hWnd = (HWND)wParam;

    if( pdwHelpIds != NULL && hWnd != NULL )
        WinHelpW( hWnd, HelpFileOf( hWnd, pdwHelpIds ), HELP_CONTEXTMENU, (ULONG_PTR)pdwHelpIds );
    return 0;
}
