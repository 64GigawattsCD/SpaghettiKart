# Spiny Valley Texture Overrides

Tiny SpaghettiKart texture overlay for replacing selected MK64 Reloaded textures without touching the rest of Reloaded.

Live override slots:

```text
textures/tracks/yoshi_valley/yoshi_valley_data/d_course_yoshi_valley_hedgehog.png
textures/common_data/common_texture_speedometer.png
```

Current state: the hedgehog texture is seeded with the MK64 Reloaded texture as a placeholder, and the speedometer slot contains the ArcadeKart RPM meter faceplate. Replace art in-place, keeping filenames and folder paths exactly the same.

Load order is handled by `mods.toml`: this mod depends on `MK64-Reloaded-SK`, so Reloaded loads first and this texture lands on top. The active Reloaded pack uses a date-style version string, so the dependency intentionally tracks the pack name with a loose version range.
