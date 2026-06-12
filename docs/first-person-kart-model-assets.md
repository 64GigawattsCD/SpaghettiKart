# First-Person Kart Model Assets

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
