//-----------------------------------------------------------------------------
// File: resourcefile.cpp
//
// Desc: CResourceFile - see resourcefile.h.
//
//       Original object:
//           shell\osshell\control\scrnsave\branded\src\objfre\amd64\resourcefile.obj
//-----------------------------------------------------------------------------
#include "resourcefile.h"




//-----------------------------------------------------------------------------
// Name: GetString()
// Desc: Copies the next line (without its line feed; a carriage return is
//       kept) into pszBuffer, at most cchBuffer - 1 characters.  Returns
//       pszBuffer and in *pcchRead the number of characters stored plus one,
//       or NULL (and 0) at the end of the resource.
//
//       The parameters are in the order of the decorated name in the symbol
//       file.  (The shipped code receives the length pointer in the register
//       of the second and the buffer size in that of the third argument.)
//-----------------------------------------------------------------------------
char* CResourceFile::GetString( char* pszBuffer, size_t cchBuffer, size_t* pcchRead )
{
    char* pSrc = m_pCur;
    char* pDst = pszBuffer;
    int   cchMax = (int)cchBuffer - 1;
    int   cchScanned = 0;
    int   cchStored = 0;
    bool  bEnd = false;

    if( cchMax > 0 )
    {
        for( ;; )
        {
            if( m_cbLeft == 0 )
            {
                bEnd = true;
                break;
            }

            if( *pSrc != '\n' )
            {
                *pDst++ = *pSrc;
                cchStored++;
            }

            m_pCur++;
            m_cbLeft--;
            pSrc++;
            cchScanned++;

            if( *pSrc == '\n' || cchScanned >= cchMax )
                break;
        }
    }

    *pDst = '\0';

    if( bEnd )
    {
        *pcchRead = 0;
        return NULL;
    }

    *pcchRead = (size_t)( cchStored + 1 );
    return pszBuffer;
}
