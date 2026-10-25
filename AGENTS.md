# SpaghettiKart Working Context

## Start Here
- Active local checkout: `E:\SpaghettiKart-VR-refresh`, branch `codex/vr-refresh-oct13`.
- `E:\SpaghettiKart` is the archived original VR draft. The OneDrive `mk64 Arcade`
  folder is a launcher/mod workspace, not this source checkout.
- Read `docs/current-state.md` once at session start, then run
  `./tools/dev/context.ps1`. Read only the relevant source/doc entries below.
- Treat dates and saved evidence as historical; inspect live Git/device state.

## Work Efficiently
- Start with filenames and targeted `rg -n -m 15` searches. Read bounded ranges.
  Do not dump whole session logs, generated mesh includes, asset trees or diffs.
- `./tools/dev/context.ps1 -Mode headings` lists historical note sections;
  `-Mode notes -Section 'Radial Pedals'` returns one bounded section.
- Send noisy native commands through `tools/dev/run-logged.ps1`; full logs stay
  under ignored `build/agent-logs`, with a short result or failure tail in chat.
- Reuse existing tools/tests. Load Blender/ComfyUI/Meshy only for relevant work.
- Update `docs/current-state.md` in place after milestones (keep <=60 lines).
  Keep detailed evidence in topic docs; append only new discoveries to history.
- Do not trade verification or correctness for fewer tokens. Keep model settings
  and global tools unchanged. Never infer task billing from account balance deltas.

## Source Map
| Area | Start With | Detail / Focused Tests |
| --- | --- | --- |
| Phone controls | `android/app/src/main/java/dev/spaghettikart/phone/TouchControls.java`, `ShifterGate.java` | `docs/touch-controls-art.md`, `tests/android/` |
| Android/native input | `src/port/AndroidPhone.cpp`, `src/port/Engine.cpp` | `android/README.md`, `tests/kart_assets/android_client.py` |
| HUD/RPM | `src/hud_layout.c`, `src/hud_race_layout.c`, `src/hud_rpm.h`, `src/render_objects.c` | `docs/arcadekart-hud-layout.md`, `tests/hud/`, `tests/vr_refresh/` |
| Menus/options | `src/port/ui/GameOptions.cpp`, `src/port/ui/PortMenu.cpp`, `src/menus.c`, `src/menu_pointer.c` | `docs/ui-unification-audit-2026-10-25.md`, `tests/game_options/`, `tests/menu_pointer/` |
| Camera/cockpit | `src/engine/cameras/GameCamera.cpp`, `src/first_person_kart.c` | `docs/cockpit-and-autonomous-tests.md`, `tools/kart_assets/author_cockpit.py` |
| LAN | `src/port/net/`, `src/port/ui/LanRaceWindow.cpp` | `docs/network-lan-prototype-2026-10-16.md`, `tests/network/` |
| Audio | `src/port/Engine.cpp`, `src/port/audio/` | `docs/android-audio-diagnosis-2026-10-16.md`, `tests/audio/` |
| Renderer | `libultraship/` (separate Git repo) | Check its status before editing or publishing |

## Operating Rules
- Latest preference: implement first, let the user playtest. Do not launch games,
  drive the phone or run gameplay automation without a newer request. Offline
  unit/tooling checks remain useful. Any background gameplay must be muted.
- Preserve normal settings/saves. Use existing isolated test bridges when testing
  is requested; see `docs/cockpit-and-autonomous-tests.md`.
- Commit/push renderer changes before the parent gitlink when publication is asked.
  Use the existing GitHub no-reply email; never rewrite published history routinely.
- Keep ROMs, credentials, compiled outputs and generated duplicate headers local.
  Old Blender checkpoints are split archives on `VRintegration`; no new LFS uploads
  are accepted by this public fork.
- Command recipes and helper checks: `tools/dev/README.md` (read only as needed).
