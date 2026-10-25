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
