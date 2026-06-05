# ArcadeKart Render Modernization and VFX Notes

This note records the intent and current shape of the render-modernization and post-process VFX pass.

## Design Intent

- Keep the game render and HUD render as separate layers so modern camera/post-process effects can sell speed without warping or blurring HUD readability.
- Make effects per-player-view aware, so split-screen can apply the same system inside each player rectangle rather than across the whole composite.
- Keep the effect stack tunable through CVars and menu controls. Most feel passes should not require recompiling.
- Preserve original game feedback where it works, then layer ArcadeKart effects on top: drift smoke/`E` puffs, boing landing text, minimap behavior, rumble-style events, and track-specific surface feel.
- Prefer normalized inputs for effect drivers: speed ratio, boost amount, roughness, shake impulse, and finish fade all move through 0..1-style values before being shaped.

## Layering Model

- Race scene rendering targets `gArcadeKartPostFxSceneFrameBuffer`.
- HUD rendering targets `gArcadeKartPostFxHudFrameBuffer`.
- The scene framebuffer is post-processed first.
- The HUD framebuffer is composited afterward so HUD elements sit above barrel warp, motion blur, screen shake, camera shake, and overscan.
- DX11 uses an HLSL path in `libultraship/src/ship/window/gui/Gui.cpp` for the scene post-FX shader and HUD composite shader.
- The HUD framebuffer is cleared to a magenta key color. The HUD composite shader keys that color into transparency and uses point-clamp sampling plus edge de-spill to keep white HUD glyphs, minimap edges, portraits, and the RPM meter from picking up magenta.

## Work Done

- Added layered scene/HUD framebuffer setup in the racing framebuffer path.
- Bound the scene framebuffer after race viewport setup because the viewport code can select the normal framebuffer.
- Added a DX11 post-process shader path for:
  - Barrel/fisheye warp.
  - Overscan overdraw.
  - Lightweight radial/motion blur.
  - Screen shake.
  - Per-player split-screen rectangles.
- Added a HUD composite shader that keeps post-FX under the HUD.
- Added fallback ImGui mesh warp path for non-DX shader situations.
- Added safeguards for stale framebuffer CVars; the compositor clears layered-HUD active state when framebuffer texture lookup fails.
- Changed CPU framebuffer readback for jumbotron-style course screens to use the scene framebuffer when layered post-FX is active, so Luigi Raceway and Wario Stadium screens can receive the game image instead of the wrong layer.
- Restored recent drift particle state by resetting texture LUT/alpha state inside the drift particle render path before loading the old I8 drift textures.
- Restored boing/landing-style feedback separately from drift-start behavior.

## Current Effect Drivers

- Speed ratio is based on current speed divided by a boosted top-speed reference, not the current gear cap.
- Speed response uses `gArcadeKart.PostFx.ResponsePower`, currently defaulting to `2.0`.
- FOV narrows from `gArcadeKart.Camera.SpeedWideFov` to `gArcadeKart.Camera.SpeedNarrowFov`, currently `100` to `60`, driven by the same speed curve.
- Camera position shifts from near `45 back / 12 up` to far `60 back / 3 up`, also driven by the same speed curve.
- Barrel warp uses base, speed, and boost terms, then multiplies through `WarpIntensity` and `WarpOutputScale`.
- Warp output scale is currently doubled again to `4.0` after the previous `2.0` pass.
- Screen-shake amplitude is driven by the maximum of direct shake impulse and road roughness response.
- Road roughness response is normalized and formed from:
  - `normalizedSpeed * roadRoughness`
  - `+0.1` while drifting
  - `+0.3` while boosting
- Screen-shake frequency is now driven by inverse roughness:
  - `curve = pow(1.0 - roughness, 0.5)`
  - `activeHz = lerp(8.0, 60.0, curve)`
  - Smooth ground shakes faster; maximum roughness shakes slower and chunkier.
- Post-FX scale fades toward zero when a player finishes, and effects should not run over menus or scoreboards.

## Important CVars

- `gArcadeKart.PostFx.Enabled`
- `gArcadeKart.PostFx.LayerHud`
- `gArcadeKart.PostFx.OverscanPercent`
- `gArcadeKart.PostFx.BarrelStrength`
- `gArcadeKart.PostFx.SpeedBarrelStrength`
- `gArcadeKart.PostFx.BoostBarrelStrength`
- `gArcadeKart.PostFx.WarpIntensity`
- `gArcadeKart.PostFx.WarpOutputScale`
- `gArcadeKart.PostFx.MotionBlur`
- `gArcadeKart.PostFx.ShakeStrength`
- `gArcadeKart.PostFx.ShakeOutputScale`
- `gArcadeKart.PostFx.ShakeResponsePower`
- `gArcadeKart.PostFx.ShakeFrequencyFastHz`
- `gArcadeKart.PostFx.ShakeFrequencySlowHz`
- `gArcadeKart.PostFx.ShakeFrequencyRoughnessPower`
- `gArcadeKart.Camera.SpeedFovEnabled`
- `gArcadeKart.Camera.SpeedWideFov`
- `gArcadeKart.Camera.SpeedNarrowFov`
- `gArcadeKart.Camera.SpeedPositionShift`
- `gArcadeKart.Camera.PositionNearBack`
- `gArcadeKart.Camera.PositionNearUp`
- `gArcadeKart.Camera.PositionFarBack`
- `gArcadeKart.Camera.PositionFarUp`

## Debug Readouts

- The on-screen telemetry block reports active barrel strength, shake pixels, speed curve, roughness input, roughness curve, active shake Hz, FOV target, camera speed reference, and camera position curve.
- Screenshot capture notes live in `docs/codex-session-notes.md`; fullscreen captures need DPI awareness or Windows reports only a scaled top-left portion of the 4K display.

## Guardrails

- Do not composite HUD through the scene shader.
- Do not final-scale the HUD framebuffer for overscan; fix source layout inside the safe zone instead.
- Do not assume framebuffer CVar ids are valid at startup.
- Keep tuning-slider mode isolated from race controls; race menu directions should not secretly mutate post-FX values during normal testing.
- If warp appears too weak, inspect the normalized speed reference and `WarpOutputScale` before increasing the base shader math.
