# Windows Vista+ and Windows XP screen savers – reconstructed source

Readable, rebuildable C/C++ source, HLSL effects and resource scripts for the
five Direct3D / GDI screen savers of Windows Vista (x64) and above, and for the screen
savers of Windows XP / Windows Server 2003 x64 ([below](#windows-xp--windows-server-2003)),
reconstructed from the shipped binaries and, where they exist, their public
symbol files.

| Screen saver    | File            | Version          | Source directory (original build path)              |
|-----------------|-----------------|------------------|-----------------------------------------------------|
| Aurora          | `Aurora.scr`    | 6.0.6000.16386   | `shell\osshell\control\scrnsave\aurora`             |
| Bubbles         | `Bubbles.scr`   | 6.0.6000.16386   | `shell\osshell\control\scrnsave\bubbles`            |
| 3D Text         | `ssText3d.scr`  | 6.0.6001.18000   | `shell\osshell\control\scrnsave\text3d`             |
| Windows Energy  | `ssBranded.scr` | 6.0.6001.18000   | `shell\osshell\control\scrnsave\branded\src`        |
| Windows Logo    | `logon.scr`     | 6.0.6001.18000   | `shell\osshell\control\scrnsave\logon`              |
| Mystify         | `Mystify.scr`   | 10.0.19041.4355  | `shell\osshell\control\scrnsave\mystify`            |
| Ribbons         | `Ribbons.scr`   | 10.0.19041.4355  | `shell\osshell\control\scrnsave\ribbons`            |

and the libraries they are built from:

| Library         | Source directory                                    | Used by                       |
|-----------------|-----------------------------------------------------|-------------------------------|
| `D3DSaver9.lib` | `shell\osshell\control\scrnsave\d3dsaver9`          | Aurora, Bubbles, 3D Text, Energy |
| `Helpers.lib`   | `shell\osshell\control\scrnsave\helper`             | Aurora, Bubbles, 3D Text, Energy |
| `Shape3D.lib`   | `shell\osshell\control\scrnsave\shape3d`            | 3D Text                       |
| `scrnsavw.lib`  | `shell\osshell\control\scrnsave\common\scrnsavw`    | Windows Logo                  |

The directory names, file names and object lists are those recorded in the
symbol files (e.g. `o:\rtm.obj.amd64fre\shell\osshell\control\scrnsave\branded\src\objfre\amd64\wipe.obj`
became `shell/osshell/control/scrnsave/branded/src/wipe.cpp`).  Every source
file names the object it was reconstructed from in its header.  The
reconstructed NT build files (`dirs`, `sources`, `makefile`) record the
original build layout; the build of this repository is CMake
(see [BUILDING.md](BUILDING.md)).

## What is not here

* **No images and no original binaries.**  Icons, bitmaps, Targa and JPEG
  textures stay in your own copies of the screen savers.
  `tools/extract_assets.py` copies them out of your `.scr` files into
  `./assets` (ignored by git) for the build; without them the savers can be
  built with `-DSCRNSAVE_NO_ASSETS=ON`.
* **D3DX.**  The originals link the D3DX 9 library of the Windows SDK
  statically.  It is not part of the reconstruction; the rebuilt savers use
  `d3dx9` from the DirectX SDK / MinGW-w64 (`d3dx9_43.dll` at run time).

The text resources of Windows Energy – 13 effect files (`fx/*.fx`), 3 meshes
(`media/*.x`) and 15 key frame animations (`media/*.anim`) – are source code
and are included byte for byte.  The other four savers draw with the
fixed-function pipeline and have no shaders.

## How the sources were recovered

* Functions, classes, globals and static data are named after the public
  symbols (`S_PUB32`, and `S_LPROC32` for static functions); virtual function
  tables, constructors and destructors give the class hierarchies and the
  order of the virtual functions.
* Class and structure layouts were derived from the code and are checked
  against the binaries with `static_assert`s (x64).
* Names that the symbol files do not contain – local variables, parameters,
  members, enumerators, resource identifiers – are reconstructions in the
  style of the surrounding code; where the code of the original is odd
  (dead stores, a read past an array, an icon resource with a string name)
  it is reproduced and commented, not fixed.
* Code merged by the linker (identical functions folded, constants
  propagated, functions inlined across modules) was split up again into the
  functions the symbols name.

## Verification

* `tools/verify/test_*.cpp` map the original `.scr` into a test process,
  call its functions by address and compare them with the rebuilt ones,
  frame by frame (197 checks: Aurora 14, Bubbles 35, Windows Logo 17,
  3D Text 46, Windows Energy 85 – all passing).  The rebuilt code is linked
  so that both sides use the same `msvcrt.dll` math (and, for Windows Energy,
  the same D3DX math), so most results are compared bit for bit.
* Built with windres, the resources are byte-identical to those of the
  originals and their `.mui` files: Windows Energy 171 of 171, Windows Logo
  93 of 93, Aurora 11 of 13, Bubbles 15 of 17, 3D Text 16 of 18.  The
  remaining dialog templates differ only in the letter case of two
  common-control class names (`msctls_trackbar32`, `SysTabControl32`),
  which windres stores in upper case.
* Under Wine, the rebuilt savers run like the originals (Windows Energy with
  the effect loader of the original, see [BUILDING.md](BUILDING.md)).
* **Mystify and Ribbons** (Windows 10): both build warning-clean with
  mingw-w64 g++ 13 on Linux (`-std=c++17`, four output files each: `.scr`
  and `.scr.mui`).  PE analysis of the rebuilt binaries confirms correct
  structure: `d3d9.dll:Direct3DCreate9` import, expected Win32 API imports
  (`comctl32`, `user32`, `gdi32`, `advapi32`, `kernel32`, `ole32`), and all
  resource types present (`"BMP"` 102 sprite, `RT_MUI` record, icons,
  version, manifest in the `.scr`; dialog, string table, version, `RT_MUI`
  in the `.mui`).  The savers link `d3d9` only (no D3DX dependency), so
  they avoid the Wine `d3dx9` limitation that affects `ssBranded.scr`.
  See [Win10_BUILD.md](Win10_BUILD.md) for the Win10 saver build.

## Windows XP / Windows Server 2003

The screen savers of the x64 editions of Windows XP and Windows Server 2003
(SP2), in `winxp/` with the same layout.  The GDI savers are x64 programs;
the four Direct3D 8 savers are 32-bit programs that x64 Windows installs
as `wss*.scr`.

| Screen saver      | File (x64 edition)          | Version         | Source directory (below `winxp\shell\osshell\control\scrnsave`) |
|-------------------|-----------------------------|-----------------|--------------------------------------------|
| (Blank)           | `scrnsave.scr`              | 5.2.3790.1830   | `blank`                                    |
| Starfield         | `ssstars.scr`               | 5.2.3790.1830   | `stars`                                    |
| Mystify           | `ssmyst.scr`                | 5.2.3790.1830   | `mystify`                                  |
| Beziers           | `ssbezier.scr`              | 5.2.3790.1830   | `bezier`                                   |
| Marquee           | `ssmarque.scr`              | 5.2.3790.1830   | `marquee`                                  |
| Windows logo      | `logon.scr`                 | 5.2.3790.3959   | `logon`                                    |
| 3D FlowerBox      | `wssflwbox.scr` (x86)       | 5.2.3790.3959   | `flowerbox`                                |
| 3D Text           | `wsstext3d.scr` (x86)       | 5.2.3790.3959   | `text3d`                                   |
| 3D Pipes          | `wsspipes.scr` (x86)        | 5.2.3790.3959   | `pipes`                                    |
| 3D Flying Objects | `wss3dfo.scr` (x86)         | 5.2.3790.1830   | `3dfo`                                     |
| My Pictures Slideshow | `ssmypics.scr`          | 5.2.3790.1830   | `mypics` (GDI+)                            |

| Library         | Source directory          | Used by                                        |
|-----------------|---------------------------|------------------------------------------------|
| `scrnsavw.lib`  | `common\scrnsavw`         | Starfield, Mystify, Beziers, Marquee, logo     |
| `d3dsaver.lib`  | `d3dsaver`                | the four Direct3D 8 savers                     |
| (`comstr.c`)    | `common`                  | Starfield, Mystify, Beziers, Marquee           |

* Symbol files exist for `logon.scr` and three of the Direct3D savers
  (FlowerBox, 3D Text, Pipes); their names are used as for Vista.  The
  other savers have none: library code was identified by matching it
  against the symbol-carrying binaries, and the remaining names follow the
  OpenGL / Windows NT versions of the same savers where those are known,
  or are chosen by the reconstruction.  Each file says which applies.
* The Direct3D 8 savers link Microsoft's static D3DX 8 library, which is no
  longer distributed: `external/d3dx8` is a clean-room implementation of
  the part of its interface they call (see its README).  The software
  rasterizer the originals register (`d3d8rgb.lib`, internal to the
  Windows source tree) is left out (`NO_D3D8RGB`).
* With windres, the resources are byte-identical to those of the originals
  except for the letter case of control class names in some dialogs (as
  for Vista).  The XP savers have not been run: no Wine was available to
  test them.
* My Pictures Slideshow (`ssmypics.scr`, no symbol file) is reconstructed
  from its code alone; its class and function names are descriptive ones.
  windres writes its dialog's icon control with a size of 0 (the original
  has 20 x 20), and its manifest leaves out the original's comment line.

See [BUILDING.md](BUILDING.md#windows-xp--windows-server-2003) for the build.

## Layout

```
CMakeLists.txt                  build of the Vista savers (MinGW-w64 or Visual Studio)
cmake/                          MinGW-w64 toolchain files (x86_64, i686)
external/winbrand/              import library definition of winbrand.dll (Vista)
external/d3dx8/                 clean-room D3DX 8 subset (XP Direct3D savers)
external/shlwapip/              import library definition: IsOS by ordinal (XP logo)
shell/osshell/control/scrnsave/ the Vista sources, in their original places
winxp/CMakeLists.txt            build of the Windows XP / 2003 savers
winxp/shell/osshell/control/scrnsave/  the Windows XP / 2003 sources
tools/extract_assets.py         copies the artwork out of your originals
tools/verify/                   differential tests against the originals (Vista)
```
