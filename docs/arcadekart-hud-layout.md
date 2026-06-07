# ArcadeKart HUD Layout Notes

This note records the intent and current shape of the ArcadeKart HUD layout pass.

## Design Intent

- Keep the HUD recognizable as Mario Kart 64: original race timer, lap, placement, portrait, minimap, item, and speedometer/RPM art should stay visually native unless we intentionally replace a specific asset.
- Treat each HUD feature as a movable cluster instead of hand-positioned one-off sprites. The major clusters are placement number, top-four portrait strip, timer/lap strip, lap-time list, item box, minimap/dots, and RPM meter.
- Anchor clusters to a player-view safe zone, not to the raw screen, so 1P and split-screen layouts share the same mental model.
- Apply scaling at cluster boundaries using code-side constants. Individual glyphs and textures should keep their native proportions inside the cluster so text does not become vertically squeezed or horizontally stretched.
- Keep post-process effects below the HUD. The HUD is meant to read clearly even when the game layer has barrel warp, motion blur, screen shake, or overscan.

## Layout Model

- `HudLayoutContext` owns the full player view and a safe rectangle. The full view is still used for scale math; anchors can resolve inside the safe rectangle.
- The local UI system is modeled after practical Unreal-style widgets:
  - Canvas slots for anchored placement.
  - Horizontal and vertical boxes for grouped children.
  - Scale boxes for cluster-level size tuning.
  - Size boxes for stable fixed-format widgets.
  - Grid-style placement where a compact matrix makes sense.
- Layout offsets are expressed in player-view-relative units where possible, then converted through helper functions so 4:3, widescreen, and split-screen behave consistently.

## Work Done

- Moved the race timer/lap presentation to a top-center cluster using original texture glyphs for `TIME`, timer digits, `LAP`, and lap count.
- Disabled the legacy translucent "ghost" timer/lap intro pass while the ArcadeKart top-center strip owns that region.
- Moved individual lap times into a top-right vertical list, with right-edge anchoring and original timer glyph style.
- Moved placement to the top-left and wrapped it as its own cluster so end-of-race enlargement has predictable padding.
- Moved the top-four portrait stack to center-left, wrapped in a vertical box, and tuned spacing/padding independently from placement.
- Preserved portrait and item opacity by fixing HUD framebuffer compositing and avoiding accidental alpha loss through the post-FX path.
- Moved minimap and dots into a right-side cluster and updated racer dots to use character colors while retaining the original rainbow flash for first place.
- One-player minimap placement is right-edge driven from a fixed HUD-reference inset. The active minimap image builds its own scaled rect, so larger track maps move their left edge inward while their right edge stays anchored.
- Reworked the speedometer into an ArcadeKart RPM meter cluster anchored bottom-right:
  - Custom faceplate texture from the mod pack.
  - Needle driven by a normalized RPM function.
  - Gear and digital RPM readout overlaid on the meter face.
  - Orange/red shift feedback driven by transmission tuning.
- Removed per-cluster HUD CVars after tuning stabilized. Current scale and placement values live as named constants, with `gArcadeKart.Hud.SafeZoneScale` left as the single global HUD edge-inset control.

## Current Visual Target

- Placement high top-left, inset enough to survive enlarged end-of-race text.
- Portrait strip below and left, centered vertically as a group.
- Timer/lap strip top-center, with original proportions and readable spacing.
- Individual lap times top-right, smaller than the main timer and aligned by right edge.
- Item box centered under the timer/lap cluster.
- Minimap mid-right.
- RPM meter bottom-right, aligned with the right-side HUD edge and fully inside the safe zone.
- Minimap art dimensions must not push the RPM meter. The minimap/dots group and RPM meter/readout group are separately anchored from fixed right-side insets; saved placement CVars are intentionally not read for those two groups.
- Gear and RPM readout overlays are positioned inside the fixed RPM widget canvas. Their right edge is center-relative to the widget's bottom-right body area, so `AUTO`/gear text width does not affect the main meter body or the minimap.
- Right-side HUD clusters render into the ArcadeKart HUD framebuffer, so they must use unchanged 320x240 HUD coordinates. Do not send minimap/RPM quads through the old wide HUD helpers that call `OTRGetDimensionFromRightEdge`; that moves them outside the HUD layer before composition.
- `gArcadeKart.Hud.SafeZoneScale` scales edge insets globally. It does not change individual cluster proportions or resurrect per-widget offsets.

## Guardrails

- Do not scale the final HUD framebuffer to compensate for overscan. It causes double-shrinking and can clip source pixels before composition.
- Do not use the legacy `print_timer` path for the modern top-right lap list; it ignores the intended layout rect in widescreen.
- Keep the root and executable mod copies of the RPM faceplate in sync: `mods/zz-arcadekart-rpm-faceplate.o2r` and `build/x64/Debug/mods/zz-arcadekart-rpm-faceplate.o2r`.
- If a HUD sprite looks tinted magenta or pink, inspect the HUD framebuffer keying/de-spill path before changing the art.
