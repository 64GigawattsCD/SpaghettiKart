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
- HUD cluster scale and placement tuning is fixed in code now. The only live ArcadeKart HUD tuning CVar is `gArcadeKart.Hud.SafeZoneScale`, which globally scales edge insets.
- RenderModernize HUD visual target as of 2026-06-01: placement high top-left, portrait strip below/left, time/lap centered high, vertical lap split list top-right, minimap mid-right, RPM meter bottom-right. Latest useful full capture: `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\screenshots\render-modernize-layout\RenderModernize-layout-final-pass-20260601-152150.png`.
- The one-player minimap and RPM meter are separate anchored groups now. The minimap constructs a scaled rect from the active track map, so larger map textures grow leftward. The RPM meter does not read minimap dimensions. Right-side minimap/RPM placement is fixed in code from HUD-reference insets now, so stale saved placement CVars do not move those groups.
- Right-side ArcadeKart HUD elements render into the post-FX HUD framebuffer. Use unchanged 320x240 HUD coordinates there; old wide helpers/matrices that call `OTRGetDimensionFromRightEdge` push right-side sprites out of the HUD framebuffer.
- Portrait strip uses safe-zone anchoring for center-left placement with code-side constants for scale, edge inset, spacing, and center offset.
- HUD micro-pass completed: placement left/top offsets moved inward, portrait strip edge offset increased, map vertical/padding defaults moved inward slightly, and top/lap cluster defaults re-centered to reduce manual slider churn.
- RenderModernize HUD cluster scale constants are 0.8 for portrait strip, time/lap strip, lap-time list, minimap/dots, and RPM meter; each cluster keeps its safe-zone-aware anchor while shrinking.

## HUD Timer/Lap

- `func_8004EB38` was the legacy translucent afterimage pass for `TIME` and `LAP`; keep it disabled while the ArcadeKart HUD owns the race timer/lap presentation.
- The top-center timer/lap strip uses native sprite rectangles for `TIME`, timer digits, `LAP`, and `1/3`; keep those as separate layout children so spacing can be tuned without distorting glyph proportions.
- The top-right lap split list must use `hud_layout_canvas_safe_slot` and the scaled timer glyph renderer. The legacy `print_timer` path ignores the intended layout width/right edge in widescreen and can clip against the right side.

## Minimap

- `draw_minimap_character` has four explicit marker paths: normal dot, rainbow dot, normal human kart icon, and rainbow human kart icon. The first-place paths use the same scaled texture-rectangle geometry as the non-rainbow paths so markers stay aligned.
- Live human minimap kart icons now use an isolated rotated path: a private centered 8x8 quad, `func_80042330_unchanged`, `D_0D007968` for tint-friendly minimap state, the RGBA16 kart texture, and an explicit `D_0D007EB8` restore. Do not go back to `func_80046AD4` / `common_vtx_player_minimap_icon`; that old path repeatedly caused a wide horizontal chunk of HUD/game rendering to disappear. The angle comes from `player->rotation[1]` plus a baked 180-degree flip. The marker draws a slightly larger black pass under the colored/rainbow pass so it keeps a black outline while tinted.
- The race minimap now has a single layout helper for the texture, finish-line marker, and racer dots. Keep all minimap world-to-HUD transforms inside that rect so map art dimensions do not leak into other HUD groups.
- Minimap placement must clamp the actual active map rect against the HUD safe rect after per-track width/height and cluster scale are applied. Do not anchor from a fixed/reference map size or only from the track-provided center; large/wide maps must move their left edge inward while their right edge stays inside the safe region.

## Controller Sequential Shift

- `Shift Gear Up` and `Shift Gear Down` are dedicated bindable commands.
- Default controller flow is right-stick click for `Clutch`, then right-stick up/down for sequential upshift/downshift; sequential shifts are ignored unless the clutch command is active.
- Keep the running controls list in `docs/control-change-log.md`.
- `Toggle HUD` now uses `KART_TOGGLE_HUD_BUTTON`; `BTN_CRIGHT` is legacy only, so stale right-stick-right/C-right mappings do not hide the HUD.

## Transmission Mode

