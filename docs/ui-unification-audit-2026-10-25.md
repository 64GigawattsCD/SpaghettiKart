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
