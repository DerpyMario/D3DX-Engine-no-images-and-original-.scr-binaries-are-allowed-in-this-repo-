//-----------------------------------------------------------------------------
// File: texture.cpp
//
// Desc: The texture file helpers of the screen saver common library of the
//       OpenGL screen savers (the default bitmap, the file dialog), as far as
//       this port uses them.
//
//-----------------------------------------------------------------------------
#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include <wchar.h>
#include "ss3dfo.h"
#include "resource.h"

#define GEN_STRING_SIZE     64

// Texture file types
#define TEX_UNKNOWN         0
#define TEX_BMP             2


static TCHAR gszWarningTitle[MAX_PATH];
static TCHAR gszTextureSizeMsg[MAX_PATH];  // (not used by this port)
static TCHAR gszTextureInvalidMsg[MAX_PATH];// (not used by this port)
static TCHAR gszBitmapInvalidMsg[MAX_PATH];
static TCHAR gszTextureDialogTitle[GEN_STRING_SIZE];
static TCHAR gszTextureFilter[2*GEN_STRING_SIZE];
static TCHAR gszBmp[GEN_STRING_SIZE];
static TCHAR gszDotBmp[GEN_STRING_SIZE];
static BOOL  gbTextureObjects;              // (no OpenGL texture objects)
static BOOL  gbInFileDialog;




//-----------------------------------------------------------------------------
// Name: ss_fOnWin95()
// Desc:
//-----------------------------------------------------------------------------
static BOOL ss_fOnWin95()
{
    OSVERSIONINFO osvi;

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx( &osvi );

    return osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;
}




//-----------------------------------------------------------------------------
// Name: ss_LoadTextureResourceStrings()
// Desc:
//-----------------------------------------------------------------------------
BOOL ss_LoadTextureResourceStrings()
{
    LPTSTR pszStr;

    LoadString( NULL, IDS_TEXTUREDIALOGTITLE, gszTextureDialogTitle, GEN_STRING_SIZE );
    LoadString( NULL, IDS_BMP, gszBmp, GEN_STRING_SIZE );
    LoadString( NULL, IDS_DOTBMP, gszDotBmp, GEN_STRING_SIZE );

    // The filter has the pattern after the name: two strings in one
    LoadString( NULL, IDS_TEXTUREFILTER, gszTextureFilter, GEN_STRING_SIZE );
    pszStr = &gszTextureFilter[lstrlen( gszTextureFilter ) + 1];
    LoadString( NULL, IDS_STARDOTBMP, pszStr, GEN_STRING_SIZE );
    pszStr[lstrlen( pszStr ) + 1] = TEXT('\0');

    LoadString( NULL, IDS_WARNING, gszWarningTitle, MAX_PATH );
    LoadString( NULL, IDS_BITMAP_INVALID, gszBitmapInvalidMsg, MAX_PATH );
    LoadString( NULL, IDS_TEXTURE_INVALID, gszTextureInvalidMsg, MAX_PATH );
    LoadString( NULL, IDS_TEXTURE_SIZE, gszTextureSizeMsg, MAX_PATH );

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: ss_DeleteTexture()
// Desc:
//-----------------------------------------------------------------------------
void ss_DeleteTexture( TEXTURE* pTex )
{
    if( pTex == NULL )
        return;

    if( gbTextureObjects && pTex->texObj )
        pTex->texObj = 0;

    if( pTex->pal != NULL )
        free( pTex->pal );

    if( pTex->data != NULL )
        free( pTex->data );
}




//-----------------------------------------------------------------------------
// Name: ss_lstrcmpi()
// Desc: Compares two strings, ignoring case
//-----------------------------------------------------------------------------
static int ss_lstrcmpi( LPCTSTR psz1, LPCTSTR psz2 )
{
    int iResult = CompareString( LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE, psz1, -1, psz2, -1 );

    if( iResult == CSTR_EQUAL )
        return 0;

    return (iResult == CSTR_LESS_THAN) ? -1 : 1;
}




//-----------------------------------------------------------------------------
// Name: ss_GetDefaultBmpFile()
// Desc: The bitmap of the product type: winnt.bmp (workstation) or
//       lanmannt.bmp (server)
//-----------------------------------------------------------------------------
void ss_GetDefaultBmpFile( LPTSTR pszBmpFile )
{
    HKEY hkey;
    DWORD dwSize = MAX_PATH;

    if( ss_fOnWin95() )
    {
        lstrcpy( pszBmpFile, TEXT("") );
        return;
    }

    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                      TEXT("System\\CurrentControlSet\\Control\\ProductOptions"),
                      0, KEY_QUERY_VALUE, &hkey ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx( hkey, TEXT("ProductType"), NULL, NULL, (LPBYTE) pszBmpFile,
                             &dwSize ) == ERROR_SUCCESS &&
            (dwSize / sizeof(TCHAR)) + 4 < MAX_PATH + 1 )
        {
            lstrcat( pszBmpFile, gszDotBmp );
        }
        else
        {
            *pszBmpFile = TEXT('\0');
            lstrcpy( pszBmpFile, TEXT("winnt.bmp") );
        }

        RegCloseKey( hkey );
    }
    else
    {
        lstrcpy( pszBmpFile, TEXT("winnt.bmp") );
    }

    // anything but WinNT is a server
    if( ss_lstrcmpi( pszBmpFile, TEXT("winnt.bmp") ) )
        lstrcpy( pszBmpFile, TEXT("lanmannt.bmp") );
}




