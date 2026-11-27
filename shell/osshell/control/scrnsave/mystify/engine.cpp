//============================================================================================
// engine.cpp -- Mystify.scr Direct3D 9 engine (see engine.h for provenance)
//
// Function mapping:
//   EngineCreate            RVA 0x4FC8  (constructor defaults)
//   EngineInitialize        RVA 0xA424 + 0x9994 (device list, per-screen init)
//   InitPolygon             RVA 0x7464  (per-line position, corners, colors)
//   LoadTexture             RVA 0x2A80 -> 0xC3C4 ("BMP"/102 -> dynamic texture)
//   EngineFrameMove         RVA 0x5370 -> 0x2130 (camera) -> 0x7B38 (polygons)
//   BuildProjection/View    RVA 0x12E7C / 0x13288 (D3DX equivalents)
//   EngineRenderScene       RVA 0x5450 -> 0x842C (strips) + 0x1DB0 (fade quads)
//   EngineErrorToStringId   RVA 0xB7E0
//============================================================================================
#include "engine.h"
#include "resources.h"
#include "noise.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cstdio>

namespace Mystify {


namespace {

float RandUnit()                    // rand() * 1/32768 (RVA 0x7C5D: mulss 0x18B08)
{
    return static_cast<float>(std::rand()) * (1.0f / 32768.0f);
}

float RandRange(float lo, float hi)
{
    return lo + (hi - lo) * RandUnit();
}

D3DXVECTOR3 Cross(const D3DXVECTOR3& a, const D3DXVECTOR3& b)
{
    return D3DXVECTOR3(a.y * b.z - a.z * b.y,
                       a.z * b.x - a.x * b.z,
                       a.x * b.y - a.y * b.x);
}

float Length(const D3DXVECTOR3& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float NoiseAt(const D3DXVECTOR3& p)         // RVA 0xCC5C (fractal sum)
{
    return D3DSaverNoise::Fractal3(p.x, p.y, p.z);
}

void BuildProjection(D3DMATRIX* out, float fovY, float aspect, float zn, float zf)
{
    const float yScale = 1.0f / std::tan(fovY * 0.5f);
    const float xScale = yScale / aspect;
    std::memset(out, 0, sizeof(*out));
    out->_11 = xScale;
    out->_22 = yScale;
    out->_33 = zf / (zn - zf);
    out->_43 = -1.0f;
    out->_34 = zn * zf / (zn - zf);
}

void BuildView(D3DMATRIX* out, const D3DXVECTOR3& eye, const D3DXVECTOR3& at,
               const D3DXVECTOR3& up)
{
    D3DXVECTOR3 zaxis = at - eye;
    float len = Length(zaxis);
    if (len > 0.0f) { zaxis.x /= len; zaxis.y /= len; zaxis.z /= len; }
    D3DXVECTOR3 xaxis = Cross(up, zaxis);
    len = Length(xaxis);
    if (len > 0.0f) { xaxis.x /= len; xaxis.y /= len; xaxis.z /= len; }
    D3DXVECTOR3 yaxis = Cross(zaxis, xaxis);

    out->_11 = xaxis.x; out->_12 = yaxis.x; out->_13 = zaxis.x; out->_14 = 0.0f;
    out->_21 = xaxis.y; out->_22 = yaxis.y; out->_23 = zaxis.y; out->_24 = 0.0f;
    out->_31 = xaxis.z; out->_32 = yaxis.z; out->_33 = zaxis.z; out->_34 = 0.0f;
    out->_41 = -(xaxis.x * eye.x + xaxis.y * eye.y + xaxis.z * eye.z);
    out->_42 = -(yaxis.x * eye.x + yaxis.y * eye.y + yaxis.z * eye.z);
    out->_43 = -(zaxis.x * eye.x + zaxis.y * eye.y + zaxis.z * eye.z);
    out->_44 = 1.0f;
}

//----------------------------------------------------------------------------------------
// "BMP" 102 loader (RVA 0xC3C4 in Mystify.scr - identical format to Ribbons)
//   +0x02 WORD version == 2, +0x0A WORD width, +0x0C WORD height, +0x0E WORD bpp,
//   then per-pixel RLE: control byte (bit7 = run, bits 0..6 = count) + 3/4 colour bytes.
//----------------------------------------------------------------------------------------
HRESULT CreateTextureFromSpriteResource(IDirect3DDevice9* dev, IDirect3DTexture9** out)
{
    HRSRC hrsc = FindResourceW(nullptr, MAKEINTRESOURCEW(IDB_TEXTURE), L"BMP");
    if (!hrsc)
        return D3DERR_NOTAVAILABLE;
    HGLOBAL hg = LoadResource(nullptr, hrsc);
    if (!hg)
        return D3DERR_NOTAVAILABLE;
    const BYTE* data = static_cast<const BYTE*>(LockResource(hg));
    const DWORD size = SizeofResource(nullptr, hrsc);
    if (!data || size < 18)
        return E_FAIL;

    // Format sniff (RVA 0xC894): byte[0] == 0 and (byte[2] - 2) & 0xF7 == 0
    // marks this custom format (type 2 or 0x0A). Type 1 would be PNG, type 3
    // would be a plain "BM" BMP file.
    if (data[0] != 0 || ((data[2] - 2) & 0xF7) != 0)
        return E_FAIL;

    const WORD width  = *reinterpret_cast<const WORD*>(data + 0x0C);  // RVA 0xC9CF
    const WORD height = *reinterpret_cast<const WORD*>(data + 0x0E);  // RVA 0xC9E3
    const BYTE bpp    = data[0x10];                                   // RVA 0xCC14

    UINT tw = 1, th = 1;
    while (tw < width)  tw <<= 1;
    while (th < height) th <<= 1;

    HRESULT hr = dev->CreateTexture(tw, th, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                    D3DPOOL_DEFAULT, out, nullptr);
    if (FAILED(hr) || !*out)
        return E_FAIL;

    D3DLOCKED_RECT lr;
    hr = (*out)->LockRect(0, &lr, nullptr, 0);
    if (FAILED(hr))
        return E_FAIL;

    // Per-pixel decoder (RVA 0xCBC4). Two control flavours:
    //   bit 7 set   -> RUN: one colour repeated 1 + (ctrl & 0x7F) times
    //   bit 7 clear -> LITERAL: 1 + (ctrl & 0x7F) pixels, each with own colour
    // Rows are stored bottom-up; alpha is 0xFF for bpp 24. A group's first
    // pixel is emitted directly after the control byte is consumed.
    const BYTE* cursor = data + 0x12;
    DWORD runRemaining = 0;
    BYTE  runFlag = 0;
    BYTE  stored[4] = { 0, 0, 0, 0xFF };
    for (UINT row = 0; row < height; ++row)
    {
        const UINT y = height - 1 - row;               // flags bit 5 = 0 -> bottom-up
        BYTE* dest = reinterpret_cast<BYTE*>(lr.pBits) + static_cast<LONG>(y) * lr.Pitch;
        for (UINT x = 0; x < width; ++x)
        {
            BYTE color[4];
            if (runRemaining > 0)
            {
                if (runFlag)
                {
                    color[0] = stored[0]; color[1] = stored[1];
                    color[2] = stored[2]; color[3] = stored[3];
                }
                else
                {
                    color[0] = *cursor++;
                    color[1] = *cursor++;
                    color[2] = *cursor++;
                    color[3] = (bpp == 24) ? 0xFF : *cursor++;
                }
                --runRemaining;
            }
            else
            {
                const BYTE ctrl = *cursor++;
                runFlag = ctrl >> 7;
                runRemaining = ctrl & 0x7F;
                color[0] = *cursor++;
                color[1] = *cursor++;
                color[2] = *cursor++;
                color[3] = (bpp == 24) ? 0xFF : *cursor++;
                stored[0] = color[0]; stored[1] = color[1];
                stored[2] = color[2]; stored[3] = color[3];
            }
            BYTE* px = dest + x * 4;
            px[0] = color[0];
            px[1] = color[1];
            px[2] = color[2];
            px[3] = color[3];
        }
    }

    (*out)->UnlockRect(0);
    return S_OK;
}

//----------------------------------------------------------------------------------------
// Per-line initialisation (RVA 0x7464)
//----------------------------------------------------------------------------------------
void InitPolygon(Engine* e, int index)
{
    Polygon& p = e->polys[index];
    p = Polygon{};
    p.width = e->settings.LineWidth;

    // Centre box (2, 2, 6) and centre velocity range (RVA 0x766A..0x767C)
    p.bounds = D3DXVECTOR3(EngineConstants::HeadBoxX, EngineConstants::HeadBoxX,
                           EngineConstants::HeadBoxZ);
    p.velMin = EngineConstants::CameraVelMin;
    p.velMax = EngineConstants::CameraVelMax;

    // Random centre + velocity (RVA 0x75B2..0x7646)
    p.pos = D3DXVECTOR3(RandRange(-p.bounds.x, p.bounds.x),
                        RandRange(-p.bounds.y, p.bounds.y),
                        RandRange(-p.bounds.z, p.bounds.z));
    p.vel = D3DXVECTOR3(RandRange(p.velMin, p.velMax),
                        RandRange(p.velMin, p.velMax),
                        RandRange(p.velMin, p.velMax));

    // Corner box (0.64, 0.64, 1.0), corner velocity range (0.005, 0.02)
    // (RVA 0x7605/0x7613/0x780E/0x7820)
    p.cornerBounds = D3DXVECTOR3(EngineConstants::CornerBoxXY,
                                 EngineConstants::CornerBoxXY,
                                 EngineConstants::CornerBoxZ);
    p.cornerVelMin = EngineConstants::CornerVelMin;
    p.cornerVelMax = EngineConstants::CornerVelMax;

    p.corner = D3DXVECTOR3(RandRange(-p.cornerBounds.x, p.cornerBounds.x),
                           RandRange(-p.cornerBounds.y, p.cornerBounds.y),
                           RandRange(-p.cornerBounds.z, p.cornerBounds.z));
    p.cornerVel = D3DXVECTOR3(RandRange(p.cornerVelMin, p.cornerVelMax),
                              RandRange(p.cornerVelMin, p.cornerVelMax),
                              RandRange(p.cornerVelMin, p.cornerVelMax));

    // Seed the current + history slots so the first frames show a straight segment.
    for (int i = 0; i < 51; ++i)
    {
        PolyFrame& f = p.frames[i];
        f.a.pos = p.pos - p.vel * static_cast<float>(i);
        f.b.pos = p.pos - p.vel * static_cast<float>(i);
        f.a.color = 0xFFFFFFFF;
        f.b.color = 0xFFFFFFFF;
        f.a.u = 0.0f; f.a.v = 0.0f;
        f.b.u = 1.0f; f.b.v = 1.0f;
    }
}

//----------------------------------------------------------------------------------------
// Camera tick (RVA 0x2130) - same model as Ribbons with Mystify's velocity range.
//----------------------------------------------------------------------------------------
void CameraFrameMove(CameraState& cam)
{
    cam.prevPos = cam.pos;
    cam.pos = cam.pos + cam.vel;
    cam.motionDelta = cam.prevPos - cam.pos;

    auto randomize = [&](float* velComp, float posComp, float bound) {
        if (*velComp > 0.0f && posComp > bound)
            *velComp = -RandRange(cam.velMin, cam.velMax);
        else if (*velComp < 0.0f && -bound > posComp)
            *velComp = RandRange(cam.velMin, cam.velMax);
    };
    randomize(&cam.vel.x, cam.pos.x, cam.bounds.x);
    randomize(&cam.vel.y, cam.pos.y, cam.bounds.y);
    randomize(&cam.vel.z, cam.pos.z, cam.bounds.z);

    // Noise offsets (RVA 0x2130-equivalent; Mystify .rdata 0x18B78=10.0, 0x18B7C=10.2,
    // 0x18BA0=255.0 / 0x18B30=0.2, 0x18B9C=200.0, 0x18B80=-11.0 subtraction)
    D3DXVECTOR3 o1(10.0f, 10.2f, 255.0f);
    D3DXVECTOR3 o2(0.2f, 200.0f, -11.0f);
    cam.look.x = NoiseAt(cam.pos);
    cam.look.y = NoiseAt(cam.pos + o1);
    cam.look.z = NoiseAt(cam.pos + o2);
}

//----------------------------------------------------------------------------------------
// Colour byte for one channel (RVA 0x835C..0x83C7):
//   v <= 0            -> 0x00
//   v >= 0.64         -> 0xFF
//   otherwise         -> v * 32.43
//----------------------------------------------------------------------------------------
inline BYTE ColorByte(float v)
{
    if (v <= 0.0f)
        return 0x00;
    if (v >= EngineConstants::ColorThreshold)
        return 0xFF;
    return static_cast<BYTE>(v * EngineConstants::ColorScale);
}

//----------------------------------------------------------------------------------------
// Polygon tick (RVA 0x7B38)
//----------------------------------------------------------------------------------------
void PolygonFrameMove(Engine* e, Polygon& p, const CameraState& cam)
{
    // 1. History shift (RVA 0x7B99..0x7BC7; slots carry pos+color; the oldest
    //    slot falls off and slot 0 receives the previous current frame).
    //    The previous blue channel feeds this frame's alpha (cycling glow).
    const BYTE prevBlue = static_cast<BYTE>((p.frames[0].a.color >> 24) & 0xFF);
    for (int i = 50; i >= 1; --i)
        p.frames[i] = p.frames[i - 1];

    // 2. Centre advance + wall bounce (RVA 0x7BD0..0x7E2B)
    p.pos = p.pos + p.vel;
    auto randomize = [&](float* velComp, float posComp, float bound) {
        if (*velComp > 0.0f && posComp > bound)
            *velComp = -RandRange(p.velMin, p.velMax);
        else if (*velComp < 0.0f && -bound > posComp)
            *velComp = RandRange(p.velMin, p.velMax);
    };
    randomize(&p.vel.x, p.pos.x, p.bounds.x);
    randomize(&p.vel.y, p.pos.y, p.bounds.y);
    randomize(&p.vel.z, p.pos.z, p.bounds.z);

    // 3. Three noise samples (RVA 0x7E2F..0x7EBE)
    D3DXVECTOR3 o1(10.0f, 10.2f, 255.0f);
    D3DXVECTOR3 o2(0.2f, 200.0f, -11.0f);
    D3DXVECTOR3 n(NoiseAt(p.pos) * EngineConstants::NoiseScale,
                  NoiseAt(p.pos + o1) * EngineConstants::NoiseScale,
                  NoiseAt(p.pos + o2) * EngineConstants::NoiseScale);

    // 4. Corner advance + bounce, with position snap on hit (RVA 0x7EC3..0x8178:
    //    on a boundary hit the corner coordinate is reset to +bound or -bound).
    p.corner = p.corner + p.cornerVel;
    auto bounce = [&](float* posComp, float* velComp, float bound) {
        if (*velComp > 0.0f && *posComp > bound)
        {
            *posComp = bound;
            *velComp = -RandRange(p.cornerVelMin, p.cornerVelMax);
        }
        else if (*velComp < 0.0f && -bound > *posComp)
        {
            *posComp = -bound;
            *velComp = RandRange(p.cornerVelMin, p.cornerVelMax);
        }
    };
    bounce(&p.corner.x, &p.cornerVel.x, p.cornerBounds.x);
    bounce(&p.corner.y, &p.cornerVel.y, p.cornerBounds.y);
    bounce(&p.corner.z, &p.cornerVel.z, p.cornerBounds.z);

    // 5. Edge direction: noise minus (corner / 6 + 1/6) terms, crossed with the
    //    camera motion delta, normalised (RVA 0x81B1..0x82CD).
    const float sixth = 0.166666f;
    D3DXVECTOR3 w(p.corner.x / 6.0f + sixth,
                  p.corner.y / 6.0f,
                  p.corner.z / 6.0f + sixth);
    D3DXVECTOR3 edge = Cross(n - w, cam.motionDelta);
    float len = Length(edge);
    if (len > 0.0f) { edge.x /= len; edge.y /= len; edge.z /= len; }

    // 6. Endpoints (RVA 0x82D4..0x8359): A = noise - edge*width, B = edge*width + noise
    PolyFrame& f = p.frames[0];
    f.a.pos = n - edge * p.width;
    f.b.pos = n + edge * p.width;
    f.a.u = 0.0f; f.a.v = 0.0f;
    f.b.u = 1.0f; f.b.v = 1.0f;

    // 7. Colour from the corner accumulators (RVA 0x835C..0x8421). Byte layout of
    //    the D3DCOLOR written to both endpoints:
    //      alpha = clamp(corner.z/6 + 1/6)      [byte 0x67 -> D3DCOLOR byte 0]
    //      red   = clamp(corner.y/6)            [byte 0x68 -> D3DCOLOR byte 1]
    //      green = clamp(corner.x/6 + 1/6)      [byte 0x69 -> D3DCOLOR byte 2]
    //      blue  = previous frame's blue byte   [byte 0x6a <- [rbx+0x67]]
    const BYTE bG = ColorByte(p.corner.x / 6.0f + sixth);
    const BYTE bR = ColorByte(p.corner.y / 6.0f);
    const BYTE bB = ColorByte(p.corner.z / 6.0f + sixth);
    f.a.color = f.b.color = D3DCOLOR_ARGB(bB, bR, bG, prevBlue);
}

//----------------------------------------------------------------------------------------
// Fullscreen fade quad (RVA 0x1DB0; D3DFVF_XYZRHW | DIFFUSE | TEX1, triangle fan)
//----------------------------------------------------------------------------------------
struct FadeQuadVertex
{
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};

void DrawFullscreenQuad(IDirect3DDevice9* dev, IDirect3DVertexBuffer9* vb,
                        D3DCOLOR color, DWORD shadeMode, int textureSlot)
{
    FadeQuadVertex* v = nullptr;
    if (FAILED(vb->Lock(0, 0, reinterpret_cast<void**>(&v), 0)) || !v)
        return;
    v[0] = { 0.0f, 0.0f, 0.0f, 1.0f, color, 0.0f, 0.0f };
    v[1] = { 0.0f, 1.0f, 0.0f, 1.0f, color, 0.0f, 1.0f };
    v[2] = { 1.0f, 1.0f, 0.0f, 1.0f, color, 1.0f, 1.0f };
    v[3] = { 1.0f, 0.0f, 0.0f, 1.0f, color, 1.0f, 0.0f };
    vb->Unlock();

    dev->SetRenderState(D3DRS_SHADEMODE, shadeMode);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    dev->SetTexture(textureSlot, nullptr);
    dev->SetVertexShader(nullptr);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    dev->SetStreamSource(0, vb, 0, sizeof(FadeQuadVertex));
    dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
}

} // namespace

Engine* EngineCreate()
{
    Engine* e = new Engine{};
    e->clearGrace = EngineConstants::ClearGraceFrames;
    return e;
}

void EngineDestroy(Engine* e)
{
    if (!e)
        return;
    for (int i = 0; i < EngineConstants::MaxLines; ++i)
        if (e->polyVB[i]) { e->polyVB[i]->Release(); e->polyVB[i] = nullptr; }
    if (e->fadeQuadVB) { e->fadeQuadVB->Release(); e->fadeQuadVB = nullptr; }
    if (e->texture)    { e->texture->Release();    e->texture = nullptr; }
    if (e->device)     { e->device->Release();     e->device = nullptr; }
    if (e->d3d)        { e->d3d->Release();        e->d3d = nullptr; }
    delete e;
}

HRESULT EngineInitialize(Engine* e, HWND hwnd, const Settings* s)
{
    if (!e || !hwnd || !s)
        return E_POINTER;

    e->hwndFocus = hwnd;
    e->settings = *s;
    e->blur = s->Blur ? 1 : 0;
    e->spanMultiMon = s->SpanMultiMon ? 1 : 0;
    e->fov = s->CameraFOV;
    e->zNear = EngineConstants::ZNear;
    e->zFar = EngineConstants::ZFar;

    e->d3d = Direct3DCreate9(32);                    // RVA 0x8B2A: Direct3DCreate9(0x20)
    if (!e->d3d)
        return MAKE_D3DHRESULT(2001);

    RECT rc;
    GetClientRect(hwnd, &rc);
    e->currentRect = rc;
    e->previousRect = rc;
    e->viewport.X = 0;
    e->viewport.Y = 0;
    e->viewport.Width = rc.right - rc.left;
    e->viewport.Height = rc.bottom - rc.top;
    e->viewport.MinZ = 0.0f;
    e->viewport.MaxZ = 1.0f;
    e->aspect = static_cast<float>(e->viewport.Width) /
                static_cast<float>(e->viewport.Height ? e->viewport.Height : 1);

    D3DPRESENT_PARAMETERS pp;
    std::memset(&pp, 0, sizeof(pp));
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = e->viewport.Width;
    pp.BackBufferHeight = e->viewport.Height;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    DWORD vproc = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    D3DCAPS9 caps;
    if (SUCCEEDED(e->d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
        (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT))
        vproc = D3DCREATE_HARDWARE_VERTEXPROCESSING;

    HRESULT hr = e->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                      vproc, &pp, &e->device);
    if (FAILED(hr) || !e->device)
        return hr;

    hr = CreateTextureFromSpriteResource(e->device, &e->texture);
    if (FAILED(hr))
        return MAKE_D3DHRESULT(2007);

    hr = e->device->CreateVertexBuffer(sizeof(FadeQuadVertex) * 4, D3DUSAGE_WRITEONLY,
                                       D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                                       D3DPOOL_MANAGED, &e->fadeQuadVB, nullptr);
    if (FAILED(hr))
        return hr;

    e->lineCount = e->settings.NumLines;
    for (int i = 0; i < e->lineCount; ++i)
    {
        InitPolygon(e, i);
        hr = e->device->CreateVertexBuffer(0x990, D3DUSAGE_WRITEONLY,
                                           D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                                           D3DPOOL_MANAGED, &e->polyVB[i], nullptr);
        if (FAILED(hr))
            return hr;
    }

    // Camera defaults (RVA 0x5165..0x523F): vel (0.00255, 0.00255, 0.00255),
    // velMin 1e-4, velMax 0.005, bounds (1.1, 1.1, 1.1).
    e->camera.pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    e->camera.prevPos = e->camera.pos;
    e->camera.vel = D3DXVECTOR3(0.00255f, 0.00255f, 0.00255f);
    e->camera.velMin = EngineConstants::CameraVelMin;
    e->camera.velMax = EngineConstants::CameraVelMax;
    e->camera.bounds = D3DXVECTOR3(1.1f, 1.1f, 1.1f);

    D3DSaverNoise::Initialize(static_cast<unsigned>(std::time(nullptr)));
    return S_OK;
}

void EngineFrameMove(Engine* e)
{
    if (!e)
        return;
    e->lastTick = GetTickCount();

    CameraFrameMove(e->camera);
    for (int i = 0; i < e->lineCount; ++i)
        PolygonFrameMove(e, e->polys[i], e->camera);
}

HRESULT EngineRenderScene(Engine* e)
{
    if (!e || !e->device)
        return E_POINTER;
    IDirect3DDevice9* dev = e->device;

    dev->SetViewport(&e->viewport);

    D3DMATRIX proj;
    BuildProjection(&proj, e->fov, e->aspect, e->zNear, e->zFar);
    dev->SetTransform(D3DTS_PROJECTION, &proj);

    const bool clear = (e->blur == 0) || (e->clearGrace > 0);
    if (e->clearGrace > 0)
        --e->clearGrace;
    if (clear)
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0);

