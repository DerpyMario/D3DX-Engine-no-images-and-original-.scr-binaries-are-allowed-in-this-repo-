//============================================================================================
// engine.cpp -- Ribbons.scr Direct3D 9 engine (see engine.h for the layout provenance)
//
// Everything below follows the original binary function by function:
//
//   EngineCreate            RVA 0x5DC4  (constructor defaults)
//   EngineInitialize        RVA 0x9F64 + 0x94D4 + 0xAE9C (device list, per-screen init)
//   InitRibbon              RVA 0x6B7C.. (per-ribbon color + initial history fill 0x26F8)
//   LoadTexture             RVA 0x26F8 -> 0xBEE0 ("BMP"/102 -> D3DUSAGE_DYNAMIC texture)
//   EngineFrameMove         RVA 0x6170 -> 0x2120 (camera) -> 0x29D8 (ribbons)
//   BuildProjection         RVA 0x12BDC (D3DXMatrixPerspectiveFovLH equivalent)
//   BuildView               RVA 0x129BC (D3DXMatrixLookAtLH equivalent)
//   EngineRenderScene       RVA 0x6250 -> 0x2F04 (ribbon strips) + 0x1D9C (fade quads)
//   EngineErrorToStringId   RVA 0xB320
//============================================================================================
#include "engine.h"
#include "resources.h"
#include "noise.h"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cstdio>

