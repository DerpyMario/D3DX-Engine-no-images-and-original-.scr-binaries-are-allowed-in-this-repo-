//============================================================================================
// engine.h -- Mystify.scr rendering engine
//
// Reconstructed from the static analysis of Mystify.scr (PE32+ x64, MSVC 14.20,
// 154,112 bytes, version 10.0.19041.4355, ImageBase 0x140000000).
//
// The engine is a Direct3D 9 fixed-function renderer sharing the D3DSaverWndClass
// framework with Ribbons.scr; the saver-specific part is the polygon model:
//
//   * the application object (HeapAlloc'ed 0xF7A00+ bytes, RVA 0x5078) holds:
//
//       +0x2938   camera tick counter
//       +0x293C   camera state (0x70 bytes, same layout as Ribbons but faster)
//       +0x2960   CameraFOV / aspect(1.3333) / zNear(0.1) / zFar(300)
//       +0x29A8   polygon[0]           } NumLines entries,
//       +0x29A8+i*0xA50                } each 0xA50 bytes
//       +0xF7E8   fullscreen-fade quad vertex pointers (10 slots)
//       +0xF910   SpanMultiMon (byte), Blur (byte at +0xF911)
//       +0xF914   NumLines (DWORD), LineWidth (float at +0xF918)
//
//   * one "line" = one camera-facing quad strip (exactly like a Ribbons ribbon but
//     narrower), whose head endpoints are driven by three independently bouncing
//     "corner" points + fractal noise; the strip trails 50 frames of history.
//
// Verified tunables (see analysis/architecture.md section 8):
//   head box (±2, ±2, ±6), corner box (±0.64, ±0.64, ±1.0),
//   corner velocity range [0.005, 0.02], camera velocity range [1e-4, 0.005].
//============================================================================================
#pragma once

#include <windows.h>
#include <d3d9.h>
#include "settings.h"

namespace Mystify {

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
// Tunables recovered from the binary
//----------------------------------------------------------------------------------------
struct EngineConstants
{
    static constexpr int   MaxLines          = 20;     // NumLines clamp (0x14 @ 0x73D9)
    static constexpr int   StripSegments     = 50;     // history slots per polygon (loop @ 0x7B66)
    static constexpr int   StripVertexCount  = 101;    // 0x990-byte vertex buffer / 24-byte FVF
    static constexpr int   StripTriangleCount= 99;     // DrawPrimitive(D3DPT_TRIANGLESTRIP, 99)

    static constexpr float LineWidthMin = 0.01f;               // 0.01f
    static constexpr float LineWidthMax = 0.1f;               // 0.1f
    static constexpr float CameraFovMin = 0.5f;               // 0.5f
    static constexpr float CameraFovMax = 3.0f;               // 3.0f
    static constexpr float HeadBoxX = 2.0f;                   // 2.0f  (0x40000000 @ 0x18B60)
    static constexpr float HeadBoxZ = 6.0f;                   // 6.0f  (0x40C00000 @ 0x7672)
    static constexpr float CornerBoxXY = 0.64f;                // 0.64f (0x3F1EB852 @ 0x18B40)
    static constexpr float CornerBoxZ = 1.0f;                 // 1.0f  (0x3F800000 @ 0x7816)
    static constexpr float CornerVelMin = 0.005f;               // 0.005f (0x3BA3D70A)
    static constexpr float CornerVelMax = 0.02f;               // 0.02f  (0x3CA3D70A)
    static constexpr float CameraVelMin = 1e-4f;               // 1e-4f  (0x38D1B717)
    static constexpr float CameraVelMax = 0.005f;               // 0.005f (0x3BA3D70A)
    static constexpr float NoiseScale = 2.5f;                 // 2.5f   (0x40200000 @ 0x18B64)
    static constexpr float ColorThreshold = 0.64f;             // 0.64f
    static constexpr float ColorScale = 32.43f;                 // 32.43f (0x42037899 @ 0x18B90)
    static constexpr float ZNear = 0.1f;                      // 0.1f
    static constexpr float ZFar = 300.0f;                       // 300.0f

