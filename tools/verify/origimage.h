//-----------------------------------------------------------------------------
// File: origimage.h
//
// Desc: Support code for the differential tests in this directory.
//
//       The tests check the reconstructed sources against the code they were
//       reconstructed from: they map one of YOUR original screen savers into
//       the test process (without running it), call individual functions of
//       it by address and compare the results with what the rebuilt functions
//       return for the same input.
//
//       Addresses are relative virtual addresses taken from the public symbol
//       files; they are only valid for the exact builds named in each test.
//-----------------------------------------------------------------------------
#ifndef _VERIFY_ORIGIMAGE_H
#define _VERIFY_ORIGIMAGE_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//-----------------------------------------------------------------------------
// Name: class OrigImage
// Desc: Maps a PE32+ executable by hand: sections, base relocations and
//       imports, but no entry point and no static initialisers.
//-----------------------------------------------------------------------------
class OrigImage
{
public:
    OrigImage() : m_pBase( NULL ), m_cbImage( 0 ) { }

    bool Load( const char* pszPath, DWORD dwExpectedTimeStamp )
    {
        FILE* f = fopen( pszPath, "rb" );
        if( f == NULL )
        {
            printf( "cannot open %s\n", pszPath );
            return false;
        }
        fseek( f, 0, SEEK_END );
        long cbFile = ftell( f );
        fseek( f, 0, SEEK_SET );
        BYTE* pFile = (BYTE*)malloc( cbFile );
        if( fread( pFile, 1, cbFile, f ) != (size_t)cbFile )
        {
            fclose( f );
            return false;
        }
        fclose( f );

        IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)pFile;
        IMAGE_NT_HEADERS64* pNt = (IMAGE_NT_HEADERS64*)( pFile + pDos->e_lfanew );
        if( pDos->e_magic != IMAGE_DOS_SIGNATURE || pNt->Signature != IMAGE_NT_SIGNATURE ||
            pNt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC )
        {
            printf( "%s is not a PE32+ image\n", pszPath );
            return false;
        }
        if( dwExpectedTimeStamp != 0 && pNt->FileHeader.TimeDateStamp != dwExpectedTimeStamp )
        {
            printf( "%s: time stamp %08lx, but the test was written for build %08lx\n", pszPath,
                    (unsigned long)pNt->FileHeader.TimeDateStamp, (unsigned long)dwExpectedTimeStamp );
            return false;
        }

        m_cbImage = pNt->OptionalHeader.SizeOfImage;
        m_pBase = (BYTE*)VirtualAlloc( NULL, m_cbImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
        if( m_pBase == NULL )
            return false;

        memcpy( m_pBase, pFile, pNt->OptionalHeader.SizeOfHeaders );
        IMAGE_SECTION_HEADER* pSection = IMAGE_FIRST_SECTION( pNt );
        for( WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++ )
        {
            if( pSection[i].SizeOfRawData != 0 )
                memcpy( m_pBase + pSection[i].VirtualAddress, pFile + pSection[i].PointerToRawData,
                        pSection[i].SizeOfRawData );
        }

        // Base relocations
        LONGLONG llDelta = (LONGLONG)( (ULONGLONG)m_pBase - pNt->OptionalHeader.ImageBase );
        IMAGE_DATA_DIRECTORY* pDir = &pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if( pDir->Size != 0 )
        {
            BYTE* p = m_pBase + pDir->VirtualAddress;
            BYTE* pEnd = p + pDir->Size;
            while( p < pEnd )
            {
                IMAGE_BASE_RELOCATION* pBlock = (IMAGE_BASE_RELOCATION*)p;
                if( pBlock->SizeOfBlock == 0 )
                    break;
                WORD* pEntry = (WORD*)( pBlock + 1 );
                DWORD cEntries = ( pBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION) ) / sizeof(WORD);
                for( DWORD i = 0; i < cEntries; i++ )
                {
                    if( ( pEntry[i] >> 12 ) == IMAGE_REL_BASED_DIR64 )
                        *(LONGLONG*)( m_pBase + pBlock->VirtualAddress + ( pEntry[i] & 0xfff ) ) += llDelta;
                }
                p += pBlock->SizeOfBlock;
            }
        }

