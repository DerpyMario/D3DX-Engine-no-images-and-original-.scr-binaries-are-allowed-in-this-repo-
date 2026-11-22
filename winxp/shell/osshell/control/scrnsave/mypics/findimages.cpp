//-----------------------------------------------------------------------------
// File: findimages.cpp
//
// Desc: The search for pictures of the My Pictures Slideshow screen saver
//-----------------------------------------------------------------------------
#include <windows.h>
#include <objidl.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include "findimages.h"




//-----------------------------------------------------------------------------
// Name: FindFiles()
// Desc: The subfolders first (depth first), then the files of the folder
//       matching each of the patterns (separated by semicolons)
//-----------------------------------------------------------------------------
bool FindFiles( const CStr& strDirectory, const CStr& strMasks, PFNFOUNDFILE pfnFound,
                void* pContext, int nDepth, int nMaxDepth )
{
    bool bContinue = true;
    WIN32_FIND_DATAW fd;
    HANDLE hFind;

    if( nDepth >= nMaxDepth )
        return bContinue;

    // the folders
    CStr strSearch( AddBackslash( strDirectory ) );
    strSearch += CStr( L"*" );

    hFind = FindFirstFileW( strSearch, &fd );
    if( hFind != INVALID_HANDLE_VALUE )
    {
        do
        {
            if( !bContinue )
                break;

            if( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                lstrcmpW( fd.cFileName, L".." ) != 0 && lstrcmpW( fd.cFileName, L"." ) != 0 )
            {
                if( pfnFound != NULL )
                {
                    CStr strPath( AddBackslash( strDirectory ) );
                    strPath += CStr( fd.cFileName );
                    bContinue = pfnFound( false, strPath, &fd, pContext );
                }

                if( bContinue )
                {
                    CStr strPath( AddBackslash( strDirectory ) );
                    strPath += CStr( fd.cFileName );
                    bContinue = FindFiles( strPath, strMasks, pfnFound, pContext, nDepth + 1,
                                           nMaxDepth );
                }
            }
        }
        while( FindNextFileW( hFind, &fd ) );

        FindClose( hFind );
    }

    // the files
    CTokenizer tokenizer( strMasks );
    while( bContinue )
    {
        CStr strMask = tokenizer.NextToken( CStr( L";" ) );
        strMask.TrimRight();
        if( strMask.Length() == 0 )
            break;

        strSearch = AddBackslash( strDirectory );
        strSearch += strMask;

        hFind = FindFirstFileW( strSearch, &fd );
        if( hFind != INVALID_HANDLE_VALUE )
        {
            do
            {
                if( !bContinue )
                    break;

                if( pfnFound != NULL )
                {
                    CStr strPath( AddBackslash( strDirectory ) );
                    strPath += CStr( fd.cFileName );
                    bContinue = pfnFound( true, strPath, &fd, pContext );
                }
            }
            while( FindNextFileW( hFind, &fd ) );

            FindClose( hFind );
        }
    }

    return bContinue;
}




//-----------------------------------------------------------------------------
// Name: CImageSignature
// Desc:
//-----------------------------------------------------------------------------
CImageSignature::CImageSignature()
    : m_pPattern(NULL), m_pMask(NULL), m_nSize(0)
{
    ZeroMemory( &m_guidFormat, sizeof(m_guidFormat) );
    ZeroMemory( &m_clsidCodec, sizeof(m_clsidCodec) );
}

void CImageSignature::Destroy()
{
    delete[] m_pPattern;
    m_pPattern = NULL;
    delete[] m_pMask;
    m_pMask = NULL;
    m_nSize = 0;
    ZeroMemory( &m_guidFormat, sizeof(m_guidFormat) );
    ZeroMemory( &m_clsidCodec, sizeof(m_clsidCodec) );
}

CImageSignature& CImageSignature::operator=( const CImageSignature& sig )
{
    if( &sig != this )
        Assign( sig.m_pPattern, sig.m_pMask, sig.m_nSize, sig.m_guidFormat, sig.m_clsidCodec );
    return *this;
}

