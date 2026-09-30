# VR upstream refresh - 2026-09-30

## Sources and checkout

- Integration checkout: `E:\SpaghettiKart-VR-refresh`, branch `codex/vr-upstream-refresh`.
- Original checkout: `E:\SpaghettiKart`, branch `VRintegration`, left with its existing uncommitted work.
- HarbourMasters upstream: `2b3ddb4a5`, fetched September 30, 2026.
- ArcadeKart starting point: `9c10b91ad`.
- VR source: https://github.com/LoizouS/MKart64-VR at `6f6ee4e68dd321cf1c7867aef39c088295bdeab7`.
  This preserves RaYRoD-TV's OpenXR fork, including the Quest 3 asymmetric-FOV and course-loading stereo fixes.
- Imported game changes are relative to its base `ed45e1254`; renderer changes are relative to
  libultraship `f5c3843fe937320b64ff754fa6bf71b13ff5e7a1`. Dependencies remain submodules,
  rather than adopting the fork's vendored copies. Its race setup, pause menu and race modifiers
  accompany the VR code. Original source comments and licensing remain intact.
- The older poregon OpenVR fork was inspected, but is not the basis of this integration.

## Work that was in progress

- Per-player first-person view; Blender body and wheels; material groups and steering wheels.
- Wheel/G27 controls, pedals, H shifter, handbrake and force feedback; separate driving/item actions.
- Arcade transmission, custom HUD/RPM faceplate, particles, layered Direct3D post-processing.
- An uncommitted DX11/OpenXR framebuffer-copy prototype in the original checkout. That prototype
  is superseded here by the public fork's per-eye OpenGL/OpenXR rendering, not copied on top of it.
- The uncommitted Blender body exporter and original screenshots remain in the original checkout.

## Camera and integration decisions

- `f7e005569` replaced the original first-person camera orientation with a tyre-derived mesh pose.
  Restored the yaw-based camera function from `21f4e371e` for desktop first person. Mesh orientation
  still belongs to the mesh renderer; it no longer supplies the desktop camera's forward basis.
  This isolates the suspect change; user confirmation of the reported driving/view offset is pending.
- VR keeps the donor's chase-camera-to-eye composition. The arcade first-person command switches
  VR view mode instead of applying a second camera transform. The kart mesh uses the first-person
  visibility path in VR too, except during the donor's external action-camera states.
- Adapted custom GUI, input, particles and transmission code to the new libultraship APIs and include paths.
- Updated the C framebuffer bridge calls to pass libultraship's new `forceFixedAspect` argument explicitly.
- Layered HUD/post-FX is enabled only for desktop DX11. OpenGL draws the game/HUD directly so custom
  offscreen targets do not divert the stereo eye pass. OpenGL HUD positioning still needs tuning.
- Kept ArcadeKart's character/transmission selection and added the donor's optional rival-slot handling.

## Compile and run

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64 -DENABLE_VCPKG=OFF -DCMAKE_TOOLCHAIN_FILE=E:/SpaghettiKart/build/x64/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build/x64 --config Debug --target Spaghettify --parallel 8
```

The executable is `build/x64/Debug/Spaghettify.exe`. Run from that directory with `--vr` for
OpenXR/OpenGL, or `--novr` for desktop. Without either flag the donor probes for a headset.
The OpenXR runtime must already be selected and the headset connected for stereo playback.

Runtime assets, saves, mods and controls were copied from the old Debug directory into this checkout.
`spaghetti.o2r` was repacked from the refreshed assets with the existing Torch executable.
Original runtime files were not modified. `spaghettify.cfg.json.before-vr-test` preserves the
new checkout's pre-test settings. This machine's vcpkg cache/toolchain was reused.

## Verification and remaining checks

- Debug compilation passed with the Windows MSVC toolchain.
- DX11 desktop smoke test reached a race with the arcade HUD.
- OpenGL desktop fallback reached a race with first-person geometry and a centered starting-straight view.
- OpenXR detected SteamVR, then `xrCreateSession` returned `XR_ERROR_RUNTIME_FAILURE`.
  SteamVR's `xrclient_Spaghettify.txt` identifies the underlying error as `VRInitError_Init_HmdNotFound`.
  Desktop fallback remained responsive. Connect the headset, then restart with `--vr`.
- No active headset session has been verified. Stereo comfort, recentering, head tracking, wheel/pedal
  hardware, moving-camera alignment and VR-plus-local-split-screen behavior require hands-on testing.
- HUD faceplate/placement needs visual follow-up, especially on the direct OpenGL path.
- The copied nested generated headers under `include/assets/assets` are untracked and not part of the commits.
- Integration commits are local. Publish the libultraship branch before publishing a parent commit that pins it.
