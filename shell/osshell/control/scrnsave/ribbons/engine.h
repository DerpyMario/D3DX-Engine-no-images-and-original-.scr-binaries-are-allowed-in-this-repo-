//============================================================================================
// engine.h -- Ribbons.scr rendering engine
//
// Reconstructed from the static analysis of Ribbons.scr (PE32+ x64, MSVC 14.20,
// 155,136 bytes, version 10.0.19041.4355, ImageBase 0x140000000).
//
// The engine is a Direct3D 9 fixed-function renderer:
//
//   * one IDirect3D9 factory (Direct3DCreate9(32), RVA 0x8724)
//   * one IDirect3DDevice9 per monitor (or a single spanned device when
//     SpanMultiMon is set); devices are created during "BuildDeviceList"
//     (vtable slot 15, RVA 0x67E0 / 0x9F64) with multisample probing
//   * the animation state of the saver lives in the application object
//     (HeapAlloc'ed 0xA16A0 bytes, RVA 0x5CF2):
//
//       +0x0038   title/error text buffer (LoadStringW(IDS_APP_NAME))
//       +0x0058   ribbon[0]           } NumRibbons entries,
//       +0x2A58   ribbon[i] = +0x2A58 + i*0xA28  } each 0xA28 bytes
//       +0x2468   current monitor rect (preview / span)
//       +0x2478   per-frame viewport rect
//       +0x2458   IDirect3D9*
//       +0x2408   per-screen engine objects (0x2BB8 bytes each)
//       +0x263C   registry path (wide string)
//       +0x29D8   {deviceA, deviceB} pair swapped every frame
//       +0x29E8   camera state (0x70 bytes, see CameraState below)
//       +0x2A00C  CameraFOV, +0x2A010 aspect, +0x2A014 zNear(0.1), +0x2A018 zFar(300)
//       +0xA1568  fullscreen-fade quad vertex pointers (2 slots, ping-pong)
//       +0xA1690  SpanMultiMon (byte), Blur (byte)
//       +0xA1694  NumRibbons (DWORD), RibbonWidth (float at +0xA1698)
//
// The two public entry points are EngineFrameMove() (RVA 0x6170 -> 0x2120 -> 0x29D8)
// and EngineRenderScene() (RVA 0x6250 -> 0x2F04 / 0x1D9C), driven by the run loop at
// RVA 0x9010 (PeekMessage + Sleep(10) model, see analysis/architecture.md).
//============================================================================================
#pragma once

#include <windows.h>
#include <d3d9.h>
#include "settings.h"

namespace Ribbons {

//----------------------------------------------------------------------------------------
// Minimal D3DXVECTOR3-compatible type. The original binary statically links the D3DX
// maths library; the reconstruction implements the handful of operations it actually
// uses so the build has no D3DX dependency.
//----------------------------------------------------------------------------------------
struct Vec3
{
    float x, y, z;
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s)       const { return Vec3(x * s, y * s, z * s); }
};

using D3DXVECTOR3 = Vec3;

//----------------------------------------------------------------------------------------
// Tunables recovered from the binary (all offsets are into the original Ribbons.scr)
//----------------------------------------------------------------------------------------
struct EngineConstants
{
    static constexpr int   MaxRibbons        = 250;    // NumRibbons clamp (0xFA @ 0x825A)
    static constexpr int   RibbonSegments    = 50;     // history slots per ribbon (loop @ 0x2A06)
    static constexpr int   StripVertexCount  = 101;    // 0x990-byte vertex buffer / 24-byte FVF
    static constexpr int   StripTriangleCount= 99;     // DrawPrimitive(D3DPT_TRIANGLESTRIP, 99)

    static constexpr float RibbonWidthMin = 0.01f;             // 0.01f (0x3C23D70A)
    static constexpr float RibbonWidthMax = 2.5f;             // 2.5f  (0x40200000)
    static constexpr float CameraFovMin = 0.5f;               // 0.5f  (0x3F000000)
    static constexpr float CameraFovMax = 3.0f;               // 3.0f  (0x40400000)
    static constexpr float CameraBoxExtent = 1.1f;            // 1.1f  (0x3F8CCCCD @ 0x2A48..)
    static constexpr float CameraVelMin = 6.4e-5f;               // 6.4e-5f (0x3851B717)
    static constexpr float CameraVelMax = 1.03e-4f;               // 1.03e-4f (0x38D1B717)
    static constexpr float NoiseScale = 10.2f;                 // 10.2f (0x41233333 @ 0x18B7C)
    static constexpr float ZNear = 0.1f;                      // 0.1f
    static constexpr float ZFar = 300.0f;                       // 300.0f (0x43960000)

    static constexpr DWORD ClearGraceFrames = 10;      // [0x1C2C0] initial .data value
    static constexpr DWORD FadeQuadColor = 0xFC010101;              // 0xFC010101 - "Apply Blur and Fade" quad
    static constexpr DWORD BlackQuadColor = 0xFF000000;             // 0xFF000000 - opaque repaint quad
};

