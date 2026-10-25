# Native Lobby Checks

No game process or device interaction is used by this suite.

```powershell
cmake -S tests/lobby -B build/lobby-tests -G 'Visual Studio 17 2022' -A x64
cmake --build build/lobby-tests --config Debug
ctest --test-dir build/lobby-tests -C Debug --output-on-failure
```

Coverage: four-player local party admission into an eight-seat session, compatible
lobby-only browser admission, paging, controller reachability of every native
keyboard command, password display masking, and all UDP port numbers plus malformed
text. The shared functions are in `GameOptionsModel.h`; runtime UI is in `GameLobby.cpp`.

Frontend source handoff on 2026-10-25: `menus.c` routes native main-menu input;
`menu_items.c` draws pages and retains the existing footer/quit confirmation;
`GameOptions.cpp` routes Online, Back, scrolling, and raw network menu input.
`game_lobby_poll_race_load` in `menus.c` serves repeated same-state race reloads;
the backend agent connected it before the game loop's state-change check.

The browser lists actual LAN discovery results. Direct Internet is by address;
there is no public directory or relay UI. Password/address/name/port edits stay
in the game screen. Native stock art is reused; no new image assets or paid
external image calls occurred. Per-task billing is not exposed by these checks.

Windows object-only compilation passed for GameLobby.cpp, GameOptions.cpp,
menus.c, and menu_items.c using separate intermediate directories. Whole-game
linking and Android compilation belong to the parent integration pass. Runtime
controller/touch layout and complete races remain for user playtesting.

Final frontend review on 2026-10-25: native Ready affects only its own local driver;
host Start before readiness no longer readies the whole party. Loading/connecting
states ignore readiness and setup shortcuts. Explicit F1 requests open Options
before racing-state lobby/standings input, instead of being discarded. Compile-time
checks cover the session-action rules and four-viewport pause ownership/edges.

Focused evidence: GameLobby.cpp and GameOptions.cpp isolated MSVC compilation,
79,053 lobby model checks, Options and pointer-menu CTest suites, and the two pure
tests in `tests/kart_assets/test_lobby_menus.py` passed. The opt-in `--run` path was
not used. Online item and Battle rows query the backend's `net_capabilities()`
bits independently. Enabled capabilities expose the ordinary item choices and
remove the Battle pending label without another frontend edit. Local features
remain available independently; Battle CPU fill remains off.
GP and VS both display the shared Laps setting; Battle uses Balloons instead.
Backend mode normalization clears Battle mirror mode and CPU fill.

Main-menu revision, 2026-10-25: three 64x54 tiles (1P, Local, Online) reuse the stock
1P art, Game Select heading, selection arrow and flash. Four padded corner badges
only join/leave; character, transmission and readiness edits stay in the lobby.
1P locks the local party to its primary controller. Local preparation no longer
auto-starts: it enters an unready lobby. Switching 1P GP/VS skips Battle directly,
retaining CPU-fill/mirror settings. Geometry and input-routing regression checks
run in this suite; the opt-in Python runner follows the revised pointer IDs and
explicit local lobby. No game was launched for this revision. Stock assets were
reused; paid image/service credits: zero. Per-task token billing is not exposed.

Driver selection revision: `DriverSelectModel.h` keeps the eight stock portraits
in one shared grid, then anchors each local player's Automatic/Manual, confirmation
and Ready prompts to their race viewport. One player uses the full image, two use
top/bottom halves, and three/four use quadrants. Gapped physical controller ports
map to compact local viewport slots; remote racers do not add local regions.

The pure driver-select target covers geometry, all party masks, confirmation
barriers and Back transitions (28,680 checks). A stage change ends that frame's
port processing. Local choices include pending party intent; remote ownership is
read from the roster. Per-party request acknowledgments keep stale lobby updates
from overwriting unacknowledged choices. Authoritative loading cancels pending
profile edits. This advances the application protocol to v4; both devices need
the matching application. No game was launched by these checks.

The bridge exposes `menu.lobby.driver_selection` with stage, physical controller,
compact slot, lock state and viewport bounds. Its existing per-port input queues
can drive each player independently when gameplay testing is requested. The
opt-in Python runner follows the shared grid and per-player prompt targets.
Stock portraits were reused; no ComfyUI or paid image/service jobs were needed.
