# Windows Vista screen savers – reconstructed source

Readable, rebuildable C/C++ source, HLSL effects and resource scripts for the
five Direct3D / GDI screen savers of Windows Vista (x64), reconstructed from
the shipped binaries and their public symbol files.

| Screen saver    | File            | Version          | Source directory (original build path)              |
|-----------------|-----------------|------------------|-----------------------------------------------------|
| Aurora          | `Aurora.scr`    | 6.0.6000.16386   | `shell\osshell\control\scrnsave\aurora`             |
| Bubbles         | `Bubbles.scr`   | 6.0.6000.16386   | `shell\osshell\control\scrnsave\bubbles`            |
| 3D Text         | `ssText3d.scr`  | 6.0.6001.18000   | `shell\osshell\control\scrnsave\text3d`             |
| Windows Energy  | `ssBranded.scr` | 6.0.6001.18000   | `shell\osshell\control\scrnsave\branded\src`        |
| Windows Logo    | `logon.scr`     | 6.0.6001.18000   | `shell\osshell\control\scrnsave\logon`              |

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

## Layout

```
CMakeLists.txt                  build (MinGW-w64 or Visual Studio)
cmake/                          MinGW-w64 toolchain file
external/winbrand/              import library definition of winbrand.dll
shell/osshell/control/scrnsave/ the sources, in their original places
tools/extract_assets.py         copies the artwork out of your originals
tools/verify/                   differential tests against the originals
```
