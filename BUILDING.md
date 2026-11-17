# Building

## Requirements

* CMake 3.16 or later, and Ninja or another generator
* one of
  * **MinGW-w64** (GCC), as a cross compiler on Linux or natively (MSYS2) –
    this is the tested configuration (GCC 13, mingw-w64 11)
  * **Visual Studio** (x64) with the **DirectX SDK (June 2010)** for
    `d3dx9.h` / `d3dx9.lib` (`DXSDK_DIR`) – supported by the build files,
    not tested
* Python 3 (standard library only), to extract the artwork

## 1. Extract the artwork

The repository contains no images.  Copy them out of your own screen savers:

```bash
python3 tools/extract_assets.py /path/to/Windows/System32
```

or one file at a time (`python3 tools/extract_assets.py path/to/ssBranded.scr`).
This writes `./assets/<saver>/res/...`, which git ignores.  `--list` shows
the resources of a file instead.

To build without the artwork, configure with `-DSCRNSAVE_NO_ASSETS=ON`:
Aurora and Bubbles then fail to load their textures, 3D Text only offers the
solid colour surface, Windows Logo shows only the logo, and Windows Energy
reports that it cannot find its textures.

## 2. Configure and build

Cross-compiling with MinGW-w64:

```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

Visual Studio (from a developer prompt):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDXSDK_DIR="C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)"
```

The result is `build/Aurora.scr`, `Bubbles.scr`, `ssText3d.scr`,
`ssBranded.scr` and `logon.scr`.

| Option                 | Default    | Meaning                                                   |
|------------------------|------------|-----------------------------------------------------------|
| `SCRNSAVE_ASSET_DIR`   | `./assets` | where `extract_assets.py` put the artwork                 |
| `SCRNSAVE_NO_ASSETS`   | `OFF`      | build without the artwork                                 |
| `SCRNSAVE_BUILD_TESTS` | `OFF`      | build the differential tests (MinGW-w64 only)             |

GNU builds use `-fno-builtin`, so that the savers call the C run-time
functions of `msvcrt.dll` as the originals do (GCC would otherwise turn
`sinf`/`cosf` pairs into its own `sincosf`, with slightly different results).

## 3. Run

Right-click a `.scr` file and choose *Install*, or copy it to
`C:\Windows\System32`.  Command line: `/s` runs it, `/c` opens its settings,
`/p <hwnd>` previews it.

* The Direct3D savers need `d3dx9_43.dll` (DirectX End-User Runtime); the
  originals had D3DX linked in.
* 3D Text and Windows Logo use `winbrand.dll` (part of Windows since Vista;
  Windows Logo loads it only when needed).
* **Wine:** Wine's `d3dx9` cannot compile the effects of Windows Energy
  (`sampler_state` blocks) and does not implement
  `D3DXMESHOPT_VERTEXCACHE`, so the rebuilt `ssBranded.scr` stops with an
  error there.  For a run under Wine, the test build makes
  `build/origd3dx/ssBranded.scr`, which borrows the effect and mesh loaders
  from your original (put it next to it as `ssBranded.orig.scr`, or set
  `SSBRANDED_ORIGINAL`).  Wine has no `winbrand.dll`; any DLL that exports
  `BrandingFormatString` and `BrandingLoadImage` will do for 3D Text and
  Windows Logo.

## 4. Differential tests

```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DSCRNSAVE_BUILD_TESTS=ON
```

```bash
cmake --build build
```

Each test takes the path of the original it was written for:

```bash
build/test_branded.exe /path/to/ssBranded.scr
```

| Test           | Original (x64)                       | Link time stamp |
|----------------|--------------------------------------|-----------------|
| `test_aurora`  | `Aurora.scr` 6.0.6000.16386          | `0x4549BB49`    |
| `test_bubbles` | `Bubbles.scr` 6.0.6000.16386         | `0x4549BB49`    |
| `test_logon`   | `logon.scr` 6.0.6001.18000           | `0x47919773`    |
| `test_text3d`  | `ssText3d.scr` 6.0.6001.18000        | `0x47919797`    |
| `test_branded` | `ssBranded.scr` 6.0.6001.18000       | `0x479197c3`    |

The addresses in the tests belong to exactly these builds; a test refuses
any other file.  `test_text3d` needs a Direct3D device (a display) and the
fonts it names.  The tests run on Windows and under Wine.

## The original build

The originals were built with the NT build environment (`build.exe`) from
`shell\osshell\control\scrnsave`; output went to `objfre\amd64` below each
directory (`D3DSaver9.lib`, `Helpers.lib`, `Shape3D.lib`, the `.scr` files)
and the localisable resources were split off into `en-US\*.scr.mui`.  The
`dirs` / `sources` / `makefile` files in the tree reconstruct that layout
from the symbol files for reference; the CMake build builds all resources,
the localisable ones included, into the `.scr` files.
