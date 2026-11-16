//-----------------------------------------------------------------------------
// File: aurora.h
//
// Desc: The Aurora screen saver.
//
//       Original objects:
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\aurora.obj
//           shell\osshell\control\scrnsave\aurora\objfre\amd64\auroraconfig.obj
//-----------------------------------------------------------------------------
#ifndef _AURORA_H
#define _AURORA_H

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "d3dsaver9.h"
#include "camera.h"
#include "arc.h"

#define AURORA_MAX_LAYERS       10

//-----------------------------------------------------------------------------
// Name: struct DeviceObjects
// Desc: Per-render-unit objects.  Both are created in RestoreDeviceObjects()
//       and released in InvalidateDeviceObjects(), but nothing ever renders
//       into them - they look like the remains of a render-to-texture pass.
//-----------------------------------------------------------------------------
struct DeviceObjects
{
    IDirect3DTexture9* pRenderTexture;
    IDirect3DSurface9* pDepthStencil;
};

//-----------------------------------------------------------------------------
// Name: class AuroraScreensaver
//-----------------------------------------------------------------------------
class AuroraScreensaver : public CD3DScreensaver
{
public:
                    AuroraScreensaver();

    virtual VOID    DoConfig();
    virtual VOID    ReadSettings();

protected:
    VOID            ValidateSettings();

    virtual VOID    SetDevice( UINT iDevice );
    virtual HRESULT ConfirmDevice( D3DCAPS9* pCaps, DWORD dwBehavior, D3DFORMAT fmtBackBuffer );
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    virtual HRESULT InvalidateDeviceObjects();

protected:
    DeviceObjects   m_DeviceObjectsArray[AURORA_MAX_DEVICES];
    DeviceObjects*  m_pDeviceObjects;       // -> m_DeviceObjectsArray[m_iDevice]
    INT             m_iDevice;              // current render unit
    DWORD           m_dwLastTick;

    Camera          m_Camera;
    ArcClass        m_Arcs[AURORA_MAX_LAYERS];

    D3DXMATRIX      m_matWorld;
    D3DXMATRIX      m_matUnused;            // never referenced

    // Settings (HKCU\Software\Microsoft\Windows\CurrentVersion\Screensavers\Aurora)
    bool            m_bSpanMultiMon;        // one picture across all monitors
    INT             m_nNumLayers;           // number of arcs, 1..10
    FLOAT           m_fBrightness;          // 0.01..1
    FLOAT           m_fSpeed;               // 0.001..0.05
    FLOAT           m_fAmplitude;           // 0..10
};

#if defined(_WIN64) && defined(UNICODE)
static_assert( sizeof(AuroraScreensaver) == 0x22dc0, "AuroraScreensaver layout" );
#endif

extern AuroraScreensaver* g_pMyScreensaver;

#endif // _AURORA_H
