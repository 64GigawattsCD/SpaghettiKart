## Screenshot Capture

- For fullscreen SpaghettiKart captures, call `SetProcessDPIAware()` before reading screen or window bounds.
- Without DPI awareness, PowerShell reports the 4K primary as 2560x1440 and captures only the top-left portion of the window.
- For DirectX fullscreen, prefer screen capture with `System.Drawing.Graphics.CopyFromScreen`; `PrintWindow`/BitBlt window copies can return black frames.

## RPM Meter

- Do not build the red-zone mask by sampling `common_texture_speedometer` as I4. Texture packs can replace that asset with PNG/RGBA data, which makes CPU-side I4 nibble reads produce dirty edge pixels.
- The current red-zone path generates a small annular-sector I4 mask from meter geometry and uses character shift stats to place the red start at the good upshift threshold.

## HUD Timer/Lap

- `func_8004EB38` was the legacy translucent afterimage pass for `TIME` and `LAP`; keep it disabled while the ArcadeKart HUD owns the race timer/lap presentation.

## Minimap

- `draw_minimap_character` tints the neutral minimap progress dot from an explicit character palette; first place still uses `func_8004C450` for the existing rainbow flash.