- Manual/automatic is selectable per human player on character select with L/R. The small cursor label shows `MANUAL` or `AUTO`, and the toggle persists through `gArcadeKart.Transmission.PlayerNMode` defaults.
- AI racers remain forced automatic in `kart_transmission_is_automatic` until explicit AI manual-shift behavior is authored.
- The ArcadeKart options menu exposes simple per-player auto-transmission default checkboxes; normal race-flow selection remains on character select.
- `gArcadeKart.DebugQuickBootHudRace` is default-off again. When manually enabled, it skips straight into a 1P 50cc Mario GP race and chooses a random stock GP track each process launch. Normal character/transmission select is the default path.

## Render Modernize

- The race scene now renders into `gArcadeKartPostFxSceneFrameBuffer`; HUD rendering switches to `gArcadeKartPostFxHudFrameBuffer` afterward so post effects sit under HUD.
- The DX11 path uses HLSL pixel shaders in `Gui.cpp`: one for scene barrel/overscan/blur/shake, and one to composite the HUD framebuffer by keying the magenta clear color into alpha.
- The HUD framebuffer does not carry useful alpha when sampled by ImGui; plain alpha blending turns its clear color opaque. Keep the keyed HUD shader unless the framebuffer backend starts preserving alpha. The HUD compositor must use the point-clamp sampler and neutral-edge de-spill so the magenta clear key does not bleed into white HUD glyphs, minimap edges, portrait edges, or the RPM meter.
- Full-screen smoke capture after the HLSL pass: `artifacts/render-modernize-hlsl.png`.
- HUD is composited at full player-view size over the post-processed scene. Do not scale the final HUD framebuffer; it double-shrinks the HUD and does not recover pixels that were already clipped in the HUD source. The RPM meter is anchored as a complete panel with fixed code-side right/bottom insets, not by its visible gauge face, so its hidden texture bounds stay inside the source. The executable-side `build/x64/Debug/mods/zz-arcadekart-rpm-faceplate.o2r` must stay in sync with the root `mods` copy because the app loads mods relative to the executable directory.
- Saved transient framebuffer CVars can be stale during startup. `GetFramebufferTextureId` must return null for out-of-range ids, and the compositor clears `LayeredHudActive` when either scene or HUD framebuffer lookup fails.
- Screen shake uses `gArcadeKart.PostFx.ShakeStrength` as the mushroom/top-speed-ish amount, then scales it by speed: `gArcadeKart.PostFx.ShakeIdleScale` defaults to 0.05 at idle and `gArcadeKart.PostFx.ShakeFullSpeedRatio` defaults to 1.0 for full shake.
- Race menu directions no longer tune post-FX or wheel spring values. Post-FX tuning lives in menu CVars: normalized `gArcadeKart.PostFx.TestShakeSlider`/`TestWarpSlider`, raw-max remaps `TuningShakeInputMax`/`TuningWarpInputMax`, visual gains `TuningShakeStrength`/`TuningWarpStrength`, and response shaping via `SpeedMinRatio`/`SpeedMaxRatio` plus `ResponsePower`. Legacy debug lap skip is gated by default-off `gArcadeKart.DebugLegacyLapSkipEnabled`.
- On `FXpass`, scene framebuffer binding moved after race viewport setup because `race_begin_viewport` can select the normal framebuffer. Drift puffs also explicitly reset texture LUT/alpha state inside `render_player_drift_particles` before loading the old I8 drift textures.

## ArcadeKart Audio

- Custom bad-shift samples are copied by the `Spaghettify` post-build step from `assets/arcadekart_audio/` to the executable-side `arcadekart_audio/` folder. HMAS rejected the original Vorbis OGG files as `Invalid file`, so gameplay now loads PCM WAV conversions instead. A bad shift writes `GRIND_SOUND ... custom 1` to `logs/ArcadeKartShift.log` when the custom sample path is active.

## Texture Mods

