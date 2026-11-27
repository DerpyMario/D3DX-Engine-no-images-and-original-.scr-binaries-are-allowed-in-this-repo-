//============================================================================================
// settings.cpp -- registry-backed settings for Mystify.scr
//
// Direct reconstruction of:
//   RVA 0x7290  LoadSettings     (RegOpenKeyExW + 4x RegQueryValueExW + clamps)
//   RVA 0xBF68  Load/SaveDisplaySettings ("Screen %d", "Adapter ID", "Leave Black",
//               "Disable Hardware", "AllScreensSame")
//============================================================================================
#include "settings.h"
#include "resources.h"

#include <windows.h>
#include <stdio.h>

namespace Mystify {

namespace {

const wchar_t kRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Screensavers\\Mystify";

// .rdata string offsets in the original binary (for cross-reference):
//   0x180A0  registry path          0x18150  SpanMultiMon
//   0x18170  NumLines               0x18188  LineWidth
//   0x181A0  CameraFOV              0x184C8  AllScreensSame
//   0x18500  Adapter ID             0x18518  Leave Black
//   0x18530  Disable Hardware

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

    // Defaults installed by the app constructor (RVA 0x4FC8):
    //   [0xF910] word 0x0101  -> SpanMultiMon = 1, Blur = 1
    //   [0xF914] 1            -> NumLines
    //   [0xF918] 0.02f        -> LineWidth (0x3CA3D70A)
    //   [0x2960] 1.41f        -> CameraFOV (0x3FB47AE1)
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return;

    DWORD numLines = static_cast<DWORD>(s->NumLines);
    ReadDword(key, L"NumLines", &numLines);
    s->NumLines = static_cast<int>(numLines);

    ReadFloat(key, L"LineWidth", &s->LineWidth);
    ReadFloat(key, L"CameraFOV", &s->CameraFOV);

    DWORD span = 0, size = sizeof(span);
    if (RegQueryValueExW(key, L"SpanMultiMon", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(&span), &size) == ERROR_SUCCESS)
        s->SpanMultiMon = (span != 0);

    // NOTE: the original never queries "Blur" here - see settings.h.

    // --- clamps, exactly as in sub_7290 ------------------------------------------
    if (s->NumLines < 1)   s->NumLines = 1;
    if (s->NumLines > 20)  s->NumLines = 20;                    // 0x14

    if (s->LineWidth < 0.01f) s->LineWidth = 0.01f;             // 0x3C23D70A
    if (s->LineWidth > 0.1f)  s->LineWidth = 0.1f;              // 0x3DCCCCCD

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
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS)
        return;

    WriteDword(key, L"NumLines", static_cast<DWORD>(s->NumLines));
    WriteFloat(key, L"LineWidth", s->LineWidth);
    WriteFloat(key, L"CameraFOV", s->CameraFOV);
    WriteDword(key, L"SpanMultiMon", s->SpanMultiMon ? 1 : 0);
    // The original's dialog host stores the blur flag as well; kept for symmetry.
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
        swprintf_s(name, 64, L"Screen %d");              // .rdata 0x184E0 "Screen %d"

        HKEY sub;
        if (RegOpenKeyExW(key, name, 0, KEY_QUERY_VALUE, &sub) != ERROR_SUCCESS)
            continue;

        DWORD v = 0;
        ReadDword(sub, L"Leave Black", &v);              // .rdata 0x18518
        s->LeaveBlack[i] = (v != 0);
        v = 0;
        ReadDword(sub, L"Disable Hardware", &v);         // .rdata 0x18530
        s->DisableHardware[i] = (v != 0);

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

} // namespace Mystify