    static constexpr DWORD ClearGraceFrames = 10;
    static constexpr DWORD FadeQuadColor = 0xFC010101;              // 0xFC010101
    static constexpr DWORD BlackQuadColor = 0xFF000000;             // 0xFF000000
};

//----------------------------------------------------------------------------------------
// Camera state (0x70 bytes at app+0x293C; RVA 0x2130)
//----------------------------------------------------------------------------------------
struct CameraState
{
    D3DXVECTOR3 pos;            // +0x00
    D3DXVECTOR3 prevPos;        // +0x0C
    D3DXVECTOR3 look;           // +0x34 three noise samples * scale
    D3DXVECTOR3 motionDelta;    // +0x40 prevPos - pos
    D3DXVECTOR3 vel;            // +0x4C initial (0.00255, 0.00255, 0.00255)
    float       velMin;         // +0x58 1e-4
    float       velMax;         // +0x5C 0.005
    D3DXVECTOR3 bounds;         // +0x60 (1.1, 1.1, 1.1)
};

//----------------------------------------------------------------------------------------
// One "line" (0xA50 bytes at app+0x29A8 + i*0xA50; RVA 0x7B38)
//----------------------------------------------------------------------------------------
struct PolyVertex               // D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, stride 24
{
    D3DXVECTOR3 pos;           // 12
    D3DCOLOR    color;         // 4  (corner-driven cycling color)
    float       u, v;          // 8
};

struct PolyFrame                // one 0x30-byte history slot = two strip vertices
{
    PolyVertex a;              // endpoint A at +0x00 (pos + color)
    BYTE       pad1[8];
    PolyVertex b;              // endpoint B at +0x18 (pos + color)
    BYTE       pad2[8];
};

struct Polygon
{
    float        width;        // +0x0000 = Settings::LineWidth
    D3DXVECTOR3  edgeA;        // +0x0058 current endpoint A (noisy preview of slot[0].a)
    D3DCOLOR     colorA;       // +0x0064 current endpoint A color
    D3DXVECTOR3  edgeB;        // +0x0070 current endpoint B
    D3DCOLOR     colorB;       // +0x007C current endpoint B color
    PolyFrame    frames[51];   // +0x0088..0x09E8: current + 50 history slots
    D3DXVECTOR3  pos;          // +0x09E8 quad centre (the "flying" point)
    D3DXVECTOR3  noise;        // +0x09F4 three noise samples
    D3DXVECTOR3  vel;          // +0x0A00 centre velocity
    D3DXVECTOR3  bounds;       // +0x0A0C centre box (2, 2, 6)
    float        velMin;       // +0x0A18
    float        velMax;       // +0x0A1C
    D3DXVECTOR3  corner;       // +0x0A20 corner accumulator (drives the colour)
    D3DXVECTOR3  cornerVel;    // +0x0A2C corner velocities
    D3DXVECTOR3  cornerBounds; // +0x0A38 corner box (0.64, 0.64, 1.0)
    float        cornerVelMin; // +0x0A44 0.005
    float        cornerVelMax; // +0x0A48 0.02
};

//----------------------------------------------------------------------------------------
// Engine object
//----------------------------------------------------------------------------------------
struct Engine
{
    HWND                  hwndFocus;
    IDirect3D9*           d3d;
    IDirect3DDevice9*     device;
    IDirect3DTexture9*    texture;           // star sprite ("BMP" 102)

    IDirect3DVertexBuffer9* fadeQuadVB;      // 112 bytes, D3DFVF_XYZRHW|DIFFUSE|TEX1
    IDirect3DVertexBuffer9* polyVB[EngineConstants::MaxLines]; // 0x990 bytes each

    CameraState           camera;
    Polygon               polys[EngineConstants::MaxLines];
    int                   lineCount;

    Settings              settings;
    int                   spanMultiMon;
    int                   blur;
    float                 fov, aspect, zNear, zFar;
    D3DVIEWPORT9          viewport;
    RECT                  currentRect;
    RECT                  previousRect;
    int                   clearGrace;
    DWORD                 lastTick;
};

//----------------------------------------------------------------------------------------
// Lifecycle
//----------------------------------------------------------------------------------------
Engine* EngineCreate();
void    EngineDestroy(Engine* e);

HRESULT EngineInitialize(Engine* e, HWND hwnd, const Settings* s);
void    EngineFrameMove(Engine* e);
HRESULT EngineRenderScene(Engine* e);
HRESULT EngineRestoreDevice(Engine* e);
UINT    EngineErrorToStringId(HRESULT hr);

} // namespace Mystify
