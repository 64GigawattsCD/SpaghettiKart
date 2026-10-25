# First-Person Cockpit and Direct Tests

Checkout: `E:\SpaghettiKart-VR-refresh`, branch `codex/vr-refresh-oct13`.

## Kart

The runtime now draws our own low-poly pipe-frame cockpit in the existing desktop
first-person and VR first-person path. Third-person still uses the original racer
sprites. No borrowed Amped Up art or driver model is included.

The previous Blender draft remains untouched at
`E:\SpaghettiKart\artifacts\first-person-kart-screens\meshy_kart_retopo_source.blend`.
Its frame dimensions and wheel spacing informed the new geometry. The authored
model has a tubular chassis, nose/side panels, seat, pedals, engine fins, exhaust,
four tires with hubs, and a separate steering wheel. Paint follows the player's
character ID; tires, seat, floor, metal and hubs keep their own materials.

- Runtime: `src/first_person_kart.c`, `src/first_person_cockpit.inc.c`.
- Reproducible author/export script: `tools/kart_assets/author_cockpit.py`.
- Editable assembled Blender scene:
  `build/kart-tools/cockpit-final/pipe-frame-cockpit.blend`.
- Inspected preview: `build/kart-tools/cockpit-final/cockpit-preview.png`.
- Geometry budget: 5,992 rendered triangles including all four wheels, versus
  53,108 in the previous frame/wheel combination. The old generated includes stay
  on disk but are no longer included by the renderer.

The script uses Blender geometry APIs and the established C-display-list batching
pattern, with baked vertex shading multiplied by runtime paint. It writes the new
runtime include explicitly; it is separate from the generic staged Fast64 exporter.
The saved scene is assembled for editing/inspection; regeneration is script-based,
not a round-trip export of arbitrary edits to that scene.

Wheel steering and steering-wheel rotation follow the player's steering input;
wheel roll advances once per game tick. Cockpit matrices have their own interpolation
scope, and the draw restores common display state for later track/particle rendering.
The desktop first-person near plane is 0.35 units to retain nearby cockpit geometry.
Camera position, heading calculations, driving physics and VR head transforms are
unchanged. VR retains its existing seat/scale controls and action-camera behavior.

## Direct Test Integration

`src/port/KartTestBridge.cpp` is inactive unless `SK_TEST_ROOT` names a test folder.
It consumes complete JSON input queues directly before the game's controller
edge/axis processing. Physical gamepad/VR input is not merged into these instances.
No desktop input injection, window activation or screen capture is used.

The launcher copies asset archives into a fresh runtime directory, sets `SHIP_HOME`,
mutes all four game audio buses before launch, and disables controller rumble. It
leaves normal play settings and saves intact. Test quick-boot selects Luigi Raceway
deterministically. A missing client heartbeat releases all queued holds after three
seconds; explicit stop releases them on the next controller tick.

Renderer readbacks happen before desktop presentation or per-eye submission.
Raw captures are PPM; the client writes lossless PNG copies. State reports contain
native position, velocity, camera/look vectors, kart pose, character, mode, input,
queue position, step start/end ticks and per-capture state. Windows report-file
contention retries on the next tick rather than terminating a valid queue.

```powershell
python tests/kart_assets/test_game.py
python tests/kart_assets/test_toolchain.py
ctest --test-dir build/vr-refresh-tests -C Debug --output-on-failure
```

For adaptive individual runs, use a new runtime folder:

```powershell
python tests/kart_assets/game_client.py launch E:/SpaghettiKart-VR-refresh/build/kart-game-test/manual-new
python tests/kart_assets/game_client.py state E:/SpaghettiKart-VR-refresh/build/kart-game-test/manual-new
python tests/kart_assets/game_client.py run E:/SpaghettiKart-VR-refresh/build/kart-game-test/manual-new --plan tests/kart_assets/palette-plan.json
python tests/kart_assets/game_client.py stop E:/SpaghettiKart-VR-refresh/build/kart-game-test/manual-new
```

`stop` ends the input queue, not the process. `process.json` identifies only the
isolated instance for shutdown. The automated suite owns and closes its processes.

Plans have an alphanumeric/hyphen/underscore ID and 1-128 steps, with at most 9,000
controller ticks total. Each step has `frames` (1-1,800), optional `buttons` bitmask,
`stick_x`, `stick_y`, `look_x`, `look_y` (-80..80), and a capture basename. Buttons
use native masks, e.g. 32768 for accelerate. Test-only `character` switches the live
ID for rendering checks, not a full character-select/stat initialization. `view`
selects desktop chase/first-person (0/1); `vr_view` selects an existing VR mode.
The Python client maintains the heartbeat while awaiting completion.

`stereo: true` renders synthetic eyes through the real `RunVrEye` interpreter in
an OpenGL desktop test. It verifies stereo geometry and capture plumbing, not
OpenXR tracking, compositor submission, controller hardware or headset comfort.
The synthetic matrices are isolated from normal play. Add `--vr` to the launcher
for an actual connected-headset test; active VR captures both native eye targets.
No live headset test was completed in this pass.

## Android Audio Probe

`python tests/kart_assets/android_client.py audio --serial DEVICE --output FOLDER`
runs a bounded, app-local driving queue with sound synthesis enabled but every
PCM sample zeroed immediately before submission to the output device. It never
plays the captured recording. The Activity requires both `kart_test` and
`audio_test` extras; ordinary tests still mute all buses. Normal preferences remain
separate from the test profile and the runner closes its app instance afterward.

The audio thread records at most 600 sound blocks to `audio.wav`, after 60 warmup
blocks, and publishes `audio-report.json` with queue depth, empty-queue observations,
refill intervals, mixing time, pre-clamp overrange count, and nonzero PCM count.
The client requires at least 600 measured blocks, nonzero recorded sound, unchanged
normal preferences, and zero empty-queue observations. The WAV is pre-output audio;
it does not include any silence inserted later by SDL or test phone speakers or
Bluetooth. File recording is inactive during normal play.

Focused mixer and queue-model checks:

```powershell
cmake -S tests/audio -B build/audio-tests
cmake --build build/audio-tests --config Release
ctest --test-dir build/audio-tests -C Release --output-on-failure
```

## Evidence

`build/kart-game-test/verification.json` links the most recent complete runs.
Both backends test eight paint hues, unchanged floor material, visible steering,
first/third-person switching, forward movement, forward-view/velocity alignment,
heartbeat expiry, explicit stop and malformed-plan rejection. The driving segment
moved about 299 game units with forward alignment approximately 0.99994.
OpenGL additionally captures two left/right image pairs through the stereo path.
The suite checks actual pixels, not just process liveness.

Final verification on 2026-10-13: Debug compilation passed; the two-backend native
suite passed in 61 seconds; five Blender/toolchain tests passed; existing binary-angle
regression passed. Final native evidence folders are
`build/kart-game-test/OpenGL-20261013-234448` and
`build/kart-game-test/DirectX11-20261013-234520`. All owned game processes closed.

Remaining verification: a seated headset session with the user's current VR
scale/forward settings, head movement, recentering, comfort, and controller/wheel
hardware. Race effects such as lightning shrink and all split-screen combinations
were not exercised by this focused pass.