//----------------------------------------------------------------------------------------
// Camera state (0x70 bytes at app+0x29E8; reconstructed from RVA 0x2120)
//----------------------------------------------------------------------------------------
struct CameraState
{
    D3DXVECTOR3 pos;            // +0x00 camera position, starts at (0, 0, -0.9)
    D3DXVECTOR3 prevPos;        // +0x0C position of the previous frame
    D3DXVECTOR3 look;           // +0x34 wobble-augmented look direction (3 noise samples * 10.2)
    D3DXVECTOR3 motionDelta;    // +0x40 prevPos - pos, drives ribbon edge orientation
    D3DXVECTOR3 vel;            // +0x4C velocity, randomized on wall hits
    float       velMin;         // +0x58 6.4e-5
    float       velMax;         // +0x5C 1.03e-4
    D3DXVECTOR3 bounds;         // +0x60 per-axis half-extents (1.1, 1.1, 1.1)
};

//----------------------------------------------------------------------------------------
// One ribbon (0xA28 bytes at app+0x2A58 + i*0xA28; reconstructed from RVA 0x29D8)
//----------------------------------------------------------------------------------------
struct RibbonVertex            // D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, stride 24
{
    D3DXVECTOR3 pos;           // 12
    D3DCOLOR    color;         // 4  (per-ribbon pastel tint; alpha fades along the trail)
    float       u, v;          // 8
};

struct RibbonFrame             // one 0x30-byte history slot = two strip vertices
{
    RibbonVertex a;            // endpoint A (offset 0x00)
    BYTE         pad1[8];      // 0x18..0x1F unused
    RibbonVertex b;            // endpoint B (offset 0x18)
    BYTE         pad2[8];      // 0x28..0x2F unused
};

struct Ribbon
{
    float          width;      // +0x0000 = Settings::RibbonWidth
    RibbonFrame    frames[51]; // +0x0058..0x09E8: current slot + 50 history slots
    D3DXVECTOR3    pos;        // +0x09E8 head position (also 0x9EC / 0x9F0)
    D3DXVECTOR3    noise;      // +0x09F4 three noise samples of this frame
    D3DXVECTOR3    vel;        // +0x0A00 head velocity
    D3DXVECTOR3    bounds;     // +0x0A0C per-axis half-extents of the head box
    float          velMin;     // +0x0A18
    float          velMax;     // +0x0A1C
    D3DCOLOR       color;      // +0x0A20 pastel tint derived from normalized random triple
};

//----------------------------------------------------------------------------------------
// Engine object
//----------------------------------------------------------------------------------------
struct Engine
{
    HWND                  hwndFocus;
    HMONITOR              monitors[16];
    int                   monitorCount;

    IDirect3D9*           d3d;
    IDirect3DDevice9*     device;            // current screen's device (app+0x2460)
    IDirect3DTexture9*    texture;           // star sprite (resource "BMP" 102)

    IDirect3DVertexBuffer9* fadeQuadVB;      // 112 bytes, D3DFVF_XYZRHW|DIFFUSE|TEX1
    IDirect3DVertexBuffer9* ribbonVB[EngineConstants::MaxRibbons]; // 0x990 bytes each, per-ribbon strip

    CameraState           camera;
    Ribbon                ribbons[EngineConstants::MaxRibbons];
    int                   ribbonCount;

    Settings              settings;
    int                   spanMultiMon;      // 1 = single device over the virtual screen
    int                   blur;              // "Apply Blur and Fade"
    float                 fov, aspect, zNear, zFar;
    D3DVIEWPORT9          viewport;
    RECT                  currentRect;       // viewport rect in screen coordinates
    RECT                  previousRect;      // previous frame's rect (repaint quad)
    int                   clearGrace;        // counts down ClearGraceFrames
    DWORD                 lastTick;
};

//----------------------------------------------------------------------------------------
// Lifecycle (mirrors the original call flow)
//----------------------------------------------------------------------------------------
Engine* EngineCreate();
void    EngineDestroy(Engine* e);

// Enumerate adapters, probe multisample types, create one device per screen.
// RVA 0x9F64 (BuildDeviceList) + 0x94D4 (InitDeviceObjects per screen).
HRESULT EngineInitialize(Engine* e, HWND hwnd, const Settings* s);

// One animation step. RVA 0x6170: camera tick + per-ribbon head advance.
void    EngineFrameMove(Engine* e);

// Render every screen. RVA 0x6250: viewport, projection, Clear/blur quad,
// per-ribbon DrawPrimitive(TRIANGLESTRIP, 99), Present per device.
HRESULT EngineRenderScene(Engine* e);

// Device-lost handling (RVA 0xACxx path): TestCooperativeLevel / Reset / restore.
HRESULT EngineRestoreDevice(Engine* e);

// HRESULT -> LoadStringW id (reconstruction of vtable slot 8, RVA 0xB320).
UINT    EngineErrorToStringId(HRESULT hr);

} // namespace Ribbons
