//-----------------------------------------------------------------------------
// File: util.h
//
// Desc: Small helper classes of the My Pictures Slideshow screen saver
//       (ssmypics.scr): a string with a built-in buffer, a growable array,
//       a registry key, a device context that restores what was selected
//       into it, a window timer, a string tokenizer and an item ID list.
//
//       The binary has no symbol file: the names of this reconstruction are
//       descriptive, not the original ones.  The layouts are those of the
//       x64 binary (noted at each class).
//-----------------------------------------------------------------------------
#ifndef _MYPICS_UTIL_H
#define _MYPICS_UTIL_H

#include <windows.h>
#include <shlobj.h>


//-----------------------------------------------------------------------------
// Name: class CStr
// Desc: A string with room for 128 characters of its own; longer strings
//       go to the heap, which grows 16 characters at a time.  (0x118 bytes)
//-----------------------------------------------------------------------------
class CStr
{
public:
    enum { c_nBuiltInLength = 128, c_nDefaultGrowBy = 16 };

    CStr();
    CStr( LPCWSTR psz );
    CStr( const CStr& str );
    virtual ~CStr();

    CStr& operator=( LPCWSTR psz )         { if( psz != m_psz ) Assign( psz ); return *this; }
    CStr& operator=( const CStr& str )     { if( &str != this ) Assign( str.m_psz ); return *this; }
    CStr& operator+=( const CStr& str )    { Append( str ); return *this; }

    bool    Assign( LPCWSTR psz );
    bool    Append( const CStr& str );
    bool    LoadString( UINT nId, HINSTANCE hInstance );
    CStr&   Format( UINT nFormatId, HINSTANCE hInstance, ... );

    int     Length() const;
    int     Find( WCHAR ch ) const;
    int     ReverseFind( WCHAR ch ) const;
    bool    IsEqual( const CStr& str ) const;
    void    TrimRight();

    LPCWSTR String() const                 { return m_psz; }
    operator LPCWSTR() const               { return m_psz; }
    WCHAR   operator[]( int i ) const      { return m_psz[i]; }

private:
    bool    Reserve( UINT nLength );
    void    Destroy();

    WCHAR   m_szBuiltIn[c_nBuiltInLength];  // +0x008
    LPWSTR  m_psz;                          // +0x108
    UINT    m_nMaxLength;                   // +0x110
    UINT    m_nGrowBy;                      // +0x114
};

CStr AddBackslash( const CStr& strPath );


//-----------------------------------------------------------------------------
// Name: class CDynamicArray
// Desc: An array that grows by a fixed number of elements.  The elements
//       are created with new[], so a class needs a default constructor and
//       an assignment operator.  (0x20 bytes)
//-----------------------------------------------------------------------------
template <class T>
class CDynamicArray
{
public:
    CDynamicArray( int nGrowBy = 10 )
        : m_nSize(0), m_nMaxSize(0), m_nGrowBy(nGrowBy), m_pData(NULL)
    {
    }

    virtual ~CDynamicArray()
    {
        Destroy();
    }

    int         Size() const                { return m_nSize; }
    T&          operator[]( int i )         { return m_pData[i]; }
    const T&    operator[]( int i ) const   { return m_pData[i]; }

    // Make room for nSize elements.  The storage is replaced whenever the
    // size reaches the capacity, even if it does not grow.
    bool Reserve( int nSize )
    {
        int nOldMaxSize = m_nMaxSize;

        if( nSize < m_nMaxSize )
            return true;

        while( m_nMaxSize < nSize )
            m_nMaxSize += m_nGrowBy;

        T* pData = new T[m_nMaxSize];
        if( pData == NULL )
        {
            m_nMaxSize = nOldMaxSize;
            return false;
        }

        for( int i = 0; i < m_nSize; i++ )
            pData[i] = m_pData[i];

        delete[] m_pData;
        m_pData = pData;
        return true;
    }

    // Add an element; returns its index, or -1
    int Append( const T& item )
    {
        if( !Reserve( m_nSize + 1 ) )
            return -1;

        m_pData[m_nSize] = item;
        return m_nSize++;
    }

    void Destroy()
    {
        delete[] m_pData;
        m_pData = NULL;
        m_nMaxSize = 0;
        m_nSize = 0;
    }

protected:
    int     m_nSize;                        // +0x08
    int     m_nMaxSize;                     // +0x0c
    int     m_nGrowBy;                      // +0x10
    T*      m_pData;                        // +0x18
};


//-----------------------------------------------------------------------------
// Name: class CRegistryKey
// Desc: An open (or created) registry key.  (0x148 bytes)
//-----------------------------------------------------------------------------
class CRegistryKey
{
public:
    CRegistryKey( HKEY hRoot, const CStr& strKey, bool bCreate = false, REGSAM samDesired = 0,
                  LPSECURITY_ATTRIBUTES psa = NULL );
    virtual ~CRegistryKey();

    bool    IsOpen() const                  { return m_hRoot != NULL && m_hKey != NULL; }
    bool    Close();

