# Kart Asset Tooling

Project checkout: `E:\SpaghettiKart-VR-refresh`.

Follow-up: our authored runtime cockpit and direct game tests are documented in
`docs/cockpit-and-autonomous-tests.md`. The generic staged workflow below remains
separate from that explicitly integrated model.

This prepares our own models for static F3DEX export. It does not install a new
kart in the game or import Amped Up artwork. Camera, physics, meshes currently
compiled into the game, and the original checkout remain unchanged.

## Installed Pieces

- Existing Blender 5.2.2 LTS; no global installation or preferences changed.
- HarbourMasters Fast64 2.5.4: geometry, F3D materials and static C export.
- Litronom/Robichu population exporter: course paths and placements, not racer rigs.
- Local reference sources: OverKartDecomp, Tarmac64 and the original Library.
  These are not compiled or linked into SpaghettiKart.

`tools/kart_assets/sources.json` pins exact repository revisions and source URLs.
Checkouts and project Blender settings live in `.cache/kart-tools`, outside Git.
Keep third-party source notices intact. See the manifest for license status;
the reference repositories are not blanket permission to redistribute adaptations.
No Amped Up models, textures, animations or ROM patch are used by this workflow.

## Start Blender

Run `open-kart-blender.cmd` from the checkout. It opens a separate Blender session
with Fast64 and the population exporter active and the empty starter scene:
`build/kart-tools/starter/kart_authoring.blend`.

The scene has export/reference collections, pivot markers and six F3D material
roles. It contains no kart or character geometry. Keep newly sourced originals
separate from working copies. Use Blender's ordinary import tools for supported
formats, then prepare export meshes in `KART_EXPORT`.

The launcher uses a project-only profile and disables embedded script autorun.
Normal Blender launches retain the user's existing tools/preferences. Files that
use these F3D materials should be opened through this project launcher.

## Model Contract

- Authoring: +X lateral, -Y forward, +Z up. One Blender unit equals one game unit;
  final model fit is deliberately deferred until we have the actual assets.
- Export: `(x, y, z)` becomes `(x, z, -y)`, with +Y up and +Z forward.
  This rotation preserves handedness. It is NOT the old wheel script's axis swap.
- Each export object is a separate rigid part. Use ASCII identifier names, apply
  rotation/scale, remove parenting, and place the object's origin at its pivot.
  Object location is written as a converted pivot in the manifest, while geometry
  is exported locally around that pivot. There is no automatic wheel placement.
- Every material slot must use Fast64 materials; a UV map is required. Blender
  materials alone are not sufficient. Fast64 has its own material conversion UI.
- Keep rigs in `KART_REFERENCE`. Static export rejects armature modifiers; no
  skeletal clip exporter or racer animation state machine is connected yet.
- This outputs staged C geometry, not the current fallback's symbol/material
  contract and not an `.o2r` package. Runtime installation is a separate step.
- Runtime material recoloring and render-state restoration need explicit handling
  when a model is integrated; material role names alone do not implement them.

## Repeatable Commands

From the checkout, with Python and Git on PATH:

```powershell
python tools/kart_assets/toolchain.py setup
python tools/kart_assets/toolchain.py doctor --out build/kart-tools/doctor-new
python tools/kart_assets/toolchain.py template --out build/kart-tools/new-starter
python tools/kart_assets/toolchain.py open --blend C:/assets/my-kart.blend
python tools/kart_assets/toolchain.py validate --blend C:/assets/my-kart.blend --out build/kart-tools/inspection-new
python tools/kart_assets/toolchain.py export --blend C:/assets/my-kart.blend --out build/kart-tools/export-new
python tests/kart_assets/test_toolchain.py
```

Set `BLENDER_PATH` or use `--blender` on another machine. Output directories must
be new. Exports do not save the input scene. Each batch records the command/log;
validation writes inspection.json, and export adds manifest.json plus one C/header
pair per part. The manifest explicitly records that the runtime is not installed.
Setup is repeatable and stops if a pinned checkout has tracked local edits.

## Verification and Known Gaps

Fast64's README lists support through 5.1.2. The inspected functions have also
passed our Blender 5.2.2 batch checks; this is not a full-addon compatibility claim.
The legacy population success popup needs a GUI. Our wrapper logs that message
in background jobs without editing the downloaded source.

The functional fixture is our own triangle and two-point course path. Tests cover
materials, empty scenes, unapplied/mirrored transforms, coordinate handedness,
actual exported vertex coordinates, pivots, C output and a real population export.
Regression tests reopen a saved .blend, compare repeat exports, hash the source
before/after, and check rejection of existing output directories.
All five regression tests passed on 2026-10-13, including the eleven Blender
functional checks. To stabilize Fast64's set-based flag serialization, the
launcher clears inherited PYTHON variables, disables user site packages and
enables a fixed Python hash seed in Blender. Downloaded addon files stay intact.

No game or headset test is implied by these checks. Any future background game
tests must be muted. Skeletal driver animation and textured production-model QA
remain future work with our own assets.
