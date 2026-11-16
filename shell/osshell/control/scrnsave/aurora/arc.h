//-----------------------------------------------------------------------------
// File: arc.h
//
// Desc: One "arc" of the Aurora screen saver: a ribbon of 100 quads that is
//       bent by 3D noise and drawn additively with two scrolling layers of
//       the same texture.
//
//       Original object:
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\arc.obj
//-----------------------------------------------------------------------------
#ifndef _AURORA_ARC_H
#define _AURORA_ARC_H

#include <windows.h>
#include <d3d9.h>
#include "point.h"
#include "camera.h"

// Per-device objects are kept in fixed tables that are indexed with the
// render unit number handed to CD3DScreensaver::SetDevice().
#define AURORA_MAX_DEVICES      10

#define ARC_SEGMENTS            100
#define ARC_NUM_VERTICES        ( ( ARC_SEGMENTS + 1 ) * 2 )

struct ARCVERTEX
{
    Point3f pos;
    DWORD   color;
    float   tu0, tv0;
    float   tu1, tv1;
};

// (sic) The vertex carries two sets of texture coordinates, but the shipped
// code declares three.  Stage 2 is disabled, so the third set is never used.
#define D3DFVF_ARCVERTEX        ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX3 )

class ArcClass
{
public:
    ArcClass();

    void InitDevice( IDirect3DDevice9* pd3dDevice, int iDevice, float fBrightness );
    void Update( Camera camera, float fSpeed, float fBrightness, float fAmplitude );
    void Render( IDirect3DDevice9* pd3dDevice, int iDevice );

    // Releases what InitDevice() created.  (Expanded inline in
    // AuroraScreensaver::InvalidateDeviceObjects() in the shipped image.)
    void InvalidateDevice( int iDevice )
    {
        if( m_pVB[iDevice] != NULL )
        {
            m_pVB[iDevice]->Release();
            m_pVB[iDevice] = NULL;
        }
        if( m_pTexture[iDevice] != NULL )
        {
            m_pTexture[iDevice]->Release();
            m_pTexture[iDevice] = NULL;
        }
    }

public:
    IDirect3DTexture9*      m_pTexture[AURORA_MAX_DEVICES];
    IDirect3DVertexBuffer9* m_pVB[AURORA_MAX_DEVICES];

    ARCVERTEX   m_Vertices[ARC_NUM_VERTICES];       // what gets drawn
    ARCVERTEX   m_BaseVertices[ARC_NUM_VERTICES];   // the straight, undisturbed ribbon

    Point3f     m_noisePos;             // where in the noise field the arc currently is
    int         m_nOctaves;
    bool        m_bAbsoluteNoise;
    float       m_fTexScrollSpeed[2];   // per frame, one per texture stage
    float       m_fTexScroll[2];
    Point3f     m_unused;               // never referenced
    Point3f     m_noiseVelocity;
    Point3f     m_noiseBounds;          // the noise position is kept within +/- this
    float       m_fMinNoiseSpeed;       // range of the new velocity after bouncing off
    float       m_fMaxNoiseSpeed;       //   m_noiseBounds (see the note in arc.cpp)
    float       m_fHeight;              // height of the undisturbed ribbon
    float       m_reserved[3];          // never referenced
};

#if defined(_WIN64)
static_assert( sizeof(ARCVERTEX) == 0x20,  "ARCVERTEX layout" );
static_assert( sizeof(ArcClass) == 0x3380, "ArcClass layout" );
#endif

#endif // _AURORA_ARC_H