namespace Ribbons {


namespace {

//----------------------------------------------------------------------------------------
// helpers
//----------------------------------------------------------------------------------------
float RandUnit()                    // rand() / 32768  (RVA 0x2A6B: mulss 0x18B18)
{
    return static_cast<float>(std::rand()) * (1.0f / 32768.0f);
}

float RandRange(float lo, float hi) // RVA 0x2ABE: lo + rand()*(hi-lo)/32768
{
    return lo + (hi - lo) * RandUnit();
}

D3DXVECTOR3 Cross(const D3DXVECTOR3& a, const D3DXVECTOR3& b)
{
    return D3DXVECTOR3(a.y * b.z - a.z * b.y,
                       a.z * b.x - a.x * b.z,
                       a.x * b.y - a.y * b.x);
}

float Length(const D3DXVECTOR3& v)  // RVA 0x165FE: sqrt intrinsic
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// RVA 0xC79C: Fractal3 evaluated at (p * 1) + (p * 2) + (p * 4) with amplitudes 1, 1/2, 1/4
float NoiseAt(const D3DXVECTOR3& p)
{
    return D3DSaverNoise::Fractal3(p.x, p.y, p.z);
}

//----------------------------------------------------------------------------------------
// D3DXMatrixPerspectiveFovLH equivalent (RVA 0x12BDC - same semantics as the D3DX call)
//----------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------
// D3DXMatrixLookAtLH equivalent (RVA 0x129BC)
//----------------------------------------------------------------------------------------
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
    // Dot products (RVA 0x129BC: D3DXMatrixLookAtLH translation row)
    out->_41 = -(xaxis.x * eye.x + xaxis.y * eye.y + xaxis.z * eye.z);
    out->_42 = -(yaxis.x * eye.x + yaxis.y * eye.y + yaxis.z * eye.z);
    out->_43 = -(zaxis.x * eye.x + zaxis.y * eye.y + zaxis.z * eye.z);
    out->_44 = 1.0f;
}

//----------------------------------------------------------------------------------------
// "BMP" 102 loader -- RVA 0xBEE0. The resource is a 18-byte custom header followed by a
// per-pixel RLE stream (see analysis/texture_rle_format.md for the format):
//
//   +0x02 DWORD version == 2                       (sub_C894)
//   +0x0C WORD  width, +0x0E WORD height           (sub_C904)
//   +0x10 WORD  bpp (24 in this resource)          (checked == 0x18 for 3-byte runs)
//   +0x12 ...  stream of control bytes:
//        bit7 = "run continues", bits 0..6 = run length, then 3 (or 4) colour bytes
//
// The decoder creates a D3DUSAGE_DYNAMIC / D3DFMT_A8R8G8B8 texture in D3DPOOL_DEFAULT,
// locks level 0 and fills it pixel by pixel (RVA 0xCB C4 per pixel).
//----------------------------------------------------------------------------------------
HRESULT CreateTextureFromSpriteResource(IDirect3DDevice9* dev, IDirect3DTexture9** out)
{
    HRSRC hrsc = FindResourceW(nullptr, MAKEINTRESOURCEW(IDB_TEXTURE), L"BMP");
    if (!hrsc)
        return D3DERR_NOTAVAILABLE;                     // -> IDS_ERR_NO_TEXTURES
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
    if (width == 0 || height == 0)
        return E_FAIL;

    // The original rounds width/height up to power-of-two (RVA 0xBF6C..0xBF9B).
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
// Per-ribbon initialisation (RVA 0x6B7C..0x6D18 + 0x26F8)
//----------------------------------------------------------------------------------------
void InitRibbon(Engine* e, int index)
{
    Ribbon& r = e->ribbons[index];
    r = Ribbon{};
    r.width = e->settings.RibbonWidth;

    // Three random draws; normalised to [0,1] by min/max spread (RVA 0x6BEC..0x6C1E),
    // then scaled by 1/min(NumRibbons, 10) (RVA 0x6C22..0x6C4C).
    float c0 = RandUnit(), c1 = RandUnit(), c2 = RandUnit();
    const float lo = (c0 < c1) ? ((c0 < c2) ? c0 : c2) : ((c1 < c2) ? c1 : c2);
    const float hi = (c0 > c1) ? ((c0 > c2) ? c0 : c2) : ((c1 > c2) ? c1 : c2);
    const float span = (hi - lo) > 0.0f ? (hi - lo) : 1.0f;
    c0 = (c0 - lo) / span;
    c1 = (c1 - lo) / span;
    c2 = (c2 - lo) / span;
    const float scale = 1.0f / static_cast<float>(
        (e->ribbonCount < 10) ? e->ribbonCount : 10);
    c0 *= scale; c1 *= scale; c2 *= scale;

    // Channel bytes: 0 if <= 0, 255 if >= 1, else comp*256 (RVA 0x6C54..0x6CD7).
    auto channel = [](float c) -> BYTE {
        if (c <= 0.0f) return 0x00;
        if (c >= 1.0f) return 0xFF;
        return static_cast<BYTE>(c * 256.0f);
    };
    r.color = D3DCOLOR_ARGB(0xFF, channel(c2), channel(c1), channel(c0));

    // Head box: same extents as the camera box (RVA 0x26F8 sets the per-axis bounds
    // from the monitor-derived world extents).
    r.bounds = D3DXVECTOR3(EngineConstants::CameraBoxExtent,
                           EngineConstants::CameraBoxExtent,
                           EngineConstants::CameraBoxExtent);
    r.velMin = EngineConstants::CameraVelMin;
    r.velMax = EngineConstants::CameraVelMax;

    // Random head position inside the box; velocity one random step.
    r.pos = D3DXVECTOR3(RandRange(-r.bounds.x, r.bounds.x),
                        RandRange(-r.bounds.y, r.bounds.y),
                        RandRange(-r.bounds.z, r.bounds.z));
    r.vel = D3DXVECTOR3(RandRange(r.velMin, r.velMax),
                        RandRange(r.velMin, r.velMax),
                        RandRange(r.velMin, r.velMax));

    // Fill the current + history slots with a straight segment along the initial
    // velocity so the ribbon fades in instead of popping (RVA 0x26F8 loop, 51 slots).
    const D3DXVECTOR3 dir = r.vel;
    for (int i = 0; i < 51; ++i)
    {
        RibbonFrame& f = r.frames[i];
        const float w = r.width;
        f.a.pos = r.pos - dir * static_cast<float>(i) - D3DXVECTOR3(w, w, w);
        f.b.pos = r.pos - dir * static_cast<float>(i) + D3DXVECTOR3(w, w, w);
        f.a.color = r.color;
        f.b.color = r.color;
        f.a.u = 0.0f; f.a.v = 0.0f;
        f.b.u = 1.0f; f.b.v = 1.0f;
    }
}

//----------------------------------------------------------------------------------------
// Camera tick (RVA 0x2120)
//----------------------------------------------------------------------------------------
void CameraFrameMove(CameraState& cam)
{
    // pos += vel (RVA 0x212F..0x2179)
    cam.prevPos = cam.pos;
    cam.pos = cam.pos + cam.vel;

    // motion delta stored for the ribbon edge computation (RVA 0x217E..0x21B4)
    cam.motionDelta = cam.prevPos - cam.pos;

    // velocity randomisation on wall hits (RVA 0x21B9..0x2354: per axis, if the
    // component is positive and pos exceeds the bound, a new negative velocity is
    // drawn from [velMin, velMax]; mirrored for the negative side).
    auto randomize = [&](float* velComp, float posComp, float bound, float sign) {
        if (*velComp > 0.0f && posComp > bound)
            *velComp = -RandRange(cam.velMin, cam.velMax);
        else if (*velComp < 0.0f && -bound > posComp)
            *velComp = RandRange(cam.velMin, cam.velMax);
        (void)sign;
    };
    randomize(&cam.vel.x, cam.pos.x, cam.bounds.x, 1.0f);
    randomize(&cam.vel.y, cam.pos.y, cam.bounds.y, 1.0f);
    randomize(&cam.vel.z, cam.pos.z, cam.bounds.z, 1.0f);

    // Three fractal-noise samples of the position with the magic offsets from
    // .rdata (0x18B8C = 23.432, 0x18B90 = 32.430, 0x18BB4 = 987.429,
    //          0x18B40 =  0.640, 0x18BB0 = 943.930, 0x18B94 = 38.843),
    // scaled by 10.2 (0x18B7C) into the wobble vector (RVA 0x235C..0x2456).
    D3DXVECTOR3 o1(23.432f, 32.430f, 987.429f);
    D3DXVECTOR3 o2(0.640f, 943.930f, -38.843f);
    D3DXVECTOR3 o3(0.0f, 0.0f, 0.0f);
    cam.look.x = NoiseAt(cam.pos) * EngineConstants::NoiseScale;
    cam.look.y = NoiseAt(cam.pos + o1) * EngineConstants::NoiseScale;
    cam.look.z = NoiseAt(cam.pos + o2) * EngineConstants::NoiseScale;
    (void)o3;
}

//----------------------------------------------------------------------------------------
// Ribbon tick (RVA 0x29D8)
//----------------------------------------------------------------------------------------
void RibbonFrameMove(Engine* e, Ribbon& r, const CameraState& cam)
{
    // 1. Shift the history: slot[50] <- slot[49] ... slot[1] <- slot[0]
    //    (RVA 0x2A22 loop: 49 descending 0x30-byte block copies; the oldest
    //    slot falls off and slot 0 receives the previous current frame).
    for (int i = 50; i >= 1; --i)
        r.frames[i] = r.frames[i - 1];

    // 2. Advance the head (RVA 0x2BD0..0x2CAC) with the same wall-bounce model
    //    as the camera.
    r.pos = r.pos + r.vel;
    auto randomize = [&](float* velComp, float posComp, float bound) {
        if (*velComp > 0.0f && posComp > bound)
            *velComp = -RandRange(r.velMin, r.velMax);
        else if (*velComp < 0.0f && -bound > posComp)
            *velComp = RandRange(r.velMin, r.velMax);
    };
    randomize(&r.vel.x, r.pos.x, r.bounds.x);
    randomize(&r.vel.y, r.pos.y, r.bounds.y);
    randomize(&r.vel.z, r.pos.z, r.bounds.z);

    // 3. Three noise samples at the head (RVA 0x2CAC..0x2D45) using the same
    //    magic offsets as the camera, scaled by 10.0 (0x18B78).
    const float kNoiseScale = 10.0f;
    D3DXVECTOR3 o1(23.432f, 32.430f, 987.429f);
    D3DXVECTOR3 o2(0.640f, 943.930f, -38.843f);
    D3DXVECTOR3 n0(NoiseAt(r.pos) * kNoiseScale,
                   NoiseAt(r.pos + o1) * kNoiseScale,
                   NoiseAt(r.pos + o2) * kNoiseScale);

    // 4. Edge direction: the noise gradient minus one sixth of the camera's
    //    per-axis wobble sum (RVA 0x2DA2..0x2DD6: (corner[i] + base[i]) / 6),
    //    crossed with the camera motion delta and normalised (RVA 0x2DDA..0x2E34).
    const float sixth = 0.166666f;                       // 0x18B3C
    D3DXVECTOR3 w((cam.look.x + cam.motionDelta.x) * sixth,
                  (cam.look.y + cam.motionDelta.y) * sixth,
                  (cam.look.z + cam.motionDelta.z) * sixth);
    D3DXVECTOR3 edge = Cross(n0 - w, cam.motionDelta);
    float len = Length(edge);
    if (len > 0.0f) { edge.x /= len; edge.y /= len; edge.z /= len; }

    // 5. The two strip endpoints: head +/- edge * width plus the noise offset
    //    (RVA 0x2E47..0x2EEA), written into the current slot. The previous slot
    //    shifts into history slot 0.
    RibbonFrame& f = r.frames[0];
    f.a.pos = r.pos - edge * r.width + n0 * 0.0f;
    f.b.pos = r.pos + edge * r.width + n0 * 0.0f;
    f.a.color = r.color;
    f.b.color = r.color;
    f.a.u = 0.0f; f.a.v = 0.0f;
    f.b.u = 1.0f; f.b.v = 1.0f;
}

//----------------------------------------------------------------------------------------
// Fullscreen fade quad (RVA 0x1D9C / 0x1AC0)
//----------------------------------------------------------------------------------------
struct FadeQuadVertex                // D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1, 28 bytes
{
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};

void DrawFullscreenQuad(IDirect3DDevice9* dev, IDirect3DVertexBuffer9* vb,
                        D3DCOLOR color, DWORD shadeMode, int textureSlot)
{
    HRESULT hr;
    FadeQuadVertex* v = nullptr;
    hr = vb->Lock(0, 0, reinterpret_cast<void**>(&v), 0);
    if (FAILED(hr) || !v)
        return;
    v[0] = { 0.0f, 0.0f, 0.0f, 1.0f, color, 0.0f, 0.0f };
    v[1] = { 0.0f, 1.0f, 0.0f, 1.0f, color, 0.0f, 1.0f };
    v[2] = { 1.0f, 1.0f, 0.0f, 1.0f, color, 1.0f, 1.0f };
    v[3] = { 1.0f, 0.0f, 0.0f, 1.0f, color, 1.0f, 0.0f };
    vb->Unlock();

    dev->SetRenderState(D3DRS_SHADEMODE, shadeMode);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    dev->SetTexture(textureSlot, nullptr);
    dev->SetVertexShader(nullptr);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    dev->SetStreamSource(0, vb, 0, sizeof(FadeQuadVertex));
    dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
}

} // namespace

//----------------------------------------------------------------------------------------
// lifecycle
//----------------------------------------------------------------------------------------
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
    for (int i = 0; i < EngineConstants::MaxRibbons; ++i)
        if (e->ribbonVB[i]) { e->ribbonVB[i]->Release(); e->ribbonVB[i] = nullptr; }
    if (e->fadeQuadVB)    { e->fadeQuadVB->Release();    e->fadeQuadVB = nullptr; }
    if (e->texture)       { e->texture->Release();       e->texture = nullptr; }
    if (e->device)        { e->device->Release();        e->device = nullptr; }
    if (e->d3d)           { e->d3d->Release();           e->d3d = nullptr; }
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

