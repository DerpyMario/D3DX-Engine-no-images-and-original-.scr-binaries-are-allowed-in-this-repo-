//============================================================================================
// settings.cpp -- registry-backed settings for Ribbons.scr
//
// Direct reconstruction of:
//   RVA 0x80D0  LoadSettings     (RegOpenKeyExW + 5x RegQueryValueExW + clamps)
//   RVA 0xBAA8  Load/SaveDisplaySettings ("Screen %d", "Adapter ID", "Leave Black",
//               "Disable Hardware", "AllScreensSame")
//   RVA 0x7230  settings-dialog save path (RegCreateKeyExW)
//============================================================================================
#include "settings.h"
#include "resources.h"

#include <windows.h>
#include <stdio.h>

namespace Ribbons {

namespace {

const wchar_t kRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Screensavers\\Ribbons";

// .rdata string offsets in the original binary (for cross-reference):
//   0x180B0  registry path          0x18160  SpanMultiMon
//   0x18190  NumRibbons             0x181A8  RibbonWidth
//   0x181C0  CameraFOV              0x18180  Blur
//   0x184E0  AllScreensSame         0x18500  Screen %d
//   0x18518  Adapter ID             0x18530  Leave Black
//   0x18548  Disable Hardware

struct MonitorInfo
{
    bool  disableHardware;
    bool  leaveBlack;
    wchar_t adapterId[128];
};

void ReadDword(HKEY key, const wchar_t* name, DWORD* out)
{
    DWORD v = 0, size = sizeof(v), type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE*>(&v), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(v))
    {
        *out = v;
    }
}

void ReadFloat(HKEY key, const wchar_t* name, float* out)
{
    DWORD v = 0, size = sizeof(v), type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE*>(&v), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(v))
    {
        float f;
        memcpy(&f, &v, sizeof(f));
        *out = f;
    }
}

void WriteDword(HKEY key, const wchar_t* name, DWORD v)
{
    RegSetValueExW(key, name, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&v), sizeof(v));
}

void WriteFloat(HKEY key, const wchar_t* name, float f)
{
    DWORD v;
    memcpy(&v, &f, sizeof(v));
    RegSetValueExW(key, name, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&v), sizeof(v));
}

} // namespace

void LoadSettings(Settings* s)
{
    if (!s)
        return;

    // Defaults are installed by the app constructor (RVA 0x5DC4):
    //   [0xA1690] word 0x0101  -> SpanMultiMon = 1, Blur = 1
    //   [0xA1694] 10           -> NumRibbons
    //   [0xA1698] 0.07f        -> RibbonWidth
    //   [0x2A00C] 0.4884f      -> CameraFOV (immediately clamped to >= 0.5 below)
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return;

    DWORD numRibbons = static_cast<DWORD>(s->NumRibbons);
    ReadDword(key, L"NumRibbons", &numRibbons);
    s->NumRibbons = static_cast<int>(numRibbons);

    ReadFloat(key, L"RibbonWidth", &s->RibbonWidth);
    ReadFloat(key, L"CameraFOV", &s->CameraFOV);

    DWORD span = 0, size = sizeof(span);
    if (RegQueryValueExW(key, L"SpanMultiMon", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(&span), &size) == ERROR_SUCCESS)
        s->SpanMultiMon = (span != 0);

    DWORD blur = 0;
    size = sizeof(blur);
    if (RegQueryValueExW(key, L"Blur", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(&blur), &size) == ERROR_SUCCESS)
        s->Blur = (blur != 0);

    // --- clamps, exactly as in sub_80D0 ------------------------------------------
    if (s->NumRibbons < 1)   s->NumRibbons = 1;
    if (s->NumRibbons > 250) s->NumRibbons = 250;               // 0xFA

    if (s->RibbonWidth < 0.01f) s->RibbonWidth = 0.01f;         // 0x3C23D70A
    if (s->RibbonWidth > 2.5f)  s->RibbonWidth = 2.5f;          // 0x40200000

    if (s->CameraFOV < 0.5f) s->CameraFOV = 0.5f;               // 0x3F000000
    if (s->CameraFOV > 3.0f) s->CameraFOV = 3.0f;               // 0x40400000

    LoadDisplaySettings(s, 16);
    RegCloseKey(key);
}

