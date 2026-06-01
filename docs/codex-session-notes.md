## Screenshot Capture

- For fullscreen SpaghettiKart captures, call `SetProcessDPIAware()` before reading screen or window bounds.
- Without DPI awareness, PowerShell reports the 4K primary as 2560x1440 and captures only the top-left portion of the window.
- For DirectX fullscreen, prefer screen capture with `System.Drawing.Graphics.CopyFromScreen`; `PrintWindow`/BitBlt window copies can return black frames.

## RPM Meter

- The RPM faceplate is supplied by `mods/zz-arcadekart-rpm-faceplate.o2r` as `textures/common_data/common_texture_speedometer.png`.
- The faceplate art owns the orange warning region now; do not add runtime color masks over the gauge face.

## HUD Timer/Lap

- `func_8004EB38` was the legacy translucent afterimage pass for `TIME` and `LAP`; keep it disabled while the ArcadeKart HUD owns the race timer/lap presentation.

## Minimap

- `draw_minimap_character` tints the neutral minimap progress dot from an explicit character palette; first place still uses `func_8004C450` for the existing rainbow flash.