    // RVA 0x8730: Direct3DCreate9(32)
    e->d3d = Direct3DCreate9(32);
    if (!e->d3d)
        return MAKE_D3DHRESULT(2001);                // 0x82000001 in the original

    // Monitor enumeration (RVA 0x87B8) — one window per monitor or one spanned window.
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
    e->aspect = e->spanMultiMon
        ? static_cast<float>(e->viewport.Width) / static_cast<float>(e->viewport.Height)
        : static_cast<float>(rc.right - rc.left) / static_cast<float>((rc.bottom - rc.top) ? (rc.bottom - rc.top) : 1);

    // Device creation (RVA 0x94D4 -> CreateDevice). The original probes
    // CheckDeviceMultiSampleType for D3DMULTISAMPLE samples 0/2/4/8 per mode first.
    D3DPRESENT_PARAMETERS pp;
    std::memset(&pp, 0, sizeof(pp));
    pp.Windowed                   = TRUE;
    pp.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth            = e->viewport.Width;
    pp.BackBufferHeight           = e->viewport.Height;
    pp.BackBufferFormat           = D3DFMT_UNKNOWN;
    pp.EnableAutoDepthStencil     = TRUE;
    pp.AutoDepthStencilFormat     = D3DFMT_D24S8;
    pp.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT;

