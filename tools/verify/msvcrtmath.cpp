//-----------------------------------------------------------------------------
// File: msvcrtmath.cpp
//
// Desc: MinGW-w64 only.  The original screen savers call the single-precision
//       math functions of msvcrt.dll, whereas MinGW links its own versions,
//       which occasionally differ in the last bit.  To compare like with like
//       the tests are linked with  -Wl,--wrap=sinf,--wrap=cosf,...  and the
//       wrappers below forward to msvcrt.dll.  (Not needed for MSVC builds.)
//-----------------------------------------------------------------------------
#include <windows.h>

typedef float ( __cdecl *PFN_F1 )( float );
typedef float ( __cdecl *PFN_F2 )( float, float );

static FARPROC Msvcrt( const char* pszName )
{
    static HMODULE hCrt = LoadLibraryA( "msvcrt.dll" );
    return GetProcAddress( hCrt, pszName );
}

#define WRAP1(name) \
    extern "C" float __wrap_##name( float x ) \
    { static PFN_F1 pfn = (PFN_F1)Msvcrt( #name ); return pfn( x ); }

#define WRAP2(name) \
    extern "C" float __wrap_##name( float x, float y ) \
    { static PFN_F2 pfn = (PFN_F2)Msvcrt( #name ); return pfn( x, y ); }

WRAP1( sinf )
WRAP1( cosf )
WRAP1( tanf )
WRAP1( acosf )
WRAP1( asinf )
WRAP1( atanf )
WRAP1( sqrtf )
WRAP1( expf )
WRAP1( logf )
WRAP1( floorf )
WRAP1( ceilf )
WRAP2( atan2f )
WRAP2( powf )
WRAP2( fmodf )