bool CImageSignature::Assign( const BYTE* pPattern, const BYTE* pMask, int nSize,
                              const GUID& guidFormat, const GUID& clsidCodec )
{
    Destroy();

    m_nSize = nSize;
    m_guidFormat = guidFormat;
    m_clsidCodec = clsidCodec;

    if( nSize != 0 && pPattern != NULL && pMask != NULL )
    {
        m_pPattern = new BYTE[nSize];
        m_pMask = new BYTE[nSize];
        if( m_pPattern != NULL && m_pMask != NULL )
        {
            memcpy( m_pPattern, pPattern, nSize );
            memcpy( m_pMask, pMask, nSize );
            return true;
        }
    }

    Destroy();
    return false;
}

bool CImageSignature::Matches( const BYTE* pData, int cbData ) const
{
    if( cbData < m_nSize )
        return false;

    for( int i = 0; i < m_nSize; i++ )
    {
        if( (pData[i] & m_pMask[i]) != m_pPattern[i] )
            return false;
    }
    return true;
}




//-----------------------------------------------------------------------------
// Name: CImageSniffer::CImageSniffer()
// Desc: Collects the signatures of the decoders (GDI+ must be running)
//-----------------------------------------------------------------------------
CImageSniffer::CImageSniffer()
    : CDynamicArray<CImageSignature>( 10 ), m_nMaxSize(0), m_pBuffer(NULL)
{
    UINT nDecoders = 0, cbDecoders = 0;

    if( Gdiplus::GetImageDecodersSize( &nDecoders, &cbDecoders ) == Gdiplus::Ok &&
        cbDecoders != 0 && nDecoders != 0 )
    {
        Gdiplus::ImageCodecInfo* pDecoders = (Gdiplus::ImageCodecInfo*)LocalAlloc( LPTR, cbDecoders );
        if( pDecoders != NULL )
        {
            if( Gdiplus::GetImageDecoders( nDecoders, cbDecoders, pDecoders ) == Gdiplus::Ok )
            {
                for( UINT i = 0; i < nDecoders; i++ )
                {
                    const Gdiplus::ImageCodecInfo* pCodec = &pDecoders[i];

                    for( UINT j = 0; j < pCodec->SigCount; j++ )
                    {
                        CImageSignature sig;
                        sig.Assign( pCodec->SigPattern + j * pCodec->SigSize,
                                    pCodec->SigMask + j * pCodec->SigSize, pCodec->SigSize,
                                    pCodec->FormatID, pCodec->Clsid );
                        Append( sig );
                    }
                }
            }
            LocalFree( pDecoders );
        }
    }

    m_nMaxSize = 0;
    for( int i = 0; i < Size(); i++ )
    {
        if( m_nMaxSize < (*this)[i].Size() )
            m_nMaxSize = (*this)[i].Size();
    }

    if( m_nMaxSize != 0 )
        m_pBuffer = new BYTE[m_nMaxSize];

    if( m_pBuffer == NULL || m_nMaxSize == 0 || Size() == 0 )
    {
        delete[] m_pBuffer;
        m_pBuffer = NULL;
        m_nMaxSize = 0;
        Destroy();
    }
}

CImageSniffer::~CImageSniffer()
{
    delete[] m_pBuffer;
    m_pBuffer = NULL;
    m_nMaxSize = 0;
    Destroy();
}




//-----------------------------------------------------------------------------
// Name: CImageSniffer::GetImageFormat()
// Desc: The format of the first signature the start of the stream matches
//       (GUID_NULL: none)
//-----------------------------------------------------------------------------
GUID CImageSniffer::GetImageFormat( IStream* pStream )
{
    GUID guidFormat;
    ZeroMemory( &guidFormat, sizeof(guidFormat) );

    if( pStream == NULL )
        return guidFormat;

    ULONG cbRead = 0;
    if( SUCCEEDED( pStream->Read( m_pBuffer, m_nMaxSize, &cbRead ) ) && cbRead != 0 )
    {
        for( int i = 0; i < Size(); i++ )
        {
            if( (*this)[i].Matches( m_pBuffer, (int)cbRead ) )
            {
                guidFormat = (*this)[i].Format();
                break;
            }
        }
    }

    return guidFormat;
}

