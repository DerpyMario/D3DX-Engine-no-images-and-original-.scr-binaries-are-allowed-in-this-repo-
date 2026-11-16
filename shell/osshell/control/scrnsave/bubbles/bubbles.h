//-----------------------------------------------------------------------------
// File: bubbles.h
//
// Desc: The Bubbles screen saver.
//
//       Original objects:
//           shell\osshell\control\scrnsave\bubbles\objfre\amd64\bubbles.obj
//           shell\osshell\control\scrnsave\bubbles\objfre\amd64\bubblesconfig.obj
//-----------------------------------------------------------------------------
#ifndef _BUBBLES_H
#define _BUBBLES_H

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "d3dsaver9.h"
#include "point.h"
#include "particle.h"

#define BUBBLES_MAX_DEVICES     10
#define BUBBLES_MAX_SPHERES     5000

//-----------------------------------------------------------------------------
// Name: struct DeviceObjects
// Desc: Per-render-unit objects
//-----------------------------------------------------------------------------
struct DeviceObjects
{
    IDirect3DTexture9* pShadowTexture;
    IDirect3DTexture9* pSolidTexture;
    IDirect3DTexture9* pHighlightTexture;
    IDirect3DTexture9* pGlassTexture;
    IDirect3DTexture9* pDesktopTexture;     // what was on this monitor when the saver started

    // Snapshot of the monitor, taken by FullModeHack() before any saver window
    // exists: an 18-byte Targa header followed by 32-bit pixels, so that the
    // Targa loader can turn it into pDesktopTexture.
    BYTE*              pDesktopImage;
    INT                nDesktopWidth;
    INT                nDesktopHeight;
};

//-----------------------------------------------------------------------------
// Name: struct Camera
// Desc: Same members as the Aurora camera (minus the view direction).  Only
//       the view and projection matrices are built from it; the bubbles
//       themselves are drawn as pre-transformed screen-space quads.
//-----------------------------------------------------------------------------
struct Camera
{
    Point3f eye;
    Point3f lookAt;
    Point3f up;
    FLOAT   fFov;
    FLOAT   fAspect;
    FLOAT   fNear;
    FLOAT   fFar;
};

//-----------------------------------------------------------------------------
// Name: struct Sphere
// Desc: One bubble: a 2D Verlet particle (positions are in pixels of the
//       total render area) plus what is needed to draw it.
//-----------------------------------------------------------------------------
struct Sphere : public Particle2D
{
    INT   nIndex;           // index into BubblesScreensaver::m_Spheres
    DWORD dwUnused;
    DWORD dwColor;          // changes with the direction of the turbulence
    bool  bCollides;        // set once the new sphere no longer overlaps another one
};

//-----------------------------------------------------------------------------
// Name: struct SPHEREVERTEX
// Desc: Vertex of the screen-space quads everything is drawn with
//-----------------------------------------------------------------------------
struct SPHEREVERTEX
{
    FLOAT x, y, z, rhw;
    DWORD color;
    FLOAT tu, tv;
};

#define D3DFVF_SPHEREVERTEX     ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 )

//-----------------------------------------------------------------------------
// Name: class BubblesScreensaver
//-----------------------------------------------------------------------------
class BubblesScreensaver : public CD3DScreensaver
{
public:
                    BubblesScreensaver();
                    ~BubblesScreensaver()   { delete m_pGrid; }

    virtual VOID    DoConfig();
    virtual VOID    ReadSettings();
    VOID            ValidateSettings();

    virtual VOID    SetDevice( UINT iDevice );
    virtual HRESULT OneTimeSceneInit();
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual HRESULT InvalidateDeviceObjects();
    virtual VOID    FullModeHack();

    VOID            SetPerFrameStates();

public:
    DeviceObjects   m_DeviceObjectsArray[BUBBLES_MAX_DEVICES];
    DeviceObjects*  m_pDeviceObjects;       // -> m_DeviceObjectsArray[m_iDevice]
    INT             m_iDevice;              // current render unit
    DWORD           m_dwLastTick;

    Camera          m_Camera;
    Sphere          m_Spheres[BUBBLES_MAX_SPHERES];
    Grid2D*         m_pGrid;                // buckets the spheres for collision detection

    D3DXMATRIX      m_matWorld;
    D3DXMATRIX      m_matView;

    // Settings (HKCU\Software\Microsoft\Windows\CurrentVersion\Screensavers\Bubbles)
    bool            m_bSpanMultiMon;        // one simulation across all monitors
    bool            m_bShowBubbles;         // "ShowBubbles": draw the desktop behind the spheres
    bool            m_bShowShadows;
    bool            m_bMaterialGlass;       // glass (true) or solid (false) spheres
    FLOAT           m_fSphereDensity;       // fraction of the screen that is covered, 0.001..1
    INT             m_nNumSpheres;          // spheres currently in play
    INT             m_nNumSpheresTarget;    // spheres there will be once all have been released
    FLOAT           m_fRadius;              // pixels, 20..200
    INT             m_nTurbulenceNumOctaves;// 1..4
    FLOAT           m_fTurbulenceForce;     // 0..2000
    FLOAT           m_fTurbulenceSpeed;     // 0.005..0.1
};

#if defined(_WIN64) && defined(UNICODE)
static_assert( sizeof(DeviceObjects) == 0x38,           "DeviceObjects layout" );
static_assert( sizeof(Camera) == 0x34,                  "Camera layout" );
static_assert( sizeof(Sphere) == 0x48,                  "Sphere layout" );
static_assert( sizeof(SPHEREVERTEX) == 0x1c,            "SPHEREVERTEX layout" );
static_assert( sizeof(BubblesScreensaver) == 0x5aa98,   "BubblesScreensaver layout" );
#endif

extern BubblesScreensaver* g_pMyScreensaver;

#endif // _BUBBLES_H