    dev->BeginScene();

    if (e->blur)
        DrawFullscreenQuad(dev, e->fadeQuadVB, EngineConstants::FadeQuadColor, 1, 0);

    const D3DXVECTOR3 at(e->camera.pos + e->camera.look * 0.1f);
    D3DMATRIX view;
    BuildView(&view, e->camera.pos, at, D3DXVECTOR3(0.0f, 1.0f, 0.0f));
    dev->SetTransform(D3DTS_VIEW, &view);

    for (int i = 0; i < e->lineCount; ++i)
    {
        IDirect3DVertexBuffer9* vb = e->polyVB[i];
        if (!vb)
            continue;

        PolyVertex* v = nullptr;
        if (FAILED(vb->Lock(0, 0, reinterpret_cast<void**>(&v), 0)) || !v)
            continue;
        const Polygon& p = e->polys[i];
        int n = 0;
        for (int s = 0; s < 51 && n < 101; ++s)
        {
            v[n++] = p.frames[s].a;
            if (n < 101)
                v[n++] = p.frames[s].b;
        }
        vb->Unlock();

        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        dev->SetTexture(0, e->texture);
        dev->SetRenderState(D3DRS_TEXTUREFACTOR, p.frames[0].a.color);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetVertexShader(nullptr);
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        dev->SetStreamSource(0, vb, 0, sizeof(PolyVertex));
        dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, EngineConstants::StripTriangleCount);
    }