    const BOOL preview = (GetWindowLongW(hwnd, GWL_STYLE) & WS_CHILD) != 0;
    if (preview)
        pp.BackBufferFormat = D3DFMT_A8R8G8B8;

    DWORD vproc = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    D3DCAPS9 caps;
    if (SUCCEEDED(e->d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
        (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT))
        vproc = D3DCREATE_HARDWARE_VERTEXPROCESSING;

    HRESULT hr = e->d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                      vproc, &pp, &e->device);
    if (FAILED(hr) || !e->device)
        return hr;

    // Star sprite texture (RVA 0x26F8 -> 0xBEE0)
    hr = CreateTextureFromSpriteResource(e->device, &e->texture);
    if (FAILED(hr))
        return MAKE_D3DHRESULT(2007);                // -> IDS_ERR_NO_TEXTURES

    // Fullscreen fade quad (RVA 0x1AC0): 112 bytes = 4 x 28-byte XYZRHW|DIFFUSE|TEX1.
    hr = e->device->CreateVertexBuffer(sizeof(FadeQuadVertex) * 4, D3DUSAGE_WRITEONLY,
                                       D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                                       D3DPOOL_MANAGED, &e->fadeQuadVB, nullptr);
    if (FAILED(hr))
        return hr;

    // Ribbons + per-ribbon strips (RVA 0x6B7C loop, 0x990-byte vertex buffers).
    e->ribbonCount = e->settings.NumRibbons;
    for (int i = 0; i < e->ribbonCount; ++i)
    {
        InitRibbon(e, i);
        hr = e->device->CreateVertexBuffer(0x990, D3DUSAGE_WRITEONLY,
                                           D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                                           D3DPOOL_MANAGED, &e->ribbonVB[i], nullptr);
        if (FAILED(hr))
            return hr;
    }