        // Imports
        pDir = &pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if( pDir->Size != 0 )
        {
            IMAGE_IMPORT_DESCRIPTOR* pImport = (IMAGE_IMPORT_DESCRIPTOR*)( m_pBase + pDir->VirtualAddress );
            for( ; pImport->Name != 0; pImport++ )
            {
                const char* pszDll = (const char*)( m_pBase + pImport->Name );
                HMODULE hDll = LoadLibraryA( pszDll );
                ULONGLONG* pThunk = (ULONGLONG*)( m_pBase + pImport->FirstThunk );
                ULONGLONG* pLookup = (ULONGLONG*)( m_pBase + ( pImport->OriginalFirstThunk ?
                                                               pImport->OriginalFirstThunk : pImport->FirstThunk ) );
                for( ; *pLookup != 0; pLookup++, pThunk++ )
                {
                    FARPROC pfn = NULL;
                    if( hDll != NULL )
                    {
                        if( *pLookup & IMAGE_ORDINAL_FLAG64 )
                            pfn = GetProcAddress( hDll, (LPCSTR)( *pLookup & 0xffff ) );
                        else
                            pfn = GetProcAddress( hDll, (const char*)( m_pBase + *pLookup + 2 ) );
                    }
                    // Unresolved imports are left pointing at a trap
                    *pThunk = pfn ? (ULONGLONG)pfn : (ULONGLONG)&OrigImage::MissingImport;
                }
            }
        }

        // Unwind information, so that a fault inside the image unwinds cleanly
        pDir = &pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if( pDir->Size != 0 )
        {
            RtlAddFunctionTable( (PRUNTIME_FUNCTION)( m_pBase + pDir->VirtualAddress ),
                                 pDir->Size / sizeof(RUNTIME_FUNCTION), (DWORD64)m_pBase );
        }

        free( pFile );
        return true;
    }

    // Address of a function or of a variable of the original image
    template <class T> T At( DWORD rva ) const  { return (T)( m_pBase + rva ); }

    BYTE* Base() const                           { return m_pBase; }

private:
    static void MissingImport()
    {
        printf( "FATAL: the original code called an import that could not be resolved\n" );
        fflush( stdout );
        ExitProcess( 3 );
    }

    BYTE* m_pBase;
    DWORD m_cbImage;
};

//-----------------------------------------------------------------------------
// Bookkeeping shared by the tests
//-----------------------------------------------------------------------------
static int g_nChecks = 0;
static int g_nFailures = 0;

static inline void Check( bool bOk, const char* pszWhat )
{
    g_nChecks++;
    if( !bOk )
    {
        g_nFailures++;
        printf( "  FAIL  %s\n", pszWhat );
    }
    else
    {
        printf( "  ok    %s\n", pszWhat );
    }
}

// Largest absolute difference between two float arrays, and how many elements
// are not bit-for-bit identical.
static inline float MaxFloatDiff( const float* a, const float* b, size_t n, size_t* pcDifferent )
{
    float fMax = 0.0f;
    size_t cDifferent = 0;
    for( size_t i = 0; i < n; i++ )
    {
        if( memcmp( &a[i], &b[i], sizeof(float) ) != 0 )
            cDifferent++;
        float d = fabsf( a[i] - b[i] );
        if( d > fMax || d != d )
            fMax = ( d != d ) ? 1e30f : d;
    }
    if( pcDifferent )
        *pcDifferent = cDifferent;
    return fMax;
}

static inline int Summary( const char* pszName )
{
    printf( "%s: %d check(s), %d failure(s)\n", pszName, g_nChecks, g_nFailures );
    return g_nFailures ? 1 : 0;
}

#endif // _VERIFY_ORIGIMAGE_H
