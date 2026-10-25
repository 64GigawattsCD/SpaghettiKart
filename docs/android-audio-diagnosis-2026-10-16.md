# Android Audio Diagnosis

The reported crackling/stuttering was reproduced as audio queue starvation on the
Samsung SM-S908U, Android 16, over USB `R5CT607CKFX`. The app uses SDL2's OpenSL ES
backend, stereo S16 at 26800 Hz. The device was routed to its speaker during
inspection. All automated playback stayed muted.

## Cause and Repair

The original worker waited for a game frame before synthesizing two audio blocks.
Its maximum supply was 896 stereo frames per game update, just 33.43 ms of sound.
The phone averaged 38.74 ms between updates in the baseline run. Even ideal 30 Hz
gameplay only allowed 80 extra samples per second to recover from a stall.

Android now refills according to the playback queue, polling at most every 8 ms
between game notifications, after the first initialized game frame. It stops
adding audio once the existing 1100-frame target is reached; paused output thus
does not accumulate unbounded audio. A null backend retains game-frame pacing.
No device sample-rate change, interpolation setting change, or larger synthesis
chunk was needed. Eleven 448-sample ticks followed by one 432-sample tick, each
used twice per block, maintain an average 26800 samples/second at 30 blocks/sec.
Desktop retains its original frame-driven scheduling.

The mixer now applies volume before narrowing and saturates the final float mix
to S16. Previously, it narrowed the custom-audio signal before scaling and added
the native signal without saturation. This was an additional code defect, not
the reproduced cause: both phone recordings had zero pre-output overrange samples.
HMAS channel/effect/registry access is serialized between game and audio threads.

[SDL's queue documentation](https://wiki.libsdl.org/SDL2/SDL_QueueAudio) explains
that missing queued audio is replaced with silence. Queue depth is software-buffer
state, not a measurement of what the speaker has already played, as noted in
[SDL_GetQueuedAudioSize](https://wiki.libsdl.org/SDL2/SDL_GetQueuedAudioSize).

## Phone Evidence

| Measurement | Baseline | Repaired |
| --- | ---: | ---: |
| Measured blocks after warmup | 810 | 1140 |
| Empty queue observations | 553 | 0 |
| Mean interval between sound blocks | 38.740 ms | 33.330 ms |
| Minimum queued stereo frames | 0 | 128 |
| Maximum observed mix time | 3.278 ms | 6.337 ms |
| Pre-clamp overrange samples | 0 | 0 |
| Recorded PCM bytes | 2150400 | 2144000 |
| Normal settings unchanged | yes | yes |

Evidence folders: `build/android-phone/audio-baseline` and
`build/android-phone/audio-fixed-repeat`. Each includes reports and bounded
pre-output PCM; the latter also contains a completed driving queue and native
framebuffer capture. The first repaired attempt had zero empty queue observations
but its input heartbeat expired, so it is not counted as the completed regression.

The separate muted gameplay run in `build/android-phone/audio-gameplay-verified`
passed all 13 gameplay/config checks and all 17 touch/icon checks. Driving moved
338.965 game units; forward-view/velocity alignment was 0.9999426. The native
framebuffer/touch-overlay composite was inspected. No camera, mesh, controller
mapping, or touch layout changes were part of this repair.

The installed APK SHA256 is
`0acbbee94fdc12985a0623dd6f724179ea11e937d64c8f5d7f6afaa9eec5f2db`.
The test app closes after each run and normal preferences are restored.
Listening on the user's usual speaker/headphone route remains the final check;
these muted tests do not establish Bluetooth or physical speaker quality.

## Regression and Compilation

`tests/audio` checks positive/negative saturation, volume ordering, nonfinite
input, native-bus preservation, exact long-run sample budget, bounded queue depth,
and a model of slow rendering plus device pause/resume. The CTest suite passed.
`tests/kart_assets/android_client.py audio` enforces zero empty-queue observations,
nonzero pre-output PCM, at least 600 measured blocks, and unchanged normal settings.

The initial simultaneous PC/Android compilation exhausted virtual memory.
Gradle's worker count did not cap Ninja's native compiler jobs. The successful
Android path compiled native targets first with explicit concurrency, then packaged:

```powershell
cmake --build android/app/.cxx/Release/5ps2i6q6/arm64-v8a --target Spaghettify SDL2 --parallel 2
# From android/, with JAVA_HOME pointing to Android Studio's jbr:
.\gradlew.bat assembleRelease --no-daemon --max-workers=2
```

The `.cxx` path is this checkout's configured output and may change after toolchain
or configuration changes. No global machine/compiler settings were altered.

PC Debug also compiled successfully. After the memory failure, a compiler/PDB wait
required terminating only this task's compilation process tree and retrying with
process-local `_CL_='/Z7 /MP2'`, `/p:CL_MPCount=2`, and `/nodeReuse:false`.
No PDB service or unrelated process was terminated. The muted OpenGL/DirectX11
gameplay suite passed in 63.7 seconds; current evidence is linked by
`build/kart-game-test/verification.json`. Synthetic stereo is renderer coverage,
not a live-headset check.
The controller-default suite also passed three tests / five muted boots in
56.6 seconds, covering fresh bindings, migration, idempotence, and custom maps.
