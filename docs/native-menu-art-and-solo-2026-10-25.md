# Native Menu Art And Solo Flow

## Presentation

The native menus retain stock bitmaps, portraits, game-font text and the original
mode-tile dimensions. Common buttons now use an alpha-blended charcoal surface,
one-pixel clipped corners and a quiet yellow leading rail instead of flashing
opaque slabs. Texture focus can be drawn independently without tinting the art.
Small checker pairs beneath titles and local-player color rails provide the
extra kart-menu character. Touch hit rectangles remain unchanged.

`draw_box_fill` encodes RGBA5551 in the fill cycle: its alpha parameter is not an
8-bit opacity control. Native surfaces and portrait captions now use `draw_box`
instead. Main join badges, character captions, viewport dividers, roster rows,
setup, Options, embedded bindings and Quit follow the shared treatment. The
existing texture renderer retains stock texture alpha masks.

No raster assets were added, and no ComfyUI or paid art jobs were submitted.
Art/service credits spent: 0. Per-task model-token billing is unavailable.

## Solo Route

Only the explicit Main-menu 1P action enables the shortcut. A Local session with
one human and an Online session with one local driver retain the party flow.

1. Race setup retains cup/course, class, laps, CPU fill, mirror and item options.
   Player capacity is omitted. The persistent Next footer is available by touch,
   mouse or controller selection; Start also continues from any setting.
2. Next opens the same universal character grid, without the one-person lobby.
3. The same centered transmission choices follow. Race confirms transmission,
   readies the solo participant and queues one Start, without a separate Ready
   prompt or lobby Start command.

Back from character selection returns to setup with the local session intact;
Next reopens a fresh selection barrier. Back from setup leaves the session and
returns Main. Pending actions are consumed once and cancelled on Back, changed
mode or connection errors. GP standings remain explicit, initially focused on
Next Race. Setup returns directly to settings; New Cup returns there after the
fourth race rather than attempting a fifth race.

## Offline Verification

- Three lobby suites cover party/browser/editor, shared driver geometry/barriers,
  and solo eligibility, footer navigation, cancellation and one-shot actions.
- Two pointer suites include the actual native art helpers compiled with recording
  primitives, checking alpha, untouched corners, focus-only edges and fixed bounds.
- The native Options model suite passes.
- All six network suites pass, including added one-player GP/VS selection,
  readiness, one-shot load, results and revised-settings checks.
- The Python lobby helper's two offline tests pass. Its new `--run --solo` route
  is opt-in and was not run. It covers trimming to one player, persistent Next,
  Back/re-entry, manual transmission and the single-view race transition.

No game process, screenshots or phone inputs were used for verification. Actual
visual appearance and interactive menu/race transitions remain for user review.

Windows Debug and Android release compilation passed:

- `build/agent-logs/menu-art-solo-final-pc-20261025-191324-26503031.log`
- `build/agent-logs/menu-art-solo-final-phone-20261025-191334-0b7f5471.log`

The APK was installed with `adb install -r` on SM-S908U at 2026-10-25 19:16
phone time. No app launch was requested. Settings SHA-256 was unchanged:
`61d6eec31c98f1b744a8aa6c35462016065ce844689e3fb93f11e72f9a3dcc9d`.
