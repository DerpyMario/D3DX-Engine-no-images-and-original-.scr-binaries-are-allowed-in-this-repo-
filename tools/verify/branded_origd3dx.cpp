//-----------------------------------------------------------------------------
// File: branded_origd3dx.cpp
//
// Desc: Run-time check of the reconstructed Windows Energy screen saver on
//       systems whose d3dx9 cannot build its media (Wine's d3dx9 neither
//       compiles effects that use sampler_state blocks nor implements
//       D3DXMESHOPT_VERTEXCACHE).
//
//       The original ssBranded.scr links D3DX statically.  This file is
//       linked into the REBUILT screen saver with
//
//           -Wl,--wrap=D3DXCreateEffectFromResourceW,--wrap=D3DXLoadMeshFromXResource
//           -Wl,--wrap=D3DXGetShaderOutputSemantics
//
//       and sends those calls into the D3DX code of YOUR original
//       ssBranded.scr, mapped into the process with OrigImage.  Everything
//       else - the whole screen saver - is the rebuilt code, so the two can
//       be compared frame by frame.
//
//       The original is looked for in %SSBRANDED_ORIGINAL%, else in
//       "ssBranded.orig.scr" next to the executable.
//-----------------------------------------------------------------------------
#include <d3dx9.h>
#include "origimage.h"

// ssBranded.scr 6.0.6001.18000 (amd64)
#define BRANDED_TIMESTAMP                   0x479197c3
#define RVA_XC_A                            0x00001648  // C++ initializers
#define RVA_XC_Z                            0x00001668
#define RVA_D3DXCreateEffectFromResourceW   0x00040b1c
#define RVA_D3DXLoadMeshFromXHelper         0x00043190  // D3DXMesh::D3DXLoadMeshFromXHelper
#define RVA_D3DXGetShaderOutputSemantics    0x00053464

typedef void ( __cdecl *PFNINIT )();
typedef HRESULT ( WINAPI *PFNCREATEEFFECTFROMRESOURCEW )( IDirect3DDevice9*, HMODULE, LPCWSTR, const D3DXMACRO*,
                                                          ID3DXInclude*, DWORD, ID3DXEffectPool*, ID3DXEffect**,
                                                          ID3DXBuffer** );
typedef HRESULT ( __cdecl *PFNLOADMESHHELPER )( void* pSource, DWORD dwSourceType, DWORD dwOptions,
                                                IDirect3DDevice9*, ID3DXBuffer**, ID3DXBuffer**, ID3DXBuffer**,
                                                DWORD*, ID3DXMesh** );
typedef HRESULT ( WINAPI *PFNGETSHADEROUTPUTSEMANTICS )( const DWORD*, D3DXSEMANTIC*, UINT* );

static OrigImage s_Orig;

static bool LoadOriginal()
{
    static int s_nState = 0;        // 0 not tried, 1 loaded, -1 failed

    if( s_nState == 0 )
    {
        char szPath[MAX_PATH];

        if( GetEnvironmentVariableA( "SSBRANDED_ORIGINAL", szPath, MAX_PATH ) == 0 )
        {
            GetModuleFileNameA( NULL, szPath, MAX_PATH );
            char* pSlash = strrchr( szPath, '\\' );
            strcpy( pSlash != NULL ? pSlash + 1 : szPath, "ssBranded.orig.scr" );
        }

        s_nState = -1;
        if( s_Orig.Load( szPath, BRANDED_TIMESTAMP ) )
        {
            // the static initializers of the D3DX library
            for( PFNINIT* p = s_Orig.At<PFNINIT*>( RVA_XC_A ); p < s_Orig.At<PFNINIT*>( RVA_XC_Z ); p++ )
            {
                if( *p != NULL )
                    ( *p )();
            }
            s_nState = 1;
        }
        else
        {
            MessageBoxA( NULL, "The original ssBranded.scr was not found (see SSBRANDED_ORIGINAL).",
                         "branded_origd3dx", MB_OK );
        }
    }

    return s_nState == 1;
}

extern "C" HRESULT WINAPI __wrap_D3DXCreateEffectFromResourceW( IDirect3DDevice9* pDevice, HMODULE hSrcModule,
                                                                LPCWSTR pSrcResource, const D3DXMACRO* pDefines,
                                                                ID3DXInclude* pInclude, DWORD Flags,
                                                                ID3DXEffectPool* pPool, ID3DXEffect** ppEffect,
                                                                ID3DXBuffer** ppCompilationErrors )
{
    if( !LoadOriginal() )
        return E_FAIL;

    return s_Orig.At<PFNCREATEEFFECTFROMRESOURCEW>( RVA_D3DXCreateEffectFromResourceW )(
        pDevice, hSrcModule, pSrcResource, pDefines, pInclude, Flags, pPool, ppEffect, ppCompilationErrors );
}

extern "C" HRESULT WINAPI __wrap_D3DXLoadMeshFromXResource( HMODULE hModule, LPCSTR pName, LPCSTR pType,
                                                            DWORD Options, IDirect3DDevice9* pDevice,
                                                            ID3DXBuffer** ppAdjacency, ID3DXBuffer** ppMaterials,
                                                            ID3DXBuffer** ppEffectInstances, DWORD* pNumMaterials,
                                                            ID3DXMesh** ppMesh )
{
    struct { HMODULE hModule; LPCSTR pName; LPCSTR pType; } source = { hModule, pName, pType };

    if( !LoadOriginal() )
        return E_FAIL;

    return s_Orig.At<PFNLOADMESHHELPER>( RVA_D3DXLoadMeshFromXHelper )(
        &source, 2 /* resource */, Options, pDevice, ppAdjacency, ppMaterials, ppEffectInstances,
        pNumMaterials, ppMesh );
}

// (The effects of the original D3DX describe their shaders in its own way;
// ask the same D3DX about them.)
extern "C" HRESULT WINAPI __wrap_D3DXGetShaderOutputSemantics( const DWORD* pFunction, D3DXSEMANTIC* pSemantics,
                                                               UINT* pCount )
{
    if( !LoadOriginal() )
        return E_FAIL;

    return s_Orig.At<PFNGETSHADEROUTPUTSEMANTICS>( RVA_D3DXGetShaderOutputSemantics )( pFunction, pSemantics, pCount );
}
