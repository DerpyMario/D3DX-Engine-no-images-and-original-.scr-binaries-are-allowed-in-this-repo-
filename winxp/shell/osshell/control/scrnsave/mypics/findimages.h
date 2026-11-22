//-----------------------------------------------------------------------------
// File: findimages.h
//
// Desc: The search for pictures of the My Pictures Slideshow screen saver.
//       A thread of low priority walks the picture folder and its
//       subfolders (ten levels deep), checks the first bytes of each file
//       whose name matches one of the patterns of the GDI+ decoders against
//       the decoders' signatures, and posts each picture it finds to the
//       screen saver window.
//-----------------------------------------------------------------------------
#ifndef _MYPICS_FINDIMAGES_H
#define _MYPICS_FINDIMAGES_H

#include <windows.h>
#include <objidl.h>
#include "util.h"

// Called for each folder (bFile false) and each file found; false stops
// the search
typedef bool (*PFNFOUNDFILE)( bool bFile, LPCWSTR pszPath, const WIN32_FIND_DATAW* pFindData,
                              void* pContext );

bool FindFiles( const CStr& strDirectory, const CStr& strMasks, PFNFOUNDFILE pfnFound,
                void* pContext, int nDepth, int nMaxDepth );


//-----------------------------------------------------------------------------
// Name: class CImageSignature
// Desc: The first bytes of a file of one image format: (byte & mask) must
//       equal the pattern.  (0x38 bytes)
//-----------------------------------------------------------------------------
class CImageSignature
{
public:
    CImageSignature();
    ~CImageSignature()                      { Destroy(); }

    CImageSignature& operator=( const CImageSignature& sig );
    bool    Assign( const BYTE* pPattern, const BYTE* pMask, int nSize, const GUID& guidFormat,
                    const GUID& clsidCodec );
    bool    Matches( const BYTE* pData, int cbData ) const;

    const GUID& Format() const              { return m_guidFormat; }
    int     Size() const                    { return m_nSize; }

private:
    void    Destroy();

    BYTE*   m_pPattern;                     // +0x00
    BYTE*   m_pMask;                        // +0x08
    int     m_nSize;                        // +0x10
    GUID    m_guidFormat;                   // +0x14
    GUID    m_clsidCodec;                   // +0x24
};


//-----------------------------------------------------------------------------
// Name: class CImageSniffer
// Desc: The signatures of all GDI+ decoders.  (0x30 bytes)
//-----------------------------------------------------------------------------
class CImageSniffer : public CDynamicArray<CImageSignature>
{
public:
    CImageSniffer();
    virtual ~CImageSniffer();

    GUID    GetImageFormat( IStream* pStream );
    bool    IsImageFile( LPCWSTR pszFile );

private:
    int     m_nMaxSize;                     // +0x20 the longest signature
    BYTE*   m_pBuffer;                      // +0x28 for the start of a file
};


//-----------------------------------------------------------------------------
// Name: class CImageFinder
// Desc: The search thread.  The limits are counted separately for folders,
//       for files that are pictures and for files that are not; 0 is no
//       limit.  (0x290 bytes)
//-----------------------------------------------------------------------------
class CImageFinder
{
public:
    CImageFinder( const CStr& strDirectory, const CStr& strMasks, HWND hWnd, UINT uMsg,
                  HANDLE hStopEvent, int nMaxFailedFiles, int nMaxFoundFiles,
                  int nMaxDirectories );
    ~CImageFinder();

    static DWORD WINAPI ThreadProc( LPVOID pvParam );

private:
    bool    Run();
    bool    OnFound( bool bFile, LPCWSTR pszPath );
    static bool FoundCallback( bool bFile, LPCWSTR pszPath, const WIN32_FIND_DATAW* pFindData,
                               void* pContext );

    CStr            m_strDirectory;         // +0x000
    CStr            m_strMasks;             // +0x118
    HWND            m_hWnd;                 // +0x230
    UINT            m_uMsg;                 // +0x238
    HANDLE          m_hStopEvent;           // +0x240 (a duplicate)
    int             m_nDirectories;         // +0x248
    int             m_nFailedFiles;         // +0x24c
    int             m_nFoundFiles;          // +0x250
    int             m_nMaxFailedFiles;      // +0x254
    int             m_nMaxFoundFiles;       // +0x258
    int             m_nMaxDirectories;      // +0x25c
    CImageSniffer   m_Sniffer;              // +0x260
};

#endif // _MYPICS_FINDIMAGES_H
