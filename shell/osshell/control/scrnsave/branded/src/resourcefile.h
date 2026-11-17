//-----------------------------------------------------------------------------
// File: resourcefile.h
//
// Desc: CResourceFile, reads a text resource of the executable line by line.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\resourcefile.obj
//-----------------------------------------------------------------------------
#ifndef _BRANDED_RESOURCEFILE_H
#define _BRANDED_RESOURCEFILE_H

#include <windows.h>

class CResourceFile
{
public:
    CResourceFile()
    {
        m_hResInfo = NULL;
        m_hResData = NULL;
        m_pData = NULL;
        m_cbData = 0;
        m_cbLeft = 0;
        m_pCur = NULL;
    }

    ~CResourceFile()
    {
        if( m_hResData != NULL )
            FreeResource( m_hResData );
    }

    // Returns FALSE if the resource does not exist or is empty.
    BOOL Open( LPCWSTR pszName, LPCWSTR pszType )
    {
        m_hResInfo = FindResourceW( NULL, pszName, pszType );
        if( m_hResInfo == NULL )
            return FALSE;

        m_hResData = LoadResource( NULL, m_hResInfo );
        if( m_hResData == NULL )
            return FALSE;

        m_pData = (BYTE*)LockResource( m_hResData );
        if( m_pData == NULL )
            return FALSE;

        m_cbData = SizeofResource( NULL, m_hResInfo );
        if( m_cbData == 0 )
            return FALSE;

        m_pCur = (char*)m_pData;
        m_cbLeft = m_cbData;
        return TRUE;
    }

    char*   GetString( char* pszBuffer, size_t cchBuffer, size_t* pcchRead );

public:
    HRSRC   m_hResInfo;
    HGLOBAL m_hResData;
    BYTE*   m_pData;
    size_t  m_cbData;
    size_t  m_cbLeft;
    char*   m_pCur;
};

#if defined(_WIN64)
static_assert( sizeof(CResourceFile) == 0x30, "CResourceFile layout" );
#endif

#endif // _BRANDED_RESOURCEFILE_H
