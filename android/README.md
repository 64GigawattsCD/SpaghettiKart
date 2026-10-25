# SpaghettiKart Phone

ARM64 Android 10+ edition with no OpenXR loader or headset requirement.
The first-person cockpit is the same character-painted mesh used on desktop.
The launcher is `SpaghettiKart Phone` (`dev.spaghettikart.phone`).

## Compile

Use JDK 17+, Android SDK 35, NDK 27.2.12479018 and CMake 3.31.6.
The SDK location comes from `ANDROID_HOME` or ignored `local.properties`.

```powershell
cd android
.\gradlew.bat assembleRelease --no-daemon --max-workers=6
```

Output: `app/build/outputs/apk/release/app-release.apk`, signed with the local
debug key for personal sideloading. No ROM or extracted Nintendo assets are in
the APK. Desktop asset extraction must have completed first.

## Install And Test

From the repository root, with an authorized phone connected:

```powershell
python tests/kart_assets/android_client.py deploy --serial DEVICE
python tests/kart_assets/android_client.py test --serial DEVICE
python tests/kart_assets/android_client.py launch --serial DEVICE
```

Deployment installs with `-r` and copies the existing local `mk64.o2r` and
`spaghetti.o2r`. It never uninstalls an app or clears saves. A signing mismatch
stops deployment. Use `--assets DIRECTORY` or `--apk FILE` for other paths.
Normal play uses `/sdcard/Android/data/dev.spaghettikart.phone/files/`.

The native test bridge is enabled only by an explicit `kart_test` activity
extra. It queues controller states inside the game and reads its framebuffer,
with the same three-second input heartbeat as desktop. Tests mute all audio
buses before audio starts, back up/restore settings, and close the test app.
Test configuration is copied into `kart-test/spaghettify.cfg.json`; asynchronous
saves only touch that copy. The runner checks the normal profile is unchanged,
waits for process exit before restoration, and launches a fresh Activity task
with explicit test flags to keep test and normal launches separate.
Reports and PNGs go to `build/android-phone/`. The screen must be unlocked.
Renderer captures do not include the separate Android touch-control overlay.
The test renders that app-owned View to another PNG and composites it with the
game readback for layout QA, without operating-system screen capture.

## Controls

Touch controls: left thumb stick steers and navigates menus. The gauge accelerates
or confirms; stop sign brakes or goes back; bent arrow hops/drifts; lightning bolt
uses items; pause bars pause; camera-switch icon toggles the cockpit.
Multitouch allows simultaneous steering, throttle and drift. Backgrounding or
losing focus releases all touch input. SDL Bluetooth/USB gamepads remain enabled.

Physical gamepads use the PC XInput layout through SDL/ControlDeck, independently
of the touch layout: RT throttle, LT brake, LB drift, RB or B item, A jump/confirm,
B menu cancel, Select/Back cockpit, Y or R3 look behind, Start menu. Left stick steers; the other
stick looks. Gamepad events route at Activity level, even when an Android overlay
has focus. Existing controller preferences are retained.

GameSir X2s Type-C (USB3537:0105) is recognized by SDL's Android mapping without
an external database. Its extra GAS/BRAKE axes alias LTRIGGER/RTRIGGER; SDL uses
the latter pair. Regression command, with the device attached and phone unlocked:

```powershell
python tests/kart_assets/android_client.py controller --serial DEVICE
```

This opt-in test sends app-local Android events through the attached device's
profile and checks the resulting native ControlDeck pad before gameplay input
is isolated. It puts focus on the overlay and filters physical controller events
only during the self-test. All53 button/stick/trigger-pressure/release checks
passed on the S22 Ultra; evidence is in
`build/android-phone/gamesir-final-controller/controller-selftest.json`. It does not
claim automated physical switch actuation. Audio stays muted and settings restore
afterward, like the gameplay test.

## Source Attribution

Android Gradle bootstrap, SDL Java glue and launcher icon imported from
SgtBilko76/SpaghettiKart-VR `spaghettikart-vr`, revision `0b5ab4eeaf`.
The shared-library and Ogg/Vorbis setup follows that port. SDL's license is
included beside its Java sources. No Quest/PICO manifest entries, permissions,
OpenXR dependency, VR patch or donor kart art are included. Native rendering
uses this repository's existing libultraship revision and GLES3 backend.

The phone Activity, touch controls, native pad adapter and ADB test client are
local additions. First-run settings select first person; later launches retain
the user's preferences.

Touch action icons are Android vector adaptations of
[Lucide](https://github.com/lucide-icons/lucide), retrieved 2026-10-14:
gauge, octagon, corner-up-right, zap, pause, switch-camera. License and attribution
are included in the APK at `assets/licenses/lucide.txt`. Touch hit areas and
controller bindings are independent of the icons. Action names appear on hover.

The Select-camera/B-item defaults include a one-time update for saved default
bindings. X no longer toggles the camera; Select no longer takes screenshots
(keyboard F12 remains). Unrelated custom mappings and later remaps are retained.
Muted desktop regression: `python tests/kart_assets/test_controller_defaults.py -v`.

## Verified Device

2026-10-14: S22 Ultra SM-S908U, Android16/API36. All twelve runtime/pixel
checks and eleven app-local touch checks passed. Evidence:
`build/android-phone/verified-20261014/verification.json`. Actual race movement
338.965 units, forward/camera alignment0.9999426, eight paint colors, both camera
views, nonblank renderer output, no VR runtime, matched landscape touch layout.
Normal play settings restored and normal launch confirmed afterward.

The SDL orientation callback is explicitly kept in sensor-landscape mode.
The Activity, not ADB, owns creation of the test directory. Initial runs found
both issues; final verification uses the corrected APK. Physical Bluetooth
controllers and sustained thermal/frame-time testing remain untested.
