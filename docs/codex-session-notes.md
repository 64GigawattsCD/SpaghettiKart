## Screenshot Capture

- For fullscreen SpaghettiKart captures, call `SetProcessDPIAware()` before reading screen or window bounds.
- Without DPI awareness, PowerShell reports the 4K primary as 2560x1440 and captures only the top-left portion of the window.
- For DirectX fullscreen, prefer screen capture with `System.Drawing.Graphics.CopyFromScreen`; `PrintWindow`/BitBlt window copies can return black frames.
- Save screenshots under `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\screenshots\...` and embed them in the reply with an absolute Markdown image link so they appear in the Codex sidebar.

## RPM Meter

- The RPM faceplate is supplied by `mods/zz-arcadekart-rpm-faceplate.o2r` as `textures/common_data/common_texture_speedometer.png`.
- The faceplate art owns the orange warning region now; do not add runtime color masks over the gauge face.
- The root `mods/zz-arcadekart-rpm-faceplate.o2r` and `build/x64/Debug/mods/zz-arcadekart-rpm-faceplate.o2r` copies both need the forward-slash archive entry `textures/common_data/common_texture_speedometer.png`; backslash entries lose to the HD pack on runtime lookup.
- The custom source art is 256x384, aspect 0.666667. Keep the in-game faceplate quad at 64x96 local units (`-32..32`, `-48..48`) with 4096x6144 texture coordinates; the previous 63x94 mesh was aspect 0.670213, about 0.53% horizontally wide.

## HUD Layout

- HUD positioning now uses an explicit layout safe zone: the root player view stays full-size for scale math, while selected canvas slots anchor inside `HudLayoutContext.safeRect`.
- Scale controls belong at visible cluster boundaries: placement number, portrait strip, time/lap strip, lap-time list, minimap/dots, and RPM meter each have their own scale CVar. Leaf renderers must consume their arranged rects for the scale boxes to affect pixels.
- RenderModernize HUD visual target as of 2026-06-01: placement high top-left, portrait strip below/left, time/lap centered high, vertical lap split list top-right, minimap mid-right, RPM meter bottom-right. Latest useful full capture: `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\screenshots\render-modernize-layout\RenderModernize-layout-final-pass-20260601-152150.png`.
- The one-player minimap currently uses explicit center CVars (`gArcadeKart.Hud.MinimapOnePlayerX`, `gArcadeKart.Hud.MinimapY`) so the RPM meter can align to the same right-side cluster while RenderModernize overscan is being tuned.
- Portrait strip now uses safe-zone anchoring for center-left placement and has a new `gArcadeKart.Hud.PortraitStripCenterYOffset` control for quick visual nudging.
- HUD micro-pass completed: placement left/top offsets moved inward, portrait strip edge offset increased, map vertical/padding defaults moved inward slightly, and top/lap cluster defaults re-centered to reduce manual slider churn.
- RenderModernize HUD cluster scale defaults are 0.8 for portrait strip, time/lap strip, lap-time list, minimap/dots, and RPM meter; each cluster keeps its existing safe-zone anchor while shrinking.

## HUD Timer/Lap

- `func_8004EB38` was the legacy translucent afterimage pass for `TIME` and `LAP`; keep it disabled while the ArcadeKart HUD owns the race timer/lap presentation.
- The top-center timer/lap strip uses native sprite rectangles for `TIME`, timer digits, `LAP`, and `1/3`; keep those as separate layout children so spacing can be tuned without distorting glyph proportions.
- The top-right lap split list must use `hud_layout_canvas_safe_slot` and the scaled timer glyph renderer. The legacy `print_timer` path ignores the intended layout width/right edge in widescreen and can clip against the right side.

## Minimap

- `draw_minimap_character` tints the neutral minimap progress dot from an explicit character palette; first place still uses `func_8004C450` for the existing rainbow flash.

## Controller Sequential Shift

- `Shift Gear Up` and `Shift Gear Down` are dedicated bindable commands.
- Default controller flow is right-stick click for `Clutch`, then right-stick up/down for sequential upshift/downshift; sequential shifts are ignored unless the clutch command is active.
- Keep the running controls list in `docs/control-change-log.md`.
- `Toggle HUD` now uses `KART_TOGGLE_HUD_BUTTON`; `BTN_CRIGHT` is legacy only, so stale right-stick-right/C-right mappings do not hide the HUD.

## Transmission Mode