    DWORD   QueryDword( const CStr& strName, DWORD dwDefault );
    CStr    QueryString( const CStr& strName, const CStr& strDefault );
    LPWSTR  QueryString( const CStr& strName, const CStr& strDefault, LPWSTR pszValue,
                         DWORD cchValue );
    bool    SetDword( const CStr& strName, DWORD dwValue );
    bool    SetString( const CStr& strName, const CStr& strValue, DWORD dwType = REG_SZ );
    bool    DeleteValue( const CStr& strName );

private:
    DWORD   ValueType( const CStr& strName );
    DWORD   ValueSize( const CStr& strName );

    CStr    m_strKey;                       // +0x008
    HKEY    m_hRoot;                        // +0x120
    HKEY    m_hKey;                         // +0x128
    bool    m_bCreate;                      // +0x130
    LPSECURITY_ATTRIBUTES m_psa;            // +0x138
    REGSAM  m_samDesired;                   // +0x140 (0: KEY_ALL_ACCESS)
};


//-----------------------------------------------------------------------------
// Name: class CScopedDC
// Desc: A device context that is released the way it was obtained, after
//       the objects that were selected into it are put back.  (0x90 bytes)
//-----------------------------------------------------------------------------
class CScopedDC
{
public:
    enum
    {
        WindowDC = 0,           // released with ReleaseDC
        ClientDC,               // GetDC
        PaintDC,                // BeginPaint
        MemoryDC,               // CreateCompatibleDC
        CreatedDC,              // deleted with DeleteDC
        NoDC
    };

    CScopedDC();
    virtual ~CScopedDC();

    bool    GetDC( HWND hWnd );
    bool    BeginPaint( HWND hWnd );
    bool    CreateCompatibleDC( HDC hdc );
    void    Release();

    HDC     GetHDC() const                  { return m_hDC; }
    operator HDC() const                    { return m_hDC; }
    bool    IsValid() const                 { return m_hDC != NULL && m_nType != NoDC; }

private:
    void    SelectDefaults();

    HDC         m_hDC;                      // +0x08
    HWND        m_hWnd;                     // +0x10
    int         m_nType;                    // +0x18
    PAINTSTRUCT m_ps;                       // +0x20
    HGDIOBJ     m_hOldBitmap;               // +0x68
    HGDIOBJ     m_hOldBrush;                // +0x70
    HGDIOBJ     m_hOldFont;                 // +0x78
    HGDIOBJ     m_hOldPen;                  // +0x80
    HPALETTE    m_hOldPalette;              // +0x88
};


//-----------------------------------------------------------------------------
// Name: class CWindowTimer
// Desc: The one timer of a window that is running.  (0x10 bytes)
//-----------------------------------------------------------------------------
class CWindowTimer
{
public:
    CWindowTimer() : m_nId(0), m_hWnd(NULL) {}

    void Set( HWND hWnd, UINT nId, UINT nElapse )
    {
        Kill();
        m_hWnd = hWnd;
        m_nId = nId;
        if( hWnd != NULL && nId != 0 )
            SetTimer( hWnd, nId, nElapse, NULL );
    }

    void Kill()
    {
        if( m_hWnd != NULL && m_nId != 0 )
        {
            KillTimer( m_hWnd, m_nId );
            m_hWnd = NULL;
            m_nId = 0;
        }
    }

private:
    UINT    m_nId;                          // +0x0
    HWND    m_hWnd;                         // +0x8
};


//-----------------------------------------------------------------------------
// Name: class CTokenizer
// Desc: Splits a string at a set of delimiter characters.  (0x128 bytes)
//-----------------------------------------------------------------------------
class CTokenizer
{
public:
    CTokenizer( const CStr& str ) : m_str(str), m_nPos(0) {}
    virtual ~CTokenizer() {}

    CStr    NextToken( const CStr& strDelimiters );

private:
    CStr    m_str;                          // +0x008
    int     m_nPos;                         // +0x120
};


//-----------------------------------------------------------------------------
// Name: class CShellIdList
// Desc: An item ID list of the shell, freed with the shell's allocator
//-----------------------------------------------------------------------------
class CShellIdList
{
public:
    CShellIdList() : m_pidl(NULL) {}
    ~CShellIdList()                         { Free(); }

    bool    GetSpecialFolder( HWND hWnd, int nFolder );
    CStr    GetPath() const;
    void    Free();

private:
    LPITEMIDLIST m_pidl;
};


//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------
HPALETTE SelectAndRealizePalette( HDC hdc, HPALETTE hPalette, BOOL bForceBackground );
void     NormalizeRect( RECT* prc );
void     FillUncovered( HDC hdc, const RECT* prcOld, const RECT* prcNew, HBRUSH hbr );
int      RoundUp( int nValue, int nMultiple );
bool     SetDialogIcons( HWND hWnd, HINSTANCE hInstance, UINT nIconId );
bool     BrowseForFolder( HWND hWndOwner, LPCWSTR pszTitle, LPWSTR pszPath );

LRESULT  HandleHelp( WPARAM wParam, LPARAM lParam, const DWORD* pdwHelpIds );
LRESULT  HandleContextMenu( WPARAM wParam, LPARAM lParam, const DWORD* pdwHelpIds );

// The random numbers of the C run time, from a seed of one's own
inline int NextRandom( UINT* pnSeed )
{
    *pnSeed = *pnSeed * 214013 + 2531011;
    return (int)(*pnSeed >> 16) & 0x7fff;
}

extern HINSTANCE g_hInstance;

#endif // _MYPICS_UTIL_H
