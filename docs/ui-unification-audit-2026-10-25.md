# Native Menu Unification

## Audit

The app had three user-facing settings paths with separate navigation and rendering:

| System | Previous Role | Current Role |
| --- | --- | --- |
| Decomp Options | Sound mode, Controller Pak copy and erase | Entry to the shared native Options tree; original save dialogs retained |
| Native pause/view overlay | A second pause page with camera tuning and race actions | Shared native pause root and Options tree |
| PortMenu/ImGui | Most audio, graphics, input, rules and VR settings | Shared option definitions; standalone window is an opt-in developer tool |
| Input Editor | Separate controller configuration window | Embedded inside the native Controller Bindings page |
| LAN window | Independent floating host/join/status panel | Native Network page, including address, character, port and session actions |

The main problems were duplicated camera option definitions, ordinary settings
that were absent from native menus, separate input capture rules and nested
desktop windows on a phone. Port menu appearance/search/popout preferences were
also mixed with gameplay options. The old pause view editor always read player
one's input even when another local player owned the pause.

## Shared Flow

`src/port/ui/GameOptions.cpp` renders through existing game text, boxes, selection
highlights and menu sound effects. Pages are General, Display, Audio, Controls,
Camera, HUD, Effects, Race Rules, Extras, LAN Race and Save Data. Lists page six
rows at a time. The front end and paused game use the same pages and settings.

Options is directly reachable from the main/character/course screens and the
original Options entry. Escape/F1 route into this flow. Pause opens a native root
with Resume, Options and the existing restart/course/driver/setup/quit actions.
Back returns through the page stack; Done closes the front-end tree. Resume uses the original controller/audio
restore sequence. The pausing local player's controller owns offline navigation.

The adapter visits the existing `PortMenu` widget definitions instead of copying
their CVars. Defaults, limits, platform conditions, dynamic visibility, callbacks
and pending renderer/audio changes therefore come from the same source. It also
handles direct-value widgets such as fullscreen and track scale. Enum keys are
sorted and cycled by their actual IDs, including sparse values. Unsupported
single-backend choices appear disabled. Android omits desktop fullscreen and
Logitech SDK settings. Existing saved keys and user controller mappings remain.

Menu chrome preferences, editor utilities and debug entries stay out of the
ordinary options tree. The original developer interface is retained behind
Ctrl+Shift+F1 when `gEnableDebugMode` is enabled. It no longer reopens as the normal
settings UI from a saved `gOpenMenu` flag. Select/Back on a gamepad remains the
existing camera binding instead of also toggling a desktop settings window.

## Interaction And Specialized Pages

- Touch/mouse targets share the native layout, with fixed Back/Resume controls,
  steppers and page arrows. Mouse wheel pages the list. Held directional input
  repeats numeric changes and accelerates after two seconds; toggles act on edges.
- Layout/page changes invalidate pending pointer releases, including changes to
  dynamically hidden camera rows and LAN availability. Pointer clicks take
  precedence over simultaneous queued wheel scrolling.
- Controller bindings retain the existing device/remapping logic in a child
  panel inside the game image. Its capture popups keep their input ownership.
  That specialized panel is unavailable in the headset; native VR tuning remains.
- VR settings still use the game HUD render path. The prior first-person
  pre-race lock, world-locked HUD choice and menu-opacity setting are retained.
  The non-Windows VR stub now supplies the declared first-person-lock query.
- Save Data routes copy/erase into the original save state machine from the
  main menu. It preserves the existing erase confirmation and copy error handling.
  In-race save operations remain unavailable. Sound mode moved to Audio and still
  writes through the original save functions.
- LAN uses the existing queued session commands and transport. Host address is
  editable as four numeric octets. A connected racer can open the native menu
  without pausing the authoritative race. Raw local menu input is retained before
  transport filtering; driving input becomes neutral while the menu is open.
  Restart/course/rule changes are disabled during a connected session; Leave LAN
  uses the existing disconnect transition. No protocol or packet layout changed.
- Android classifies native menus as menu input, including a live LAN menu, so
  gameplay touch pedals/sticks are hidden while native targets own the screen.

## Verification And Handoff

Offline checks cover page navigation over empty/large/dynamically shortened lists,
sparse enum values, numeric clamping and malformed saved floats, section routing,
pointer behavior and the existing HUD/network checks. Windows Debug and Android
release compilation are required for this change.

