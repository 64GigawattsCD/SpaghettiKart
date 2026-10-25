# First-Person Kart Model Assets

## Integrated Cockpit: 2026-10-13

The active runtime now includes `src/first_person_cockpit.inc.c`, an own-authored
5,992-triangle pipe-frame kart using the previous draft's proportions. Eight
character colors, moving steering/wheels, desktop first-person and shared VR
first-person draw path. See `docs/cockpit-and-autonomous-tests.md` for source,
renderer captures, native input queues and verification. The older generated
body/wheel includes discussed below are preserved but no longer active.

## Tooling: 2026-10-13

User plans to source our own models. The isolated Blender/Fast64 starter and
staged static exporter are documented in `docs/kart-asset-tooling.md`.
No candidate artwork below has been imported by this setup. No runtime mesh or
camera changes were made. Animated driver integration is still pending.

## Model research: 2026-09-30

Research only; no downloaded model internals inspected and no runtime assets or camera code changed.

- First candidate: fznmeatpopsicle's custom MK64 kart. Listing contains Collada DAE meshes for all eight characters, a `pipeframe64_high.dae`, PNG materials, and `credits.txt` (7.07 MB archive). Inspect credits, triangle counts, cockpit-facing surfaces, and wheel separation before porting.
  https://models.spriters-resource.com/custom_edited/mariocustoms/asset/322711/
- Matching low-poly edition: eight DAE character variants and textures (107.95 KB archive). Potential lightweight option; archive size is not a triangle count.
  https://models.spriters-resource.com/custom_edited/mariocustoms/asset/322712/
- SAB64 Pipe Frame: creator lists BLEND, FBX, and PMX, based on the Melee kart with optional MK7-style additions. Creator notes non-MMD editions may not work as intended. Nintendo-derived parts; inspect included terms before distribution.
  https://www.deviantart.com/sab64/art/MMD-FBX-Model-Pipe-Frame-Download-861593760
- MK7 Pipe Frame: DAE body and LOD meshes with character textures. Wheel assets need separate investigation.
  https://models.spriters-resource.com/3ds/mariokart7/asset/340132/
- Existing port: LTC 91's Tour Pipe Frame for Wii replaces all character karts, including menu/multiplayer and Battle variants; v2.0 dated 2024-12-26. Not a SpaghettiKart asset pack.
  https://mkwiiki.org/wiki/Pipe_Frame_(LTC_91)
- Existing VR cockpit: heurazy's Wii VR v0.5.0 release describes a camera at the driver's authored head position, local-driver hiding while retaining the kart, and impact rotation stabilization. Useful reference, separate engine integration.
  https://github.com/heurazy/mario-kart-wii-VR-port/releases/tag/v0.5.0

Preferred next investigation: inspect the detailed fznmeatpopsicle mesh beside our current kart in Blender, including the seated view. Retain the established driving-direction camera basis independently of imported mesh axes.

Credit observation at research close: account balance 1879.9243935000. No opening snapshot for this research turn, so per-task consumption is unknown. No reset redeemed.

## Current branch path

The first-person kart is currently compiled into the game as generated F3D-style display lists:

- `src/first_person_kart_mesh.inc.c`
- `src/first_person_kart_wheels.inc.c`

The Blender exporters split the meshes into material display lists so the game can set primitive colors at draw time:

- body dark detail
- body silver metal
- body blue accent
- body character recolor region
- wheel tire
- wheel hub

This keeps the first-person renderer simple while we are still proving scale, orientation, camera placement, and steering behavior.

## Spaghetti Kart mod path

Spaghetti Kart loads custom assets from `.o2r`, `.zip`, or loose folders in `mods/`. The official docs describe custom tracks as the complete 3D asset path today. Custom characters are still sprite replacement based, and the docs list 3D character models as future work.

Important references:

- https://github.com/HarbourMasters/SpaghettiKart
- https://harbourmasters.github.io/SpaghettiKart/export.html
- https://harbourmasters.github.io/SpaghettiKart/import.html
- https://harbourmasters.github.io/SpaghettiKart/characteroverview.html

## Existing code hooks

The engine already registers Fast resource factories for vertices and display lists in `src/port/Engine.cpp`.

Custom tracks use a `data_track_sections` resource to name display-list CRCs, then draw those display lists through the resource manager. See `src/engine/tracks/CustomTrack.cpp`.

Static mesh actors also already carry a model resource name, but their draw path currently treats the model string as a display-list pointer. That likely works only when the string is an OTR/O2R signature path that the display-list interpreter recognizes, not as a general first-person kart model registry.

## Longer-term plan

The likely clean path is to keep using Fast `DisplayList` and `Vertex` resources, but add a small first-person kart asset definition instead of hard-coding generated includes.

Suggested package layout:

```text
mods/my-fp-kart.o2r
mods.toml
first_person_karts/mk64_mario_kart/model.json
first_person_karts/mk64_mario_kart/body_recolor
first_person_karts/mk64_mario_kart/body_silver
first_person_karts/mk64_mario_kart/body_dark
first_person_karts/mk64_mario_kart/body_blue
first_person_karts/mk64_mario_kart/front_wheel_tire
first_person_karts/mk64_mario_kart/front_wheel_hub
first_person_karts/mk64_mario_kart/rear_wheel_tire
first_person_karts/mk64_mario_kart/rear_wheel_hub
```

The JSON should define resource names, scale, axis conventions, and material roles. The renderer would resolve each display-list resource with `ResourceGetDataByName` or `ResourceLoad`, then draw it using the same pose and primitive-color code used by the compiled fallback.

## Next engineering steps

1. Export a minimal display-list/vertex pair into a loose mod folder and load it by name from code.
2. Add a first-person kart asset descriptor loader with a compiled fallback.
3. Move the current generated meshes behind that descriptor shape.
4. Teach the descriptor how to map the recolor material to selected character colors.
5. Once this is stable, decide whether the Blender exporter should target XML resources, binary Torch resources, or checked-in C includes for development builds.