bool CImageSniffer::IsImageFile( LPCWSTR pszFile )
{
    bool bImage = false;
    IStream* pStream = NULL;

    if( SUCCEEDED( SHCreateStreamOnFileW( pszFile, STGM_READ | STGM_SHARE_DENY_WRITE, &pStream ) ) &&
        pStream != NULL )
    {
        GUID guidFormat = GetImageFormat( pStream );
        bImage = !IsEqualGUID( guidFormat, GUID_NULL );
    }

    if( pStream != NULL )
        pStream->Release();

    return bImage;
}




//-----------------------------------------------------------------------------
// Name: CImageFinder::CImageFinder()
// Desc: The thread gets its own handle of the stop event
//-----------------------------------------------------------------------------
CImageFinder::CImageFinder( const CStr& strDirectory, const CStr& strMasks, HWND hWnd, UINT uMsg,
                            HANDLE hStopEvent, int nMaxFailedFiles, int nMaxFoundFiles,
                            int nMaxDirectories )
    : m_strDirectory(strDirectory), m_strMasks(strMasks), m_hWnd(hWnd), m_uMsg(uMsg),
      m_hStopEvent(NULL), m_nDirectories(0), m_nFailedFiles(0), m_nFoundFiles(0),
      m_nMaxFailedFiles(nMaxFailedFiles), m_nMaxFoundFiles(nMaxFoundFiles),
      m_nMaxDirectories(nMaxDirectories)
{
    if( !DuplicateHandle( GetCurrentProcess(), hStopEvent, GetCurrentProcess(), &m_hStopEvent,
                          0, FALSE, DUPLICATE_SAME_ACCESS ) )
    {
        m_hStopEvent = NULL;
    }
}

CImageFinder::~CImageFinder()
{
    if( m_hStopEvent != NULL )
    {
        CloseHandle( m_hStopEvent );
        m_hStopEvent = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: CImageFinder::ThreadProc()
// Desc:
//-----------------------------------------------------------------------------
DWORD WINAPI CImageFinder::ThreadProc( LPVOID pvParam )
{
    CImageFinder* pFinder = (CImageFinder*)pvParam;

    if( pFinder != NULL )
    {
        pFinder->Run();
        delete pFinder;
    }
    return 0;
}




//-----------------------------------------------------------------------------
// Name: CImageFinder::Run()
// Desc: The search; at its end the window gets the message without a file
//-----------------------------------------------------------------------------
bool CImageFinder::Run()
{
    CStr strDirectory( m_strDirectory );
    bool bResult = FindFiles( strDirectory, m_strMasks, FoundCallback, this, 0, 10 );

    if( m_uMsg != 0 && m_hWnd != NULL && IsWindow( m_hWnd ) )
        PostMessageW( m_hWnd, m_uMsg, 0, 0 );

    return bResult;
}

bool CImageFinder::FoundCallback( bool bFile, LPCWSTR pszPath, const WIN32_FIND_DATAW* pFindData,
                                  void* pContext )
{
    if( pContext == NULL )
        return false;
    return ((CImageFinder*)pContext)->OnFound( bFile, pszPath );
}




//-----------------------------------------------------------------------------
// Name: CImageFinder::OnFound()
// Desc: Posts the pictures to the window (wParam 1, lParam a CStr the window
//       deletes).  Returns false to stop: when the stop event is set, or a
//       limit is reached.
//-----------------------------------------------------------------------------
bool CImageFinder::OnFound( bool bFile, LPCWSTR pszPath )
{
    if( m_hStopEvent != NULL && WaitForSingleObject( m_hStopEvent, 0 ) == WAIT_OBJECT_0 )
        return false;

    if( !bFile )
    {
        m_nDirectories++;
    }
    else if( m_uMsg != 0 && m_hWnd != NULL && IsWindow( m_hWnd ) )
    {
        if( !m_Sniffer.IsImageFile( pszPath ) )
        {
            m_nFailedFiles++;
        }
        else
        {
            m_nFoundFiles++;

            CStr* pstrFile = new CStr( CStr( pszPath ) );
            if( pstrFile != NULL )
                PostMessageW( m_hWnd, m_uMsg, 1, (LPARAM)pstrFile );
        }
    }

    return (m_nMaxFailedFiles == 0 || m_nFailedFiles < m_nMaxFailedFiles) &&
           (m_nMaxFoundFiles == 0 || m_nFoundFiles < m_nMaxFoundFiles) &&
           (m_nMaxDirectories == 0 || m_nDirectories < m_nMaxDirectories);
}
