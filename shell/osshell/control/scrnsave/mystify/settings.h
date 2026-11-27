//============================================================================================
// settings.h -- Mystify.scr settings model
//
// Registry hive (verified from the RegOpenKeyExW/RegQueryValueExW call sites,
// Mystify.scr RVA 0x7290 and .rdata strings 0x180A0..0x181A0):
//
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Screensavers\Mystify
//
//   Value           Type    Default  Clamp         Field                Source RVA
//   --------------- ------- -------- ------------- -------------------- ----------
//   NumLines        DWORD   1        1 .. 20       NumLines             0xF914
//   LineWidth       FLOAT   0.02     0.01 .. 0.1   LineWidth            0xF918
//   CameraFOV       FLOAT   1.41     0.5 .. 3.0    CameraFOV (radians)  0x2960
//   SpanMultiMon    DWORD   1        bool          SpanMultiMon         0xF910
//
// The "Apply Blur and Fade" checkbox of dialog 105 maps to the engine field at
// +0xF911. Interesting quirk of the original binary: the settings loader (RVA
// 0x7290) never queries a "Blur" registry value, so the flag always keeps its
// constructor default (1 = enabled); the reconstruction keeps the same behaviour.
//
// Per-monitor overlay (sub_BF68, strings at 0x184C8..0x18530) is stored under
// "Screen %d" sub-values: AllScreensSame / Adapter ID (REG_SZ) / Leave Black /
// Disable Hardware.
//============================================================================================
#pragma once

namespace Mystify {

struct Settings
{
    int   NumLines       = 1;      // number of line/polygon strips (1..20)
    float LineWidth      = 0.02f;  // strip half-width scale (0.01..0.1)
    float CameraFOV      = 1.41f;  // camera field-of-view in radians (clamped 0.5..3.0)
    bool  SpanMultiMon   = true;   // one window across the whole virtual desktop
    bool  Blur           = true;   // "Apply Blur and Fade" (constructor default, never loaded)

    // --- per-monitor display settings (dialogs 200/201, "Screen %d" registry block) ---
    bool  AllScreensSame = true;
    bool  DisableHardware[16] = {};
    bool  LeaveBlack[16]      = {};
};

// Load from HKCU\...\Screensavers\Mystify; missing values keep their defaults,
// present values are clamped exactly like the original (sub_7290).
void LoadSettings(Settings* s);

// Persist through RegCreateKeyExW.
void SaveSettings(const Settings* s);

// Load/save the per-monitor adapter configuration used by dialogs 200 / 201.
void LoadDisplaySettings(Settings* s, int monitorCount);
void SaveDisplaySettings(const Settings* s, int monitorCount);

} // namespace Mystify