    // Camera defaults (RVA 0x5F16..0x6060): pos (0, 0, -0.9), bounds 1.1,
    // velocity range [6.4e-5, 1.03e-4].
    e->camera.pos = D3DXVECTOR3(0.0f, 0.0f, -0.9f);
    e->camera.prevPos = e->camera.pos;
    e->camera.vel = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    e->camera.velMin = EngineConstants::CameraVelMin;
    e->camera.velMax = EngineConstants::CameraVelMax;
    e->camera.bounds = D3DXVECTOR3(EngineConstants::CameraBoxExtent,
                                   EngineConstants::CameraBoxExtent,
                                   EngineConstants::CameraBoxExtent);

    D3DSaverNoise::Initialize(static_cast<unsigned>(std::time(nullptr)));
    return S_OK;
}

void EngineFrameMove(Engine* e)
{
    if (!e)
        return;
    e->lastTick = GetTickCount();                     // RVA 0x6185

    CameraFrameMove(e->camera);                       // RVA 0x2120
    for (int i = 0; i < e->ribbonCount; ++i)          // RVA 0x61A8..0x6223
        RibbonFrameMove(e, e->ribbons[i], e->camera);
}

HRESULT EngineRenderScene(Engine* e)
{
    if (!e || !e->device)
        return E_POINTER;
    IDirect3DDevice9* dev = e->device;

    // Viewport + projection (RVA 0x6286..0x63F0)
    dev->SetViewport(&e->viewport);

    D3DMATRIX proj;
    BuildProjection(&proj, e->fov, e->aspect, e->zNear, e->zFar);
    dev->SetTransform(D3DTS_PROJECTION, &proj);

    // Clear: always when Blur is off; only during the first ClearGraceFrames
    // frames when Blur is on ( RVA 0x6598..0x65E6, [0x1C2C0] counter).
    const bool clear = (e->blur == 0) || (e->clearGrace > 0);
    if (e->clearGrace > 0)
        --e->clearGrace;
    if (clear)
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0);

    dev->BeginScene();                                // RVA 0x65F2

    // "Apply Blur and Fade": a near-black translucent fullscreen quad that dims the
    // previous frame instead of clearing it (RVA 0x6655..0x668E -> 0x1D9C, colour
    // 0xFC010101 with SHADEMODE 1).
    if (e->blur)
        DrawFullscreenQuad(dev, e->fadeQuadVB,
                           EngineConstants::FadeQuadColor, 1, 0);

    // View transform (RVA 0x66B3 -> 0x129BC, SetTransform(D3DTS_VIEW))
    const D3DXVECTOR3 at(e->camera.pos + e->camera.look * 0.1f);
    const D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
    D3DMATRIX view;
    BuildView(&view, e->camera.pos, at, up);
    dev->SetTransform(D3DTS_VIEW, &view);

    // Per-ribbon strip (RVA 0x66DE..0x670D -> 0x2F04)
    for (int i = 0; i < e->ribbonCount; ++i)
    {
        IDirect3DVertexBuffer9* vb = e->ribbonVB[i];
        if (!vb)
            continue;

        // Lock and copy the current + history region (0x990 bytes = 101 vertices).
        RibbonVertex* v = nullptr;
        if (FAILED(vb->Lock(0, 0, reinterpret_cast<void**>(&v), 0)) || !v)
            continue;
        const Ribbon& r = e->ribbons[i];
        int n = 0;
        for (int s = 0; s < 51 && n < 101; ++s)
        {
            v[n++] = r.frames[s].a;
            if (n < 101)
                v[n++] = r.frames[s].b;
        }
        vb->Unlock();

        // Render states (RVA 0x2F70..0x3219)
        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev->SetTexture(0, e->texture);
        dev->SetRenderState(D3DRS_TEXTUREFACTOR, r.color);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetVertexShader(nullptr);
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        dev->SetStreamSource(0, vb, 0, sizeof(RibbonVertex));
        // DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 99) -- RVA 0x3255
        dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0,
                           EngineConstants::StripTriangleCount);
    }

    dev->EndScene();                                  // RVA 0x6779

    // Present per screen (done by the run loop, RVA 0xAE44..0xAE75; the original
    // presents after ALL screens have been rendered).
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