//-----------------------------------------------------------------------------
// Name: ss_GetTextureFileType()
// Desc: TEX_BMP for a .bmp file
//-----------------------------------------------------------------------------
static int ss_GetTextureFileType( TEXFILE* ptf )
{
    LPTSTR pszExt;

    pszExt = wcsrchr( ptf->szPathName + ptf->nOffset, TEXT('.') );
    if( pszExt && lstrlen( pszExt + 1 ) )
    {
        if( !ss_lstrcmpi( pszExt + 1, gszBmp ) )
            return TEX_BMP;
    }

    return TEX_UNKNOWN;
}




//-----------------------------------------------------------------------------
// Name: ss_VerifyTextureFile()
// Desc: Only bitmaps will do
//-----------------------------------------------------------------------------
static BOOL ss_VerifyTextureFile( TEXFILE* ptf )
{
    TCHAR szBuf[2*MAX_PATH];

    if( !ptf->nOffset || !ptf->szPathName[0] )
        return FALSE;

    if( ss_GetTextureFileType( ptf ) == TEX_BMP )
        return TRUE;

    if( gbInFileDialog )
    {
        wsprintf( szBuf, gszBitmapInvalidMsg, ptf->szPathName );
        MessageBox( NULL, szBuf, gszWarningTitle, MB_OK );
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ss_SelectTextureFile()
// Desc: Lets the user choose a bitmap.  Returns TRUE if a different file
//       was chosen.
//-----------------------------------------------------------------------------
BOOL ss_SelectTextureFile( HWND hDlg, TEXFILE* ptf )
{
    OPENFILENAME ofn;
    TEXFILE newTexFile;
    LPTSTR pszFileName = newTexFile.szPathName;
    TCHAR origPathName[MAX_PATH];
    TCHAR dirName[MAX_PATH];
    LPTSTR pszFile;
    BOOL bFileSelected;

    gbInFileDialog = TRUE;

    // To tell whether the file changed
    lstrcpy( origPathName, ptf->szPathName );

    // The directory and the file name
    if( ptf->nOffset == 0 )
    {
        // Just a file name: look for it
        if( SearchPath( NULL, ptf->szPathName, NULL, MAX_PATH, dirName, &pszFile ) )
        {
            lstrcpy( ptf->szPathName, dirName );
            ptf->nOffset = (int) (pszFile - dirName);
            dirName[ptf->nOffset - 1] = TEXT('\0');
        }
        else
        {
            // not found: start in the Windows directory
            if( !GetWindowsDirectory( dirName, MAX_PATH ) )
                dirName[0] = TEXT('\0');
            pszFile = ptf->szPathName;
        }
    }
    else
    {
        lstrcpy( dirName, ptf->szPathName );
        dirName[ptf->nOffset - 1] = TEXT('\0');
        pszFile = ptf->szPathName + ptf->nOffset;
    }

    lstrcpy( pszFileName, pszFile );

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hDlg;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = gszTextureFilter;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = pszFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = dirName;
    ofn.lpstrTitle = gszTextureDialogTitle;
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = gszBmp;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    for( ;; )
    {
        bFileSelected = GetOpenFileName( &ofn );
        if( !bFileSelected )
            break;

        newTexFile.nOffset = ofn.nFileOffset;
        if( ss_VerifyTextureFile( &newTexFile ) )
        {
            *ptf = newTexFile;
            break;
        }

        // Not a bitmap: try again, in the directory of the file
        if( ofn.nFileOffset )
        {
            lstrcpy( dirName, pszFileName );
            dirName[ofn.nFileOffset - 1] = TEXT('\0');
            lstrcpy( pszFileName, pszFileName + ofn.nFileOffset );
        }
    }

    gbInFileDialog = FALSE;

    if( bFileSelected )
        return lstrcmpi( origPathName, ptf->szPathName ) != 0;

    return FALSE;
}
