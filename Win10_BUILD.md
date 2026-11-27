# Build & Install Guide

This file complements the per-saver `README.md` files with the exact toolchain
commands, MUI loader quirks, and installation steps for the **Ribbons.scr** and
**Mystify.scr** packages. Both were verified end-to-end with mingw-w64 g++ 14
and link against `d3d9` only — no DirectX SDK download is required.

## 1. Toolchain

### Microsoft Visual Studio 2019+ (matches the original binaries)

The original binaries were built with **MSVC 14.20 (VS 2019)**, x64 for the
.scr files and x86 for the .mui satellites. The reconstructed projects target
the same platform toolset (`v142`) and the Windows 10 SDK.

Install:
- Visual Studio 2019 (or 2022 with the v142 build tools component)
- Windows 10 SDK (10.0.19041.0 or newer)

### mingw-w64 (used to verify these packages)

```bash
# Debian / Ubuntu
sudo apt install mingw-w64
```

The D3D9 headers (`d3d9.h`, `d3d9types.h`, `d3d9caps.h`) ship with
`mingw-w64-common`; no separate DirectX SDK download is needed. Verified:
both packages compile and link warning-clean with g++ 14 (`-std=c++17`).

**Relocated toolchains:** if your `x86_64-w64-mingw32` toolchain was moved
away from its original install prefix, pass its `x86_64-w64-mingw32/lib`
directory to the Makefile so the linker finds `libmingwex.a` & co:

```bash
make -f Makefile.mingw mui MINGW_LIB=/opt/mingw64/usr/x86_64-w64-mingw32/lib
```

## 2. Building one saver

### Option A: Visual Studio (recommended)

```cmd
:: build the .scr (x64)
msbuild Ribbons\build\Ribbons.vcxproj        /p:Configuration=Release /p:Platform=x64
:: build the .mui (resource-only)
msbuild Ribbons\build\Ribbons.mui.vcxproj    /p:Configuration=Release /p:Platform=Win32
```

Outputs:
```
Ribbons\build\out\Release\Ribbons.scr
Ribbons\build\out\Release\Ribbons.scr.mui
```

### Option B: mingw-w64

```bash
cd Ribbons/build
make -f Makefile.mingw mui           # builds both targets
```

Outputs:
```
Ribbons/build/out/Release/Ribbons.scr       (x64)
Ribbons/build/out/Release/Ribbons.scr.mui   (resource-only)
```

Note on the .mui: the original satellites are x86 (PE32) resource-only DLLs.
MUI satellites are architecture-neutral resource containers, so the mingw
build emits them as x86_64 resource-only DLLs by default (functionally
identical next to the x64 .scr). Pass `MUI_CROSS=i686-w64-mingw32-` to the
Makefile if you want a 32-bit satellite.

### Option C: CMake

```bash
cmake -S Ribbons/build -B Ribbons/build/out -G "Visual Studio 17 2022" -A x64
cmake --build Ribbons/build/out --config Release
```

## 3. The MUI split

Each saver ships as two binaries:

- **`<saver>.scr`** — language-neutral: code, sprite ("BMP" 102), icons,
  manifest, version, and an `RT_MUI` configuration record (224 bytes, sig
  0xFECDFECD) listing which resource types stayed in the .scr (BMP / ICON /
  GROUP_ICON / VERSION / MANIFEST) and which were redirected to the .mui
  (DIALOG / STRING / VERSION), with the en-US language tag.
- **`<saver>.scr.mui`** — language-specific: the `RT_STRING` tables, the
  `RT_DIALOG` templates, its own `RT_VERSION`, and its own `RT_MUI` record.

At run time, `LoadStringW(hInstance=NULL, id, ...)` in the saver resolves
through the MUI redirector: satellite `RT_STRING` block `(id/16)+1`, entry
`id%16`. That +1 block shift versus the classic enumeration is why the
binary's string IDs (1, 300..302, 2100..2111, 2112, 2200..2212) map onto the
satellite blocks 1 / 19 / 132 / 133 / 138 / 139.

## 4. Installing the rebuilt savers

```cmd
:: back up the originals
copy %SystemRoot%\System32\Ribbons.scr       %SystemRoot%\System32\Ribbons.scr.orig
copy %SystemRoot%\System32\en-US\Ribbons.scr.mui %SystemRoot%\System32\en-US\Ribbons.scr.mui.orig

:: install the rebuilt ones
copy Ribbons\build\out\Release\Ribbons.scr       %SystemRoot%\System32\
copy Ribbons\build\out\Release\Ribbons.scr.mui   %SystemRoot%\System32\en-US\
```

(Repeat for Mystify.) Open Display Properties → Screen Saver to pick the
rebuilt saver, or run `Ribbons.scr /s` directly for a quick test,
`/p <hwnd>` for preview, `/t` for the 650x450 test window and `/c` for the
settings page.

## 5. Verifying equivalence

The reconstructed .scr files are not byte-identical to the originals (CRT
version, linker timestamp, debug directory and base relocations differ), but
the **resource directories** match in:

- `"BMP"` 102 sprite: verbatim (36,311 bytes in both savers)
- `RT_MUI` configuration record: verbatim
- `RT_ICON` / `RT_GROUP_ICON` frames: verbatim image data (re-wrapped with a
  standard `.ico` container for windres compatibility)
- `RT_MANIFEST` content
- `RT_VERSION` fixed-info and string tables (the rebuilt blob is 900 bytes vs
  the original's 904: `rc.exe` pads the version resource with 4 trailing zero
  bytes to its alignment, `windres` does not; every string and field matches)

The .mui files additionally match in:

- `RT_STRING` blocks: every ID and every text (byte sizes identical:
  46/192/3290/72/2816/1152)
- `RT_DIALOG` templates: every control, style, position and class
  (windres emits its own binary packing, functionally equivalent)
- `RT_MUI` record: verbatim (184 bytes)

Use `pefile` (or Resource Hacker) to dump both the original and rebuilt
resource trees and diff them.

## 6. Rebuild matrix

| Saver | .scr project | .mui project | Extra deps |
|---|---|---|---|
| Ribbons | `Ribbons\build\Ribbons.vcxproj` | `Ribbons\build\Ribbons.mui.vcxproj` | d3d9.lib |
| Mystify | `Mystify\build\Mystify.vcxproj` | `Mystify\build\Mystify.mui.vcxproj` | d3d9.lib |

## 7. Troubleshooting

**`cannot open include file 'd3d9.h'`** — install the mingw-w64 common
headers (`mingw-w64-common` on Debian/Ubuntu) or the Windows SDK for MSVC.

**`x86_64-w64-mingw32-windres: preprocessing failed`** — windres shells out
to `x86_64-w64-mingw32-gcc` for preprocessing; make sure the gcc binary is on
PATH (it ships in the same mingw-w64 package).

**The .mui builds but strings do not show** — the MUI loader pairs .scr and
.mui by the checksum inside the `RT_MUI` records; keep both files from the
same build.

**`muirct not found`** — not needed for these packages: the .rc scripts
already contain the correct split and the verbatim `RT_MUI` records, so no
post-build muirct step is required.
