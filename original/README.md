# Original screen savers

Screen savers written from scratch for this repository — **not** reconstructions
of anyone else's binaries. All the code here is original.

## Filaments

A Direct3D 9 screen saver: glowing strands are carried through a slowly
evolving 3D flow field and leave fading trails, while the camera drifts around
them. There are two selectable looks — thin **lines** or wide camera-facing
**ribbons** — chosen in the settings. It is built from the same handful of
ideas that make the classic 3D "ribbon" savers look the way they do — additive
blending, soft falloff, and a feedback buffer for the trails (described in
[`../docs/mystify-ribbons-notes.md`](../docs/mystify-ribbons-notes.md)) — but
the design, the maths and the code are my own:

* **Motion** — each filament's head is pushed by a *curl-noise* flow field
  (`flowfield.cpp`): the curl of a vector potential built from hash-lattice
  value noise, which gives a swirling, divergence-free current that never
  repeats. A gentle pull toward the origin keeps the filaments on screen. Each
  filament keeps a short ring buffer of its recent positions as its trail.
* **Geometry** — every trail is drawn as a camera-facing triangle strip whose
  colour fades from head to tail, added onto the frame so overlaps glow.
* **Trails** — the frame is rendered into one of two off-screen textures; each
  frame the previous one is drawn back slightly darkened, so motion smears into
  comet tails. No textures or images are needed or shipped.
* **Colour** — an HSV ramp, optionally cycling across the whole set.

There is a small settings dialog — shape (lines or ribbons), number of
strands, strand width, camera field of view, trail length, span-all-monitors,
and a colour-cycling toggle — stored under `HKCU\Software\D3DXEngine\Filaments`.
"Lines" and "ribbons" share one renderer and differ only in width scale, so
every setting applies to both; "span all monitors" makes the full-screen run
cover the whole virtual desktop instead of the primary monitor.

### Building

A standalone CMake project (it does not depend on the rest of the repository):

```bash
cmake -S original -B build-original -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-w64-x86_64.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build-original
```

The result is `Filaments.scr`. On Windows, right-click it and choose **Install**,
or copy it into `%WINDIR%\System32`, to use it as a screen saver. It links only
`d3d9`, `comctl32`, `gdi32`, `user32` and `advapi32` — no D3DX, no DirectX SDK.

### Files

| File | What it is |
|---|---|
| `filaments/filaments.cpp` | the saver: command line (`/s` `/p` `/c`), window + device, run loop, rendering, settings, dialog |
| `filaments/flowfield.h/.cpp` | value noise and the curl-noise flow field |
| `filaments/vecmath.h` | small left-handed vector/matrix helpers and HSV→colour |
| `filaments/filaments.rc`, `resource.h` | settings dialog, version info, strings |

It has not been run here (this environment has no GPU or Wine); it builds
cleanly as an x64 Windows screen saver with MinGW-w64.