- The active HD texture pack is `mods/mk64-reloaded-v2025.12.20-sk-hd.o2r` (`MK64-Reloaded-SK`). Track targeted overrides as a separate loose folder mod named `spiny-valley-porcupine-override`, currently containing `textures/tracks/yoshi_valley/yoshi_valley_data/d_course_yoshi_valley_hedgehog.png` and `textures/common_data/common_texture_speedometer.png`. Keep copies in both `E:\SpaghettiKart\mods\` and `E:\SpaghettiKart\build\x64\Debug\mods\` so the runtime loads it after Reloaded without modifying the large pack.
- Yoshi Valley hedgehog sprite flipping is camera-space now: each hedgehog/camera pair tracks its previous projected X and uses the mirrored quad only when it moves camera-left.
- World drift feedback text keeps a per-particle side anchor in `unk_010`. The tire origin and rendered card corner are intentionally crossed so the word sits outward from the kart: left drift uses the back-right tire and right drift uses the back-left tire, while the drawn card aligns the opposite horizontal corner to that tire point. The red/blue world marker overlay render call is removed from the normal pool path; lateral/backward movement still starts after age zero. The spawn gate is currently 8 logic ticks.
- Drift words are rendered from the separate ArcadeKart feedback pool. The legacy human-drift particle slot now keeps calling `setup_tyre_particles`, so normal tire dust stays visible while word cards spawn.
- World drift feedback words should only spawn while the player is actually drifting (`DRIFTING_EFFECT`, positive `driftDuration`, or positive `driftState`). Hard steering alone can still create baseline skid particles, but it must not create ArcadeKart `Slide` text.

## Particle Modernization

- `src/engine/particles/ArcadeKartParticleEmitter` now extends the existing C++ particle hook with parameterized emitters, player attachment, world/local space simulation, spawn-rate accumulation, bursts, lifetime, velocity, acceleration, alpha, and scale interpolation. It is dormant infrastructure; tire dust/exhaust/drift words still use their current paths until a specific migration pass.
- Boost exhaust sparkles now use `ArcadeKartBoostSparkEmitter`, two local-space emitters per racer attached at the same left/right exhaust stack offsets as the duplicated smoke. They key off shared `BOOST_EFFECT`, so normal/triple/golden mushroom boosts and any AI boost use the same replacement path. Their per-particle local velocity scatters evenly left/right, biases upward, and only travels backward from the exhaust. The old type-8 player boost spark renderer is intentionally a no-op while this emitter owns the visual.
- Good manual shifts call `CM_BurstArcadeKartBoostSparks`, which reuses the same boost exhaust spark emitters for a small one-shot burst without setting `BOOST_EFFECT` on the player.
- Drift/turbo tire sparkles now use `ArcadeKartDriftTireSparkEmitter`, two local-space rear-tire emitters per racer. They key off the same drift evidence as the word effect (`DRIFTING_EFFECT`, positive `driftDuration`, or positive `driftState`). The visible effect has a tire-local zap core from split `lightning_zap_0/1` half-cards plus a copied mushroom/exhaust spark layer with extra outward/up/back motion. Zap flip animation uses private flipped UV quads plus swapped split textures, never the old mirrored stock display list. The zap halves share the same stage tint: yellow for slide, orange for drift, rainbow for turbo drift. Spark-copy velocity is stored per particle but only applied while drawing the copied spark layer, so the zap stays tire-local. Drift tire sparkles skip the normal world particle pass and draw from `CM_DrawArcadeKartLateParticles` after kart rendering; do not reintroduce the camera-space nudge, because it breaks wheel alignment without fixing draw order. The emitter starts at the reported rear tire location with no extra inward/rearward local offset; keep only per-particle jitter for variety. Its local motion uses half of `player->unk_0C0` on top of body yaw so the sparks follow drift slip without over-rotating around the visible kart sprite. The text word effect remains human/owner scoped in the legacy feedback pool.
- The drift tire spark draw pass receives both `cameraId` and `screenId`. The zap/spark render position is a blend between the physical rear tire point and a screen-specific visual anchor reconstructed from the same card origin/orientation math used by `render_kart`, including the card tilt term from `unk_0CC` and the roll/yaw fields `unk_048`/`unk_050`. The sprite-local X sign is mirrored from the physical tire side because the rear-facing kart billboard projects player-left/player-right opposite from the tyre array's world-space side. This is intentional camera-facing alignment for the visible sprite tire, not a change to the dust/contact physics origin.
- New C++ particle draw paths must restore common display state with `D_0D007EB8` after drawing. The drift tire emitter's first pass used the mirrored stock card display list (`D_0D008E70`) for random horizontal flips and did not restore state; that reproduced the wide missing-HUD-band failure where center/right HUD groups disappear from the post-FX HUD framebuffer. Keep tire spark flips off or implement them with a private quad/UV path, never by swapping to the mirrored stock display list in this branch.
