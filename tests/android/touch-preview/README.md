# Touch Art Preview

This opt-in Android module compiles the production TouchControls and ShifterGate
sources and their actual resources. Its Activity replaces only the four JNI
input/state calls with an in-memory fixture. It does not emulate game physics.
No ROM, native library, audio, OS input injection, or screen capture is used.

From `android`, run `gradlew -PtouchPreview :touch-preview:assembleDebug`.
Install `tests/android/touch-preview/build/outputs/apk/debug/touch-preview-debug.apk`
on an Android emulator, then launch
`dev.spaghettikart.touchpreview/dev.spaghettikart.phone.MainActivity`.

The Activity renders the production View offscreen at five landscape dimensions
(3088x1440, 2400x1080, 1920x1080, 1280x720, 1024x768), both transmission modes
and both HUD layouts. It runs the existing app-owned gesture checks, verifies
pressed pixels differ, and checks that the central driving view stays transparent.
PNG overlays and `art-report.json` appear in the app's external files/kart-test
folder. These are actual Android Canvas images, not approximated mockups.
When composited with a previously captured game frame, label that image as an
art preview, not a new on-device gameplay capture.

The normal phone package does not include this module or fixture.
Full device/native tests remain in `tests/kart_assets/test_phone_shifter.py`.
