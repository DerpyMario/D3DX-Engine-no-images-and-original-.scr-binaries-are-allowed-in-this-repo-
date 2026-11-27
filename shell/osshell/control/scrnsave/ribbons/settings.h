//============================================================================================
// settings.h -- Ribbons.scr settings model
//
// Registry hive (verified from the RegOpenKeyExW/RegQueryValueExW call sites,
// Ribbons.scr RVA 0x80D0 and .rdata strings 0x180B0..0x181C0):
//
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Screensavers\Ribbons
//
//   Value           Type    Default  Clamp         Field                 Source RVA
//   --------------- ------- -------- ------------- --------------------- ----------
//   NumRibbons      DWORD   10       1 .. 250      NumRibbons            0xA1694
//   RibbonWidth     FLOAT   0.07     0.01 .. 2.5   RibbonWidth           0xA1698
//   CameraFOV       FLOAT   0.4884   0.5 .. 3.0    CameraFOV (radians)   0x2A00C
//   SpanMultiMon    DWORD   1        bool          SpanMultiMon          0xA1690
//   Blur            DWORD   1        bool          Blur (fade trail)     0xA1691
//
// Per-monitor overlay (sub_BAA8, strings at 0x184E0..0x18548) is stored under
// "Screen %d" sub-values: AllScreensSame / Adapter ID (REG_SZ) / Leave Black /
// Disable Hardware. Those live in the Display Settings dialogs (200/201).
//============================================================================================
#pragma once

namespace Ribbons {

struct Settings
{
    int   NumRibbons      = 10;     // number of ribbon strips (1..250)
    float RibbonWidth     = 0.07f;  // ribbon half-width scale (0.01..2.5)
    float CameraFOV       = 0.4884f;// camera field-of-view in radians (clamped 0.5..3.0)
    bool  SpanMultiMon    = true;   // one window across the whole virtual desktop
    bool  Blur            = true;   // "Apply Blur and Fade": alpha-fade pass instead of Clear

    // --- per-monitor display settings (dialogs 200/201, "Screen %d" registry block) ---
    bool  AllScreensSame  = true;   // every monitor uses the same adapter config
    bool  DisableHardware[16] = {}; // "Disable hardware 3D rendering" per monitor
    bool  LeaveBlack[16]      = {}; // "Display nothing on this monitor" per monitor
};

// Load from HKCU\...\Screensavers\Ribbons; missing values keep their defaults,
// present values are clamped exactly like the original (sub_80D0).
void LoadSettings(Settings* s);

// Persist through RegCreateKeyExW (per-monitor block handled by SaveDisplaySettings).
void SaveSettings(const Settings* s);

// Load/save the per-monitor adapter configuration used by dialogs 200 / 201.
void LoadDisplaySettings(Settings* s, int monitorCount);
void SaveDisplaySettings(const Settings* s, int monitorCount);

// The four "Preset" buttons of dialog 105 (IDs 1033/1034/1035/1037). The original
// stores the presets in .rdata float tables and simply overwrites the settings.
void ApplyPreset(Settings* s, int which);

} // namespace Ribbons