The test bridge now reports `menu.options`: open/page/context/live-network state,
row labels, values, visibility, enabled state and target IDs. Existing desktop
Options/Camera and desktop/phone pause runners consume this semantic state rather
than the removed view-overlay context numbers.

No game process or phone driving test was run during this pass, per the user's
implementation-first preference. The opt-in game routes were updated but remain
unrun. User review is still needed for native font/layout at actual screen sizes,
controller remapping capture, save dialog return, all pause transitions, native
LAN input ownership, and a headset session. Source-level and compile checks do
not establish those runtime results.

Focused checks:

```powershell
cmake -S tests/game_options -B build/game-options-tests -G 'Visual Studio 17 2022' -A x64
cmake --build build/game-options-tests --config Debug
ctest --test-dir build/game-options-tests -C Debug --output-on-failure
python -m unittest discover -s tests/kart_assets -p 'test_pointer_menu*.py'
```

## Main Footer And Stock Art Follow-Up

User's first PC review identified both the original L OPTION texture and the new
Options entry. The original OPTION/DATA items are no longer instantiated or drawn.
The player-count screen has one equally spaced Options / Data / Back / Quit row,
with 68x28 targets at y=202 inside the 320x240 game image. The separate bottom Back
and Options overlays are suppressed there. Version/debug labels move above the
banner on that screen to leave the footer clear.

Down enters the footer beneath the selected player-count tile; left/right cycle
its four actions; Up or Cancel returns to the player-count tiles. Confirm activates
the focused action. Touch/mouse release uses the same action path (IDs 301..304 in
the existing main-menu pointer context). Direct L/R/Z shortcuts are removed from
this menu only; gameplay bindings and the separate course-random action remain.
Back uses the original title-screen transition. Options opens the unified tree
without leaving the current main menu. Data retains its original transition and
record/save handling. Quit keeps its native wipe and clean engine shutdown, now
with a directional Yes/No confirmation initially focused on No.

The art pass reuses the existing blue-sky/sunset menu resource, main-menu tint,
stock glyphs, black tiles, slow flashing selections and rainbow selected text.
Options and race setup redraw that backdrop instead of opaque navy panels; pause
keeps gameplay behind a black dim layer and retains VR opacity. Navigation tiles,
transmission choices, quit confirmation and embedded-binding colours follow the
same palette. No new raster art or ComfyUI jobs were needed. Paid art credits: 0;
task-specific model-token billing is not available in this checkout.

Added offline coverage: all four pointer/controller footer actions, directional
entry/return/wrap, cancellation precedence, player-tile focus transfer and footer
geometry. The opt-in quit runner now targets the unified footer. Runtime visual
and input review remains with the user; no autonomous gameplay was launched.

Final checks passed: Windows Debug compilation, Android release compilation
(including the final spacing edits), the native options/footer model suite,
native pointer tests, seven Python menu-helper checks, and `git diff --check`.
No phone installation or visual/gameplay test was attempted in this follow-up.
Final compile logs: `build/agent-logs/native-menu-spacing-windows-20261025-113926-e186ef03.log`
and `build/agent-logs/native-menu-spacing-android-20261025-114046-b6733a30.log`.

## Victim Radio Portrait Assessment, 2026-10-26

Assessment only; no gameplay changes or game/device tests. Stock item collision
branches in `src/racing/actors.c` retain victim `temp_lo` and attacker `temp_v1`
when requesting the character hit-success voice (`0x29008006` plus character).
`func_800C90F4` receives only speaker and sound, so collision-time attribution is
the useful event hook. Existing taunts have human-owner, self-hit and sometimes
distance gates; portrait coverage should explicitly follow or revise those rules.
Stock 64x64 face assets exist. Anchor one noninteractive, clipped slide-up panel
to each victim viewport using `hud_race_viewport`; keep it clear of touch controls.
Local presentation is modest work. Online needs an authoritative transient hit
event with race/sequence identity, attacker/victim participants and voice choice;
current item frames carry actor/inventory state, not that event. Deduplicate and
expire events rather than infer hits from replicated effects. Keep audio/portrait
dispatch paired and bound repeated-hit queues. Mouth animation is additional art,
not supplied automatically by the face assets. VR can use the existing HUD plane
but still needs headset review. Art/service credits spent: 0; task token billing
unavailable.
