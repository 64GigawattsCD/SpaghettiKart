# Upstream and VR refresh: 2026-10-13

## Checkout and preserved work

- Integration: `E:\SpaghettiKart-VR-refresh`, branch `codex/vr-refresh-oct13`.
- Starting parent revision: `31dfa8c8e`; previous integration remains on `codex/vr-upstream-refresh`.
- Original `E:\SpaghettiKart` / `VRintegration` stays with its existing WIP. Only a session-note pointer was added there.
- Existing kart model research edits and untracked `include/assets/assets/` remain outside this update.
- No kart assets, camera heading basis, wheel mappings, transmission, or force-feedback tuning changed.
- All commits remain local. Publish the libultraship branch before the parent branch if publishing later.

## Fetch and upstream

Both parent checkouts had their configured remotes fetched. Added `vrmicelli` and `quest-vr` remotes to the shared parent repository, and `vrmicelli` to the integration's libultraship checkout.

Final `git fetch --all --prune --tags --no-recurse-submodules` completed for the parent and integration renderer. The first recursive fetch encountered a historical donor gitlink absent from its declared renderer remote; fetching the relevant renderer revision from `gnatman/libultraship` succeeded. We did not switch submodules to donor pins.

Merged HarbourMasters `5073d2e3636cdb2794f4c845ee3bdec5ac4718c4`, the only new main-branch commit since September 30. It removes the unused jitter-fix slider (PR #767).

## VR sources reviewed

1. [VRmicelliKart](https://github.com/gnatman/VRmicelliKart) at `7c222d7d70`: active October work on DirectX VR, procedural cockpit geometry, wheel/FFB, telemetry, sky and stereo. No published GitHub release was listed at inspection. Its camera and HUD architecture differs from our OpenGL/OpenXR path; hardware operation was not verified locally.
2. [SgtBilko76/SpaghettiKart-VR](https://github.com/SgtBilko76/SpaghettiKart-VR/tree/spaghettikart-vr), VR change `467694e378`, tip `0b5ab4eeaf`: standalone Android/OpenXR. [v0.2.0-beta1](https://github.com/SgtBilko76/SpaghettiKart-VR/releases/tag/v0.2.0-beta1) states Quest 3 testing and untested PICO support. Android packaging and its independent headset renderer were not imported into our Windows branch.
3. [RaYRoD MVRH patch distribution](https://github.com/RaYRoD-TV/MVRH/tree/gh-pages): `MarioKart64-VR-8.zip` SHA256 `1b35c1c171c4b388f891fc8bbfa346ab41cfa9297fdf66bd6344c6b99a0c8e12`. Fresh download exactly matches our September archive despite the October catalog commit. LoizouS source still reports its last push as August 7; our existing RaYRoD integration was retained.
4. [poregon/SpaghettiKart-VR](https://github.com/poregon/SpaghettiKart-VR): older DirectX/OpenVR source, last push July 2025. No newer change selected.
5. [ArtfulRascal8/XRKart-64](https://github.com/ArtfulRascal8/XRKart-64): standalone APK-focused project, last push July 29. No Windows source update selected.

## Assimilated changes

- VRmicelli `a632be01c9`: inclusive I4 texture tile bounds in both menu glyph functions and 3D text, eliminating the extra two-pixel row stride described by the donor.
- VRmicelli `63f4a343c0`: wrapped binary-angle interpolation for position/rotation matrices, billboard angles and actor transforms. Uses the existing quarter-turn discontinuity snap behavior. A small pure helper permits regression testing; narrowing goes through an integer before wrapping at 65536.
- SgtBilko `467694e378`: palm-tree interpolation IDs advance for hidden and culled entries, so IDs remain stable as visibility changes.
- Renderer `gnatman/libultraship` `925285dea108bc5501959adc77d69b6da8f1cca4`: refresh the DirectX depth comparison when decal mode changes. Local renderer commit: `f848ad4a`.

The donor's stereo HUD flush change applies to its separate HUD framebuffer pass. Our existing `RenderVrTarget` flushes the sky and completed eye passes and does not use that HUD target-switch path, so that code was not copied. Our sky dome already exists. The donor's camera-axis inversion, procedural cockpit, motion-rig telemetry, and direct-drive force changes were left separate from our camera/Blender/G27 work.

## Verification

- MSVC Debug compilation passed: `cmake --build build/x64 --config Debug --target Spaghettify --parallel 8`.
- Regression suite: configure `cmake -S tests/vr_refresh -B build/vr-refresh-tests -G "Visual Studio 17 2022" -A x64`, compile that directory, then `ctest --test-dir build/vr-refresh-tests -C Debug --output-on-failure`.
- Binary-angle test passed 262158 checks, including 0/65536 wrap, signed 180-degree wrap, both directions, endpoints, and large-angle snapping.
- Desktop OpenGL and DirectX startup checks used separate runtime directories under `build/vr-refresh-smoke`, copied asset archives, and fresh settings. Both processes remained responsive and loaded track metadata. Logs showed race setup and no error/critical entries in the initial checks.
- User requested muted background tests during this session. The helper now sets master/music/SFX/environment volumes to 0.0 before launch, never editing normal play settings. All subsequent background tests must be muted.
- Muted reruns passed for OpenGL and DirectX, 30 seconds each; all four volume settings remained zero in the saved test configurations. Test processes were closed afterward.
- These are startup/log checks, not visual gameplay or headset validation. No active headset session, stereo comfort, head tracking, recentering, or wheel-hardware test was completed here.
- Existing user checks remain: first-person heading while driving, OpenGL HUD/RPM placement, headset stereo, and Blender mesh replacement. The imported angle fix addresses interpolation crossings, not a constant camera-to-driving offset.

## Usage observation

Account snapshot at start: weekly used 73%, credits 0. Later snapshot: weekly used 80%, credits 0. These are account-wide observations, not exact per-task billing. No reset redeemed or paid service invoked.