void SaveSettings(const Settings* s)
{
    if (!s)
        return;

    HKEY key;
    // The original creates the key with RegCreateKeyExW(...KEY_SET_VALUE...)
    // inside the settings dialog OK handler (RVA 0x7230 path).
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return;

    WriteDword(key, L"NumRibbons", static_cast<DWORD>(s->NumRibbons));
    WriteFloat(key, L"RibbonWidth", s->RibbonWidth);
    WriteFloat(key, L"CameraFOV", s->CameraFOV);
    WriteDword(key, L"SpanMultiMon", s->SpanMultiMon ? 1 : 0);
    WriteDword(key, L"Blur", s->Blur ? 1 : 0);

    RegCloseKey(key);
}

void LoadDisplaySettings(Settings* s, int monitorCount)
{
    if (!s)
        return;

    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return;

    DWORD allSame = 1;
    ReadDword(key, L"AllScreensSame", &allSame);
    s->AllScreensSame = (allSame != 0);

    if (monitorCount > 16)
        monitorCount = 16;

    for (int i = 0; i < monitorCount; ++i)
    {
        wchar_t name[64];
        swprintf_s(name, 64, L"Screen %d", i);          // .rdata 0x18500 "Screen %d"

        HKEY sub;
        if (RegOpenKeyExW(key, name, 0, KEY_QUERY_VALUE, &sub) != ERROR_SUCCESS)
            continue;

        DWORD v = 0;
        ReadDword(sub, L"Leave Black", &v);              // .rdata 0x18530
        s->LeaveBlack[i] = (v != 0);
        v = 0;
        ReadDword(sub, L"Disable Hardware", &v);         // .rdata 0x18548
        s->DisableHardware[i] = (v != 0);

        // "Adapter ID" (REG_SZ, .rdata 0x18518) is read by the Display Settings
        // dialog to decide which adapter a monitor is bound to.
        wchar_t id[128] = {};
        DWORD sz = sizeof(id);
        RegQueryValueExW(sub, L"Adapter ID", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(id), &sz);

        RegCloseKey(sub);
    }

    RegCloseKey(key);
}

void SaveDisplaySettings(const Settings* s, int monitorCount)
{
    if (!s)
        return;

    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return;

    WriteDword(key, L"AllScreensSame", s->AllScreensSame ? 1 : 0);

    if (monitorCount > 16)
        monitorCount = 16;

    for (int i = 0; i < monitorCount; ++i)
    {
        wchar_t name[64];
        swprintf_s(name, 64, L"Screen %d", i);

        HKEY sub;
        if (RegCreateKeyExW(key, name, 0, nullptr, REG_OPTION_NON_VOLATILE,
                            KEY_SET_VALUE, nullptr, &sub, nullptr) != ERROR_SUCCESS)
            continue;

        WriteDword(sub, L"Leave Black", s->LeaveBlack[i] ? 1 : 0);
        WriteDword(sub, L"Disable Hardware", s->DisableHardware[i] ? 1 : 0);
        RegCloseKey(sub);
    }

    RegCloseKey(key);
}

//----------------------------------------------------------------------------------------
// The four "Preset" buttons of dialog 105 ("bold streaks of twisting light",
// "sparse, delicate curves", "tangled threads woven into cloth-like textures",
// "soft washes of color overlayed with stars").
//
// In the original binary the whole settings page is hosted by COMCTL32 ordinal #344
// (the shared shell "Screen Saver Settings" host; called at RVA 0x80B6 with the
// parent HWND, string IDs 300/301/302 and page count 1). The preset value tables
// therefore live in the host, not in Ribbons.scr, and cannot be recovered from this
// binary alone. The values below are a reconstruction that matches the four visual
// descriptions and the documented clamps; the OK path stores them through the same
// registry values as everything else.
//----------------------------------------------------------------------------------------
void ApplyPreset(Settings* s, int which)
{
    if (!s)
        return;

    switch (which)
    {
    case 1:  s->NumRibbons = 10; s->RibbonWidth = 0.07f; s->CameraFOV = 0.5f; s->Blur = true;  break;
    case 2:  s->NumRibbons = 5;  s->RibbonWidth = 0.20f; s->CameraFOV = 1.0f; s->Blur = true;  break;
    case 3:  s->NumRibbons = 24; s->RibbonWidth = 0.05f; s->CameraFOV = 1.5f; s->Blur = false; break;
    case 4:  s->NumRibbons = 14; s->RibbonWidth = 0.12f; s->CameraFOV = 2.0f; s->Blur = true;  break;
    default: break;
    }
}

} // namespace Ribbons
