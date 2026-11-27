# How the Mystify and Ribbons screen savers work

A behavioural description of the two Direct3D 9 screen savers that ship with
Windows (Mystify and Ribbons, written by Stephen Coy), based on observing the
shipped binaries. This is a description of *how they behave and how they are
put together* — the design, the maths, and the settings — not a copy of their
code. It exists so the technique can be understood and reimplemented cleanly;
the original screen saver in [`original/filaments`](../original/filaments) is
one such clean implementation in the same spirit.

No Microsoft code, symbols, binaries, or artwork are included here.

## The two savers at a glance

Both are small fixed-function Direct3D 9 programs built on the same screen
saver framework (the one the NT/Vista-era D3D savers use): a base class that
owns command-line parsing (`/s` run, `/c` configure, `/p` preview), window and
device creation, multi-monitor enumeration, the configuration dialog, and the
message loop, plus a per-saver subclass that supplies the three hooks that
matter — build device resources, advance one frame, and draw one frame.

They differ only in the moving object:

* **Mystify** draws a handful of **polylines** (the settings call them
  "lines") whose vertices chase each other around the screen — a 3D descendant
  of the classic 16-bit Mystify.
* **Ribbons** draws **wide triangle-strip ribbons** that leave long sweeping
  curves through the view.

Everything else — the camera, the trail/blur, the multi-monitor handling, the
settings plumbing — is shared in spirit between them.

## The frame pipeline

Each frame does roughly this:

1. **Advance the camera** (see below) and advance every line/ribbon by one
   step.
2. **Render into an off-screen texture, not straight to the screen.** The
   saver keeps **two screen-sized render-target textures** and ping-pongs
   between them. At the start of a frame it draws the *previous* frame's
   texture back into the current one through a full-screen quad that is
   slightly darkened (a constant per-frame attenuation). That feedback is what
   produces the characteristic **glowing trails / motion blur**: anything drawn
   fades over several frames instead of disappearing immediately. Ribbons
   exposes this as an "Apply Blur and Fade" checkbox; Mystify always does it.
3. **Draw the geometry additively** on top of the faded feedback, so
   overlapping strokes accumulate into brighter light.
4. **Copy the current texture to the back buffer** with a final full-screen
   quad, then present. The two textures swap roles for the next frame.

The full-screen quad used for both the feedback and the final copy is a single
four-vertex triangle fan in screen space (transformed vertices with a diffuse
colour and one texture coordinate). When the hardware needs power-of-two
textures, the saver rounds the render-target size up to the next power of two
and scales the quad's texture coordinates so only the used sub-rectangle shows.

## The wandering camera

The camera does not follow a scripted path; it **drifts through a 3D noise
field**. Its position is advanced by a small per-axis velocity that is bounced
back when it reaches a soft boundary (with a fresh random speed chosen on each
bounce), so the viewpoint meanders without ever leaving a box around the
origin. The point the camera looks at, and its "up" vector, are derived by
sampling a **value-noise turbulence function** at the camera's position (and at
a couple of fixed offsets from it) and scaling the result. Because the look-at
is a noisy function of position rather than a fixed target, the view tumbles
gently and never repeats.

The projection is a standard right-handed perspective transform. Its field of
view is a setting (roughly 0.5 to 3.0 radians). Near and far planes are fixed.

## The lines (Mystify) and ribbons (Ribbons)

Both objects are built the same way: a **ring buffer of recent sample points**
that forms a trail. Each step, the newest point is computed and the oldest is
discarded, so the stroke slides forward through space.

* The **head** of each stroke moves with the same bounce-in-a-box motion the
  camera uses — a per-axis velocity reflected at soft limits, with new random
  speeds on each bounce — plus a turbulence term sampled from the noise field,
  so the paths curl organically.
* **Mystify** connects the trail points with a thin line/narrow strip; a line's
  colour is taken from a per-line base colour. The number of lines (1–20) and
  the line width (about 0.01–0.1 of the view) are settings.
* **Ribbons** turns each trail into a **camera-facing triangle strip**: at every
  trail point it offsets left and right along the vector perpendicular to both
  the segment direction and the view direction, producing a flat band that
  always faces the camera. Vertex colour varies along and across the ribbon
  (the ribbon fades toward its tail and its edges), and vertex alpha tapers so
  the strip melts into the glow rather than ending in a hard edge. The number
  of ribbons (1–250) and the ribbon width (about 0.01–2.5) are settings.

A small **texture** (a soft blob, stored as a Targa resource inside the binary)
is bound while the strokes are drawn, so each segment is a smooth gradient
rather than a flat quad; combined with additive blending this is what makes the
strokes read as light.

## Ribbons' presets

Ribbons' configuration dialog offers four named **presets** — described in its
UI as things like "bold streaks of twisting light," "sparse, delicate curves,"
"tangled threads woven into cloth-like textures," and "soft washes of colour
overlaid with stars." Each preset is just a bundle of the underlying settings
(ribbon count, width, field of view, blur on/off, and the colouring/▒motion
constants) chosen for a particular look; selecting one fills in the individual
controls.

## Multiple monitors

The framework enumerates monitors and can either treat the whole desktop as one
continuous scene (the "Span Multiple Monitors" option) or run an independent
instance per monitor. When spanning, each monitor's device renders the same
world but with its projection offset and scaled to that monitor's rectangle
within the whole virtual desktop, so a ribbon crosses the seam between screens
continuously. The per-monitor device resources (the render-target textures and
the vertex buffers) are stored in small per-device arrays and selected by a
"current device index" before each device draws.

## Settings

Both savers read their settings from the registry under
`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Screensavers\`
(a `Mystify` or `Ribbons` subkey), as `REG_DWORD` values:

| Meaning | Mystify | Ribbons |
|---|---|---|
| Number of strokes | `NumLines` 1–20 | `NumRibbons` 1–250 |
| Stroke width | `LineWidth` ~0.01–0.1 | `RibbonWidth` ~0.01–2.5 |
| Camera field of view | `CameraFOV` 0.5–3.0 | `CameraFOV` 0.5–3.0 |
| Span all monitors | `SpanMultiMon` | `SpanMultiMon` |
| Blur / fade trails | (always on) | `Blur` |

Width and FOV are stored as the bit pattern of a 32-bit float. Each value is
clamped to its range on load.

Depending on the build, the configuration dialog is either the full settings
dialog (sliders for count, width and FOV, the span and blur checkboxes, and —
for Ribbons — the preset buttons, laid out in the saver's `.mui` resource) or a
simple "this screen saver has no options" task dialog.

## The Windows 10 build

The Windows 10 binaries additionally wrap the run in the standard Windows
telemetry/error-reporting helpers (WIL + TraceLogging): a logging provider is
started around the saver's lifetime and reports a "screen saver displaying"
activity. This is instrumentation around the saver and is unrelated to the
visuals; a clean reimplementation simply omits it.

## What makes the look, in one paragraph

Glowing strokes that trail and fade come from three cheap ingredients working
together: **additive blending** so strokes add light, a **soft blob texture** so
each stroke is a gradient rather than a slab, and a **feedback buffer** that
re-draws a slightly darkened copy of the previous frame every frame so motion
smears into comet-like tails. The organic, never-repeating motion comes from
driving both the camera and the strokes with **value-noise turbulence** plus a
**bounce-in-a-box** velocity model. None of these pieces is complicated on its
own; the appeal is in the combination. The original saver in
[`original/filaments`](../original/filaments) is built from exactly these
ingredients, with its own code and its own choices.