    dev->EndScene();
    return S_OK;
}

HRESULT EngineRestoreDevice(Engine* e)
{
    if (!e || !e->device)
        return E_FAIL;
    HRESULT hr = e->device->TestCooperativeLevel();
    if (hr == D3DERR_DEVICENOTRESET)
    {
        D3DPRESENT_PARAMETERS pp;
        std::memset(&pp, 0, sizeof(pp));
        RECT rc; GetClientRect(e->hwndFocus, &rc);
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth = rc.right - rc.left;
        pp.BackBufferHeight = rc.bottom - rc.top;
        pp.BackBufferFormat = D3DFMT_A8R8G8B8;
        pp.EnableAutoDepthStencil = TRUE;
        pp.AutoDepthStencilFormat = D3DFMT_D24S8;
        hr = e->device->Reset(&pp);
        if (SUCCEEDED(hr))
        {
            hr = CreateTextureFromSpriteResource(e->device, &e->texture);
            e->clearGrace = EngineConstants::ClearGraceFrames;
        }
    }
    return hr;
}

UINT EngineErrorToStringId(HRESULT hr)
{
    switch (hr)
    {
    case (HRESULT)0x8200000EULL: return IDS_ERR_COULD_NOT_START;
    case (HRESULT)0x82000006ULL: return IDS_ERR_FUNC_UNAVAILABLE_HW;
    case (HRESULT)0x8200000DULL: return IDS_ERR_FUNC_UNAVAILABLE_HW;
    case (HRESULT)0x8200000BULL: return IDS_ERR_NO_TEXTURES;
    case (HRESULT)0x82000003ULL: return IDS_ERR_NEWER_CARD;
    case (HRESULT)0x82000001ULL: return IDS_ERR_NO_DIRECT3D;
    case (HRESULT)0x82000009ULL: return IDS_ERR_TRY_LOWER_RES;
    case (HRESULT)0x82000005ULL: return IDS_ERR_NO_HW_ACCEL;
    case (HRESULT)0x8200000FULL: return IDS_NO_PREVIEW;
    case (HRESULT)0x82000007ULL: return IDS_ERR_GENERIC_START;
    case (HRESULT)0x8200000CULL: return IDS_ERR_DESKTOP_RES;
    case D3DERR_MOREDATA: return IDS_ERR_NO_VIDEO_MEMORY;
    case E_FAIL:          return IDS_ERR_UNSPECIFIED;
    case E_OUTOFMEMORY:   return IDS_ERR_NO_SYS_MEMORY;
    default:              return IDS_ERR_UNSPECIFIED;
    }
}

} // namespace Mystify
