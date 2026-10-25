# Android Touch Art

## Visual Direction

Current pedal layout is a Simon-style four-sector ring around the accelerator.
Clockwise from the top: violet Jump, blue Drift, red Brake, gold Item. The pedal
icons remain; dark outlines, colored edges and bright held sectors match the
existing palette. Camera look is beside the minimap with rear view above it.
The manual gate is left of the radial cluster, not under the pedal actions.

Gameplay controls now render only during an active human race (including the
countdown). Menus and pause use native tappable choices instead of driving
buttons. View Options and Race Setup show six rows per page, with stock-font
labels, gold adjustment/page arrows, and explicit Back/Resume/Start actions.
The transmission picker uses a centered AUTO/MANUAL modal. Menu transitions
clear all held touch controls before the overlay returns.

The stock race HUD is the reference: saturated frame accents, dark outlines,
light upper edges, gold/orange active numbers and lavender idle numbers.
The existing action symbols remain recognizable and have no controller glyphs.
Control centers remain translucent; the central driving view has no panel.

- Green accelerator with a tall ribbed pedal, red brake with a wide pedal, blue
  drift, gold item, violet pause, silver camera. Both pedals are original Blender
  sprites; the other action symbols remain Lucide.
- Independent blue camera stick with Lucide eye and an original rear-view mirror
  icon. The stick uses existing first-person look axes and recenters on release;
  hold the mirror for180 degrees behind. It is not a live mirror render target.
- Blender-rendered beveled rims, full-size steering wheel and gold shift knob.
  The silver inner ring follows the thumb; the fixed-center outer wheel turns
  up to50 degrees each way with horizontal stick input and recenters on release.
- Dark recessed gate with a continuous bevel and bright centerline. The line
  turns gold during clutch interaction; pressed buttons brighten and depress.
- Unchanged positions, hit targets, 20% shorter lever travel, widened slot entry,
  clutch-on-touch, independent steering and controls-inward/HUD-outward default.
- Gamepad maps and native transmission/network behavior are untouched.
- The shared minimap ScaleBox now fits the full gap between the corner blocks,
  centered within that gap on the outer edge. Previously a symmetric reserve
  copied the larger RPM block above the map, wasting height; down-only scaling
  also stopped small maps filling the slot. Aspect ratio and marker padding are
  retained. Luigi Raceway map height on the phone changes54.04 to88.18 HUD units.

## Source And Reproduction

`tools/kart_assets/render_touch_art.py` is the original Blender source. Run with
the local blender-local batch helper into a new output directory, inspect the
PNGs, then copy the ten `touch_*.png` files to
`android/app/src/main/res/drawable-nodpi`. The current render used Blender5.2.2,
Cycles24 samples,128px RGBA, orthographic camera and transparent film.
The original scene is in `build/touch-art/blender-v1`; the pedal revision and
render receipt are in `build/touch-art/pedals-v1`.
No external 3D assets are included. Lucide symbols retain their bundled ISC
notice in `android/app/src/main/assets/licenses/lucide.txt`.

ComfyUI supplied a single local color/style study, not production icon art.
Its ambiguous symbols were rejected; the precise existing action symbols remain.
Graph and receipt: `F:/AI/Workflows/projects/spaghetti-touch-20261016`.
Pixel Art Diffusion XL Sprite Shaper, seed641016,1024x768,28steps,CFG6,
DPM++2M/Karras. First submission stopped in torch checkpoint loading with a
Windows access violation; the existing managed launcher was restarted unchanged.
Retry `0d3d0fa0-0a6c-4c87-960f-b546627dae1c` completed in32.567s and was inspected.
No weights/extensions were installed. Meshy was unnecessary for these simple
authored sprites; Meshy credits and external image-service charges:0.

## Verification

`tests/android/touch-preview` compiles the production overlay/resources into an
emulator-only fixture, separate from the game package. It covers five landscape
sizes, both transmission modes, both HUD layouts, app-owned input gestures,
held/idle/clutch visuals, transparency and viewport fit. It does not test native
gameplay. `tests/kart_assets/check_touch_art.py` checks those PNGs and optionally
composites them onto a previously recorded game frame for visual comparison.
Such composites are art previews, not new phone gameplay screenshots.

Full phone/native transmission coverage stays in
`tests/kart_assets/test_phone_shifter.py`. All background game runs remain muted.
`test_phone_camera.py --serial DEVICE` checks native look axes/rear priority,
return-to-look/forward, driving isolation, touch checks and settings preservation.
`--desktop` exercises the shared native camera when the phone is asleep; it does
not certify the Android native merge. Icons are checked in isolation for visible
face pixels and (for pedals) dark tread pixels, not pure-white pixels over rims.