- Manual/automatic selection should first land in the ArcadeKart options near clutch, shifter smoothing, and gear tuning as a `Transmission Mode` option. Longer term it belongs in race setup/character select as a driving preference; AI should remain automatic until AI manual-shift behavior is explicitly authored.

## Render Modernize

- The race scene now renders into `gArcadeKartPostFxSceneFrameBuffer`; HUD rendering switches to `gArcadeKartPostFxHudFrameBuffer` afterward so post effects sit under HUD.
- The DX11 path uses HLSL pixel shaders in `Gui.cpp`: one for scene barrel/overscan/blur/shake, and one to composite the HUD framebuffer by keying the magenta clear color into alpha.
- The HUD framebuffer does not carry useful alpha when sampled by ImGui; plain alpha blending turns its clear color opaque. Keep the keyed HUD shader unless the framebuffer backend starts preserving alpha. The HUD compositor must use the point-clamp sampler and neutral-edge de-spill so the magenta clear key does not bleed into white HUD glyphs, minimap edges, portrait edges, or the RPM meter.
- Full-screen smoke capture after the HLSL pass: `artifacts/render-modernize-hlsl.png`.
- HUD is composited at full player-view size over the post-processed scene. Do not scale the final HUD framebuffer; it double-shrinks the HUD and does not recover pixels that were already clipped in the HUD source. The RPM meter is anchored as a complete panel with `gArcadeKart.Hud.RpmMeterRightMargin` and `gArcadeKart.Hud.RpmMeterBottomMargin`, not by its visible gauge face, so its hidden texture bounds stay inside the source. The executable-side `build/x64/Debug/mods/zz-arcadekart-rpm-faceplate.o2r` must stay in sync with the root `mods` copy because the app loads mods relative to the executable directory.
- Saved transient framebuffer CVars can be stale during startup. `GetFramebufferTextureId` must return null for out-of-range ids, and the compositor clears `LayeredHudActive` when either scene or HUD framebuffer lookup fails.
- Screen shake uses `gArcadeKart.PostFx.ShakeStrength` as the mushroom/top-speed-ish amount, then scales it by speed: `gArcadeKart.PostFx.ShakeIdleScale` defaults to 0.05 at idle and `gArcadeKart.PostFx.ShakeFullSpeedRatio` defaults to 1.0 for full shake.
- Race menu directions no longer tune post-FX or wheel spring values. Post-FX tuning lives in menu CVars: normalized `gArcadeKart.PostFx.TestShakeSlider`/`TestWarpSlider`, raw-max remaps `TuningShakeInputMax`/`TuningWarpInputMax`, visual gains `TuningShakeStrength`/`TuningWarpStrength`, and response shaping via `SpeedMinRatio`/`SpeedMaxRatio` plus `ResponsePower`. Legacy debug lap skip is gated by default-off `gArcadeKart.DebugLegacyLapSkipEnabled`.
- On `FXpass`, scene framebuffer binding moved after race viewport setup because `race_begin_viewport` can select the normal framebuffer. Drift puffs also explicitly reset texture LUT/alpha state inside `render_player_drift_particles` before loading the old I8 drift textures.

## ArcadeKart Audio

- Custom bad-shift samples are copied by the `Spaghettify` post-build step from `assets/arcadekart_audio/` to the executable-side `arcadekart_audio/` folder. HMAS rejected the original Vorbis OGG files as `Invalid file`, so gameplay now loads PCM WAV conversions instead. A bad shift writes `GRIND_SOUND ... custom 1` to `logs/ArcadeKartShift.log` when the custom sample path is active.

## Texture Mods

- The active HD texture pack is `mods/mk64-reloaded-v2025.12.20-sk-hd.o2r` (`MK64-Reloaded-SK`). Track targeted overrides as a separate loose folder mod named `spiny-valley-porcupine-override`, currently containing `textures/tracks/yoshi_valley/yoshi_valley_data/d_course_yoshi_valley_hedgehog.png` and `textures/common_data/common_texture_speedometer.png`. Keep copies in both `E:\SpaghettiKart\mods\` and `E:\SpaghettiKart\build\x64\Debug\mods\` so the runtime loads it after Reloaded without modifying the large pack.
- Yoshi Valley hedgehog sprite flipping is camera-space now: each hedgehog/camera pair tracks its previous projected X and uses the mirrored quad only when it moves camera-left.
- World drift feedback text keeps a per-particle tire corner anchor; the current anchor has `unk_010 == 1` using the positive card half-width and `unk_010 == 0` using the negative half-width.