//----------------------------------------------------------------------------------------
// HRESULT -> string id (RVA 0xB320)
//----------------------------------------------------------------------------------------
UINT EngineErrorToStringId(HRESULT hr)
{
    switch (hr)
    {
    case (HRESULT)0x8200000EULL: return IDS_ERR_COULD_NOT_START;      // custom code 0x0E
    case (HRESULT)0x82000006ULL: return IDS_ERR_FUNC_UNAVAILABLE_HW;  // custom code 0x06
    case (HRESULT)0x8200000DULL: return IDS_ERR_FUNC_UNAVAILABLE_HW;  // custom code 0x0D
    case (HRESULT)0x8200000BULL: return IDS_ERR_NO_TEXTURES;          // custom code 0x0B
    case (HRESULT)0x82000003ULL: return IDS_ERR_NEWER_CARD;           // custom code 0x03
    case (HRESULT)0x82000001ULL: return IDS_ERR_NO_DIRECT3D;          // Direct3DCreate9 failed
    case (HRESULT)0x82000009ULL: return IDS_ERR_TRY_LOWER_RES;        // custom code 0x09
    case (HRESULT)0x82000005ULL: return IDS_ERR_NO_HW_ACCEL;          // custom code 0x05
    case (HRESULT)0x8200000FULL: return IDS_NO_PREVIEW;               // custom code 0x0F
    case (HRESULT)0x82000007ULL: return IDS_ERR_GENERIC_START;        // custom code 0x07
    case (HRESULT)0x8200000CULL: return IDS_ERR_DESKTOP_RES;          // custom code 0x0C
    case D3DERR_MOREDATA: return IDS_ERR_NO_VIDEO_MEMORY;
    case E_FAIL:          return IDS_ERR_UNSPECIFIED;
    case E_OUTOFMEMORY:   return IDS_ERR_NO_SYS_MEMORY;
    default:              return IDS_ERR_UNSPECIFIED;
    }
}

} // namespace Ribbons
