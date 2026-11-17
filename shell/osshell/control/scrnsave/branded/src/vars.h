//-----------------------------------------------------------------------------
// File: vars.h
//
// Desc: Windows Energy screen saver (ssBranded.scr) - global state shared by
//       all the graphic objects, and small helpers.
//
//       The names of the globals are those of the symbol file; the
//       enumerators of SSState and SSMsg and the helper functions are named
//       by this reconstruction.
//-----------------------------------------------------------------------------
#ifndef _BRANDED_VARS_H
#define _BRANDED_VARS_H

#include <windows.h>
#include <stdlib.h>
#include <math.h>
#include <d3d9.h>
#include <d3dx9.h>

// The DirectX SDK defines D3DXMATRIXA16, a D3DXMATRIX on a 16 byte boundary;
// the d3dx9 headers of MinGW-w64 and Wine do not.
#if !defined(D3DX_ALIGN16) && defined(__GNUC__)
typedef D3DXMATRIX __attribute__(( aligned( 16 ) )) D3DXMATRIXA16;
#endif

//-----------------------------------------------------------------------------
// The show runs through three scenes, connected by wipes:
//
//   logo  --wipe-->  Microsoft flare  --wipe-->  flare (pills and auroras)  --wipe-->  logo ...
//-----------------------------------------------------------------------------
enum SSState
{
    SS_NONE = 0,                // the matrix stacks could not be created
    SS_START,                   // first frame
    SS_LOGO,                    // the Windows logo
    SS_LOGO_TO_MSFLARE,         // wipe 1
    SS_MSFLARE,                 // the Microsoft flare
    SS_MSFLARE_TO_FLARE,        // wipe 2 (the flare scene flies in meanwhile)
    SS_FLARE_IN,                // the flare scene flies in
    SS_FLARE,                   // the flare scene
    SS_FLARE_OUT,               // the flare scene flies out
    SS_FLARE_TO_LOGO,           // wipe 3 (to black)
    SS_DONE                     // shut down
};

// Posted by the scenes; CSSBranded::FrameMove() acts on them.
enum SSMsg
{
    SSMSG_NONE = 0,
    SSMSG_LOGO_DONE,            // start wipe 1
    SSMSG_LOGO_TO_MSFLARE_DONE,
    SSMSG_MSFLARE_DONE,         // start wipe 2
    SSMSG_MSFLARE_TO_FLARE_DONE,
    SSMSG_UNUSED,
    SSMSG_FLARE_DONE,           // start wipe 3
    SSMSG_FLARE_TO_LOGO_DONE
};

// flags for D3DXCreateEffectFromResource()
#define EFFECT_FLAGS            ( D3DXFX_NOT_CLONEABLE | D3DXSHADER_NO_PRESHADER )

class CSSView;

extern SSState              g_eState;
extern SSMsg                g_eMsg;
extern CSSView*             g_pView;            // the view of the device being processed
extern ID3DXMatrixStack*    g_pViewStack;
extern ID3DXMatrixStack*    g_pProjStack;
extern D3DXMATRIXA16        g_mIdent;
extern D3DXCOLOR            g_cTargetColor;     // background colour, see CSolidColorWipe


//-----------------------------------------------------------------------------
// Name: RandFloat()
// Desc: Random number in [fMin, fMax].
//-----------------------------------------------------------------------------
inline FLOAT RandFloat( FLOAT fMin, FLOAT fMax )
{
    return (FLOAT)rand() / 32767.0f * ( fMax - fMin ) + fMin;
}


//-----------------------------------------------------------------------------
// Name: ExpLerp()
// Desc: Exponential interpolation: 0 at t0, 1 at t1, with the shape of
//       fBase^x (fast start for fBase < 1, slow start for fBase > 1).
//-----------------------------------------------------------------------------
inline FLOAT ExpLerp( FLOAT t, FLOAT t0, FLOAT t1, FLOAT fBase )
{
    return ( powf( fBase, ( t - t0 ) / ( t1 - t0 ) ) - 1.0f ) / ( fBase - 1.0f );
}


// ssbranded.cpp: creates a texture from a Targa image in a resource
HRESULT MSLoadTextureFromResource( IDirect3DDevice9* pd3dDevice, LPCWSTR pszName, LPCWSTR pszType,
                                   IDirect3DTexture9** ppTexture );

#endif // _BRANDED_VARS_H
