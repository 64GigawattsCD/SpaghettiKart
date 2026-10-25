## Lean Project Context (2026-10-25)

- Startup now uses root `AGENTS.md`, the bounded living `docs/current-state.md`
  and `tools/dev/context.ps1`; this file remains a historical lookup, not a
  default full read. Source/topic routing is in AGENTS; recipes in tools/dev.
- Context helper reports both Git repositories without fetch/device access and
  retrieves one historical section. Native command helper retains full logs in
  ignored `build/agent-logs`, prints one success line or a bounded failure tail,
  and propagates exit codes. Offline helper assertions cover both result paths.
- Added a local AGENTS pointer in the OneDrive launcher workspace. Existing
  staged mods there remain untouched. No global model/tool configuration changes.
- Corrected outdated Android README control prose to match the radial layout.
- No gameplay or phone tests for this setup. Account credit snapshot 9736.194195,
  weekly usage 12%, account-wide rather than a task billing measurement.
- Verification: 23 helper assertions passed (success/failure codes, bounded
  output, stderr capture, literal arguments, working directories with spaces,
  missing executables and ambiguous note queries). Existing compiled offline
  HUD suite passed through the wrapper; no game compilation was needed.

## Publication Checkpoint (2026-10-25)

- User confirmed "All working" after testing the latest phone controls, then
  requested all accumulated work be committed and pushed to the user forks.
- Current integration: `codex/vr-refresh-oct13`, including Android, cockpit,
  audio, HUD, LAN prototype, touch/mouse menus, controls, tests and tooling.
- Original VR draft remains separate on `VRintegration` in `E:\SpaghettiKart`;
  its renderer changes and Blender source assets are included in that checkpoint.
- Excluded 123 duplicate extraction headers in `include/assets/assets/`; every
  file was hash-identical to its existing canonical `include/assets/` counterpart.
  Compiled output, local settings, ROM archives and credentials remain local.
- No gameplay tests this pass; the user's latest confirmation is the test result.
- GitHub rejected private author emails in four unpublished renderer commits.
  Replaced them with the user's existing GitHub no-reply identity, preserving
  code trees, authors, dates and messages. Updated unpublished parent commits
  and game gitlinks accordingly; pre-existing published history is unchanged. Local backup
  refs under `refs/archive/pre-publication/` retain the original commit IDs.
- The original Blender sources are stored losslessly in split 7z archives on
  `VRintegration`, since GitHub rejects new LFS uploads to this public fork.
  Archive integrity was checked; source hashes and extraction notes are included.
- Publication succeeded for both current/refreshed game and renderer branches
  and the original draft. Verified renderer trees exactly match their originals;
  refreshed game history differs only in renderer gitlinks, and the final game
  tree exactly matches the checkpoint. A Windows stdin line-ending issue in
  the initial conversion was corrected and rechecked before completion.
- No paid external asset calls. Account credit snapshot: 9736.194195; weekly
  usage 11%. This is account-wide, not a per-task charge or usage measurement.

## Radial Pedals And Camera Placement (2026-10-16)

- User requests implementation first, no autonomous tests; user will test.
- TouchControls now draws annular Simon-style sectors around center gas:
  Jump north (new Lucide arrow-up icon), Drift east, Brake south, Item west.
  Gas latches only on direct center down and releases when that pointer lifts,
  regardless of drag. Ring sectors engage on entry/down, release on exit/up;
  separator gaps release. One gas finger can hold gas plus its current sector.
  Other free pointers can slide into ring; owned steer/look/shifter/utility
  touches cannot accidentally activate it. Cancel/hide/focus release clears all.
- Camera at inward minimap edge (~42% screen height), rear-view button above.
  Manual gate moved left of ring. Controls-inward/HUD-outward default retained.
- Place-number edge padding now uses the same scale/margin as lap times instead
  of separate width/height scaling. Quad origin accounts for animated rank size
  so its enlargement remains inside the corner inset.
- Updated opt-in future self-test cases for radial gas+action, drag-off, lift and
  ring-to-center without throttle. Not run, per user. Native Windows/Android
  compiled; release installed on SM-S908U and normal app reopened for the user.
  No gameplay tests or captures this pass. APK SHA256:
  20f182c1e31fb58bad478bf9bd10f27cab0f38b151ca3fdf5b92eb17f8fb3de1.
  No paid external calls, commit or push. Account credit snapshot12280.6278,
  weekly100%, account-wide across sessions, not per-task billing.

## Invisible But Functional Controls Root Cause (2026-10-16)

- User clarified that touch input worked but controls were not visible. Confirmed
  reproduction with a new compositor assertion, not merely View visibility or
  software rendering: build/android-phone/menus-20261016-162651 failed at race.
  Java reported VISIBLE, shown=true, draw_count1 and accepted touches, but the
  app window was absent from SurfaceFlinger's Output Layer list. SDL remained.
- Cause: hiding the overlay as INVISIBLE at startup let Android cache a full
  transparent region over the SDL SurfaceView. Switching to VISIBLE invalidated
  drawing but did not trigger the layout that refreshes that region. The display
  compositor consequently discarded the controls despite their rendered pixels.
  AOSP View.setFlags and ViewRootImpl.requestTransparentRegion confirm the path.
- TouchControls.syncTouchMode now calls parent.requestTransparentRegion(this)
  after visibility changes, refreshing composition for both showing and hiding.
  No input/classifier or menu rules changed.
- Test mailbox now additionally captures the app window buffer with PixelCopy.
  Phone runner checks both rendered controls and actual compositor inclusion at
  race/resume, and checks controls blank/hidden in menus/pause. It also retries
  an occasional incomplete telemetry read rather than indexing a null result.
- Updated APK installed SHA256:
  a542718544a113c3e5ae096e4289b152d4ffe4ffaf9aa31c2dc3e18fee8441c4.
- Fixed physical-phone run passed at
  build/android-phone/menus-20261016-162819/verification.json. Race and Resume
  now include the app window in compositor Output Layers; window-race.png
  inspected. Menu/pause overlay remains blank and hidden, driving after Resume
  passed, normal settings unchanged. All background tests muted and stopped;
  normal app reopened. No paid external calls. Account credit snapshot347.220796
  (weekly100%, account-wide across sessions, not task billing). No commit/push.

## Missing Phone Controls Investigation (2026-10-16)

- User reports controls absent when their race starts. Not yet reproduced/fixed.
  Earlier phone test asserted native touch_mode but not the Android overlay's
  actual visibility/drawing. Added checkpoint checks for host/mode/visibility,
  prior on-screen draw count and app-owned transparent overlay PNG pixels.
- Muted physical-phone GP route passed the stronger checks at
  build/android-phone/menus-20261016-161906/verification.json. Controls drawn
  during countdown/race and after resume, transparent/hidden in menus/pause.
  Inspected overlay-race.png. This does not establish that the user's route works.
- Installed diagnostic APK with sparse KartTouch native mode/window-owner logs
  and Java visibility/first-draw logs. SHA256:
  49f53ab50d088997fb9e85197bc3cd251ce86bde89a5c3fa5c86059baf70586a.
  Normal app relaunched (PID24457); asked user to repeat same race and name mode.
  Read adb logcat -d -s KartTouch after reproduction. Do not claim repair yet.
- New status mailbox is only active with kart_test + touch_dispatch_test extras;
  normal launch clears both. No input mappings/classifier behavior changed here.
  All background tests muted; normal settings unchanged. No paid external calls.
  Account credit snapshot818.252746, weekly100%, account-wide not task billing.

## Touch And Mouse Native Menus (2026-10-16)

- Added shared native menu hit targets and release-to-activate input. Pointer
  coordinates use the actual rendered image rectangle, including letterboxing.
  Modals clear background targets; stale/context-changing presses are cancelled.
  Mouse hover/press feedback uses the stock yellow accent. VR pointer path is
  excluded; controller navigation remains available.
- Main/player/character/transmission/course selection, options, records, pause,
  results, quit confirmation and custom race setup now accept pointer input.
  Transmission has a larger AUTO/MANUAL modal. View Options and Race Setup use
  six spaced rows per page with adjustment/page arrows and Back/Resume/Start.
  Course Random is tappable without overlapping Back. Direct sound row changes
  retain the original save-on-leave behavior. Legacy Pak dialogs have navigation
  arrows and Confirm; not every post-race/Pak branch has been played live.
- Android driving overlay appears only for an active human race, including
  countdown. Menus, pause, results, demos and port windows hide it. Native mode
  epochs clear held steering, camera, shifter/clutch and buttons on transitions;
  stale Java writes and old gestures do not restore them. JNI pad now takes six
  parameters including the mode token, superseding the prior five-parameter note.
- Added opt-in app-owned Android touch dispatch and native pointer/telemetry
  tests. No desktop/device-wide input injection. SDL resume replaces listeners;
  the test observer is installed per gesture. Phone tests use actual SDL touch
  exclusively; synthetic native pointer cancellation is tested separately.
- Passed native pointer unit test; native visibility 10,240 cases and 10,000
  concurrent transitions; Android View fixture 20 cases (including hide/release/
  re-entry), receipt build/touch-art/menu-preview/kart-test/art-report.json.
- Muted PC menu routes passed native pointer and image-mapped pointer paths:
  OpenGL 1600x900 final build/pointer-menu-test/OpenGL-20261016-160954;
  DirectX11 960x720 build/pointer-menu-test/DirectX11-20261016-160245.
  Extended options/data/quit-No/setup/View Options passed at
  build/pointer-options-test/OpenGL-20261016-160910. Setup/options screenshots
  inspected. Sound persistence is source-checked, not asserted by that runner.
- Final muted physical SM-S908U touch route passed at
  build/android-phone/menus-20261016-161055/verification.json: menus through
  race, pause, tap Continue, movement; driving overlay hidden in menus/pause
  and restored in race. Normal settings unchanged. Installed release APK SHA256:
  b5fba96720f885d9a368f6c50ea8acd6aeaf742df25a32253e30c839e5990fdc.
- Final visual review caught the records navigation covering Best Lap digits.
  Moved that row into the unused y100-119 band, added a geometry assertion and
  inspected the corrected capture. Data/records reruns passed OpenGL161355 and
  DirectX11-20261016-161426 under build/pointer-options-test. Other extended DX
  settings/quit checks passed DirectX11-20261016-161123. Latest installed APK
  with the records correction supersedes the hash above:
  ab224fe99d0cd38a06ac8323d3e3beebf2b40224b4fed718239cea4ef7bd4c32.
  Latest APK physical touch route also passed at
  build/android-phone/menus-20261016-161514/verification.json; normal settings
  unchanged. Test sessions ended, normal app relaunched for the user.
- All tests use isolated muted sessions. No commit/push or paid asset calls.
  Account snapshots credits 2498.730996 earlier, 1167.097796 at final check,
  weekly100%; account-wide across sessions, not task billing.
  External-service credits used0. Prior dirty work retained.

## Touch Camera Stick And Mirror (2026-10-16)

- Added independent camera joystick with eye icon beside/inward of the shifter,
  plus original rear-view mirror button. Existing first-person controller look
  behavior: horizontal/vertical angle, spring return; hold mirror for180 behind,
  release restores held stick angle or forward. No live mirror, no latched toggle.
  Third-person/VR camera math unchanged; these use the existing FP look path.
- Touch JNI now takes5 parameters and packs lookX/lookY into64-bit snapshot bits
  48/56; native merges into right_stick axes by magnitude like steering. Buttons
  retain32 bits, including neutral. All release/focus/layout paths clear camera
  axes. Networking already retains camera input locally, so no schema changes.
- Extended View self-test: look axes/deadzone/clamp, simultaneous steer/throttle,
  rear priority/release, independent lift/cancel, camera/button/HUD clearance.
  All20 Android emulator cases passed. Fixed old icon pixel test (pure white did
  not describe the gray pedals at smaller sizes): checks isolated symbols with
  light face/dark tread pixels now, eliminating false success from bright rims.
  Emulator receipt/previews: build/touch-art/camera-preview. Manual/auto inspected.
- New test_phone_camera.py (--serial or --desktop) covers10 native input steps:
  both yaw directions/pitch, rear override, restore look, recenter, drive with look.
  Muted desktop shared-camera test passed, including rear alignment below-.99:
  build/kart-game-test/camera-20261016-153358/verification.json. Rear image inspected.
- Android release installed successfully on SM-S908U via192.168.1.37:45159.
  SHA256:4b44b8f2975d9dbab9d7484a9c315aa9832ccd501629693ec4da4831d85bb00f.
  Phone remained Dozing; requested unlock. Native phone gameplay test timed out
  waiting for race readiness (build/android-phone/camera-20261016-153102), then
  closed app. Updated APK installed afterward. Next: once unlocked, run camera
  and shifter phone tests with --serial192.168.1.37:45159 (or current endpoint).
  Do not describe the emulator+desktop results as a completed phone driving test.
- Emulator closed, all test processes ended. No commit/push or paid asset calls.
  Account snapshot weekly100%, credits1098.963496, account-wide not task billing.

## Steering, Pedals And Minimap Revision (2026-10-16)

- User requested wheel/ring swap, wheel tilt with thumb movement, actual pedal
  icons, and a larger minimap. Full-size outer wheel now turns up to50 degrees
  with horizontal input; silver inner ring moves with the thumb. Release/cancel
  recenters. Input travel/mapping unchanged. Added left/right/return checks and
  app-owned View captures to the existing touch self-test.
- Original Blender tall accelerator and wide brake sprites replace gauge/octagon.
  Source render_touch_art.py; scene/receipt build/touch-art/pedals-v1. Existing
  colored rims retained. No ComfyUI/Meshy needed for this revision; paid calls0.
- Found minimap shrink cause: symmetric reservation mirrored the larger RPM
  block above the map, plus a down-only ScaleBox. Now fills the actual remaining
  gap, centered between the corner blocks at the same outer edge, with aspect
  ratio/marker padding unchanged. Phone Luigi map54.04 ->88.18 HUD units,63% taller.
  Corner blocks and touch controls remain in place. Shared PC/VR layout updated.
- Android and PC compiled; release installed on SM-S908U at192.168.1.37:45159.
  APK SHA256:b72e08a2502d53fbd2509edcce211588192c654cae24b453c7b533e74967cb03.
  Muted phone test passed31 touch/icon/layout/steering checks,15 native shifts,
  automatic mode and enlarged-map corner clearance. Normal settings unchanged.
  Evidence: build/android-phone/shifter-20261016-130210/verification.json;
  manual-gate-composite.png and steering-left/right-composite.png inspected.
  Phone app closed after tests, ready for user to open.
- HUD geometry passed540 viewport/track/margin cases plus hidden-corner/small-map
  fit checks. All8 muted desktop renderer cases passed: classic,wide,DX,ultrawide,
  horizontal/vertical two-player,three,four. Receipt:
  build/hud-game-test/20261016-130259/verification.json. Two-player visual inspected.
  No headset hardware test. No commit/push; prior dirty work retained.
- Account snapshot weekly100%, credits1371.543396. Account-wide observation,
  not per-task billing; external-service credits used0.

## Touch Controls Art Pass (2026-10-16)

- Android overlay restyled to echo stock HUD colors/bevels/dark outlines. Eight
  authored Blender sprites: six rims, steering wheel and gold shift knob. Existing
  Lucide action symbols retained; shaded ivory icons, lavender/gold gear numbers,
  recessed gate and gold clutch track. Centers stay translucent. No hitbox,
  binding, gate travel/tolerance or layout changes; preferred controls inward.
- Source: tools/kart_assets/render_touch_art.py. Rendered scene/receipt in
  build/touch-art/blender-v1; packaged PNGs in Android drawable-nodpi. Details in
  docs/touch-controls-art.md. ComfyUI concept was a color study only: symbols
  ambiguous. One failed checkpoint load/access violation, managed restart,
  successful32.567s retry. Graph/history in
  F:/AI/Workflows/projects/spaghetti-touch-20261016. Meshy unnecessary; paid
  external image/3D calls and Meshy credits0. ComfyUI remains running/idle.
- New opt-in tests/android/touch-preview module compiles actual production View
  and resources with JNI-only fixture. Twenty combinations (five sizes, both
  transmissions, both HUD layouts) passed gesture/icon/layout, held-state pixels,
  clutch art, empty driving center and edge fit. Java shifter100k movements passed.
  Evidence: build/touch-art/android-preview-final/art-report.json. Manual and auto
  art previews composite actual Android View renders with the previous native
  phone frame, NOT a fresh on-phone gameplay run. Visually inspected, including
  compact manual and held-clutch. Fixed overlapping translucent gate strokes by
  rendering each bevel layer as one Path. Hidden muted emulator closed afterward.
- Android release compiled successfully; APK SHA256:
  8933f8f447bcd8f0136e450b521563b99a632a6bfa67d6b5ce8efdf210aba5d3.
  Installed on SM-S908U via user-supplied192.168.1.37:45159. APK hash verified.
  Muted test_phone_shifter.py passed28 touch/layout/icon checks and15 native gear
  transitions including clutch hold/release, neutral and reverse. Automatic mode
  also passed; normal settings unchanged. Fresh phone renderer/View composite
  inspected: build/android-phone/shifter-20261016-124834/manual-gate-composite.png.
  Receipt: verification.json in that folder. Test app closed afterward.
- Account observation: weekly100%, credits2140.354246. Shared account reading,
  not task billing. Existing dirty work retained; no commit or push.
- Deployment follow-up account snapshot: weekly100%, credits1853.871746;
  account-wide observation, not task-specific billing. No paid external calls.

## Manual Touch Shifter (2026-10-16)

- Added Android ShifterGate and TouchControls manual-mode gate: 1/3/5 above,
  2/4/6 below, R at the end of the neutral crossbar. Vertical slots lock lateral
  motion; transitions cross neutral. Detents latch, partial releases settle neutral.
- User explicitly chose clutch-on-touch: grabbing/dragging holds clutch, finger
  release engages gear, no separate clutch button. Uses existing direct gear bits
  and transmission scoring. Cancel/focus loss clears inputs; steering is independent.
- Latest user tuning is applied: travel reduced20%, visible lanes2.18x wider than
  initial implementation, physical slot-entry tolerance doubled (normalized .5,
  scaled by .8). Knob grab radius retained. Controls inward/HUD outer retained.
- Native manual/gear state reaches UI through an atomic JNI snapshot. Android touch
  merges before test/network processing now, rather than after network submission.
  Native test bridge accepts all32 button bits (Neutral uses bit24) and reports
  validation failures with the submitted request ID. Keyboard/gamepad maps unchanged.
- Java gate tests passed all detents/masks, wide corners and100k movements. Phone
  app-owned View gestures passed28 touch/icon/layout checks; separate native queues
  verified15 gear/neutral transitions and clutch hold/release in a muted race.
  Evidence: build/android-phone/shifter-20261016-122322/verification.json and
  manual-gate-composite.png (visually inspected). Normal preferences unchanged.
- Offline phone regression passed13 checks/eight colors/camera alignment0.9999426
  in build/android-phone/shifter-offline-regression. Android release and PC debug
  compiled. Updated APK installed; SHA256:
  898ce20d8880171381efe238331a5424271b3750376095b8170244745b128e10.
  Phone closed after muted tests. LAN remains automatic-only; no manual LAN claim.
- Account snapshots during shifter work: weekly100%, credits609.731496 then
  163.083346 near completion. Shared account observations, not task billing.
  No paid external services. Existing unrelated dirty work preserved; no commit.

## Preferred Phone Layout (2026-10-16)

- User chose A: touch controls inward, HUD at the outer edge. Outer controls are
  too close to the phone edge for comfortable thumb reach. Retain A as the default.
- Connected phone has no gPhone.HudLayout override, so it already uses A (0).
  No phone settings edit, app restart or compilation needed. B remains optional.
- Account usage snapshot: weekly100%, credits968.234896. Shared account reading,
  not a task-specific charge. No paid external services used.

## LAN Integration And Phone HUD A/B (2026-10-16)

- Active checkout E:\SpaghettiKart-VR-refresh, codex/vr-refresh-oct13. Existing dirty
  work preserved. No commits/pushes. Research and schema/test receipt:
  docs/network-lan-prototype-2026-10-16.md. Old branch only had discovery/hello;
  fetched head remains e8351e29f. No ready network-racing fork established.
- Added pinned ENet transport, explicit bounded v2 codec, authority-owned physics,
  two participants with local-slot remapping, load barrier, input expiry, snapshot
  interpolation and reliable finish. One local viewport each, Luigi Raceway50cc,
  three laps, automatic, no items. No client prediction, eight-player, relay or GP.
- Screenshots caught a moving-client cockpit regression: tire contacts were absent
  from snapshots, then the camera used the post-physics center while contacts
  retained the pre-physics pose. Both contacts and oldPos now replicate; only the
  client adapter uses the matching camera anchor. Offline camera unchanged.
  LAN intro countdown is skipped at the shared start barrier; rank HUD lists two.
- PC protocol/loopback passed; two real PC processes passed movement/ownership/
  convergence/disconnect plus cockpit pixel evidence in
  build/network-game-test/20261016-115400. Real Wi-Fi phone/PC passed18 checks in
  build/network-phone-test/20261016-115703. PC192.168.1.23:54777, phone192.168.1.37,
  USBserialR5CT607CKFX. ADB transports test commands, not gameplay traffic.
- A/B phone setting: Settings > General > Phone HUD Layout. A controls inward is
  default; B HUD inward. Native bridge phone_layout accepts0/1. Both native views
  and touch overlays inspected, self-tests passed, normal preferences unchanged.
  Comparisons in that LAN directory's phone/layout-0-composite.png and layout-1.
- Offline phone all13 checks passed, camera alignment0.9999426, all eight colors,
  in build/android-phone/network-offline-regression. All eight desktop HUD renderer
  cases passed in build/hud-game-test/20261016-115449. HUD/angle/RPM/audio native
  suites passed. All games muted. No physical GameSir/VR test this pass.
- Final phone RPM regression passed32 captures with unchanged settings in
  build/android-phone/network-rpm-regression. Phone and task-owned PC test games
  closed afterward. git diff --check passed. No test process left running.
- Android non-VR APK installed; SHA256:
  11edb9aa1ebeb793bfd3a6e35cddceafea4652a2e7abfc87b9392652a7ed2ecb.
  PC executable is build/x64/Debug/Spaghettify.exe. Settings > General > LAN Race:
  Host onPC, Join192.168.1.23 onphone, Start Race onPC. Full three-lap hardware
  completion is still pending; reliable finish only verified in transport tests.
- Account snapshots: weekly100%; credit balance1519.328796 earlier in this turn,
  1013.782996 near completion. These are shared account observations, not exact
  task charges. No paid external service or asset purchase used.

## Windows Networking Prompt Audit (2026-10-16)

- User reported a managed-by-organization prompt from the network test and asked
  for removal of organization management. Paused game testing to inspect.
- `dsregcmd`: AzureAdJoined/EnterpriseJoined/DomainJoined/WorkplaceJoined allNO.
  Computer is WORKGROUP, not domain joined. No EnterpriseMgmt scheduled tasks or
  OMADM accounts found. Enrollment entries have no discovery/enrollment URLs or
  UPNs; built-in providers are not evidence of external enrollment.
- Elevated `gpresult` shows only Local Group Policy; the sole listed administrative
  setting is local Windows Update AlwaysAutoRebootAtScheduledTime=0. Not removed.
  Windows11Pro25H2,26200.9711. Craig is a local administrator; app process uses the
  normal medium-level token. No broad enrollment/account/OS component deletion.
- Firewall event2097 records local public-profile block rules added for
  network_test.exe at11:30:18 and Spaghettify.exe at11:36:17. Public Ethernet2 is
  active alongside Private Ethernet192.168.1.23. Private firewall was alreadyOFF;
  Domain/Public ON. Those profile states remain unchanged.
- With normal Windows administrator confirmation, ran
  `tools/windows/Repair-LanAccess.ps1`. Backed up firewall and legacy registry key,
  removed NC_PersonalFirewallConfig=0 (old Internet Connection Firewall prohibition),
  added exact-program private-subnet UDP54777 inbound exception. This legacy entry's
  role in the modern prompt remains unconfirmed; do not assert definitive causality.
  Existing public block entries remain. No broad firewall, UAC, or antivirus changes.
- Verified receipt Completed=true, exception active/private/UDP54777/LocalSubnet,
  and legacy entry absent. Backups and elevated audit:
  `build/windows-lan-repair-20261016`. Original dialog not captured/retested.
- Native network protocol suite passed. Initial two-process game test passed remote
  movement, ownership, snapshot convergence and input-ack checks, then hit a test
  runner bug: a completed queue left a stop command pending before the next queue.
  Fixed completed-queue cleanup. Local tests now bind loopback explicitly rather
  than listening on every interface. Phone APK compilation still in progress.

## RPM Needle Red-Screen Repair (2026-10-16)

- Reproduced the user's needle glitch on USB phone R5CT607CKFX. Muted native
  bridge queued 16 accelerating and 16 braking captures, six frames per step.
  Baseline braking capture `phone-ca8a4380db-brake-01-desktop.png` was 98.06%
  red across the full frame, 99.43% in the scene crop. Evidence directory:
  `build/android-phone/rpm-baseline`.
- RPM HUD components had no interpolation child identities. Their matrices
  were paired by operation ordinal with unrelated draws; the optional shift
  glow also changed the draw count. `draw_arcadekart_rpm_meter_leaf` now scopes
  each component by player and element, keeping glow/needle/face/readout apart.
- Needle's 310.8-degree start plus 100-degree sweep crossed 65536 through a
  direct float-to-u16 cast. New C-compatible `src/hud_rpm.h` narrows through
  uint32_t first, then wraps as an integer. NaN and out-of-range inputs clamp.
  Native RPM test covers wrap crossing, endpoints, clamping and 1000 sweep steps.
- Android Release and PC Debug compiled. Both CTest cases passed. Identical
  phone sequence passed all32 captures: maximum scene red2.80%, total red3.32%
  (normal scene/paint), versus baseline red spill. Acceleration and braking PNGs
  visually inspected; needle remains on the dial and rotates. Evidence:
  `build/android-phone/rpm-fixed/rpm-report.json`. The test checks capture count,
  scene/full-frame red spill, and unchanged normal settings.
- Muted full phone regression passed all13 gameplay/config checks plus touch
  self-test in `build/android-phone/rpm-gameplay-verified`. Camera alignment
  0.9999426. PC OpenGL/DirectX11 suite, including synthetic stereo, passed61.7s.
  No physical VR headset or GameSir hardware test this pass. No physics, audio,
  camera, mesh, controller-map or touch-layout edits. Phone closed after tests;
  normal preferences restored. Updated app installed on the phone.
- APK SHA256 `3e7fc9f5735ecae2d7874e2a90e19dc5f684b96cda445bc943ab7aa927a2070d`.
  No paid services or resets. Account-wide weekly usage96% at opening snapshot,
  97% near completion; credits0. These snapshots are not per-task billing.

## Android Audio And Network Audit (2026-10-16)

- User reported crackling/stuttering. USB phone R5CT607CKFX / SM-S908U / Android16
  uses SDL OpenSL ES at 26800 Hz; speaker route observed. Baseline audio followed
  slow game frames (38.740 ms average) while supplying only 33.43 ms per update:
  553 empty software-queue observations / 810 measured blocks after warmup.
- Engine.cpp now refills Android audio independently of renderer notifications,
  with an 8 ms queue check and existing 1100-frame target; first game frame still
  gates initialization. Native 448/432 sample budget averages exactly 26800 Hz.
  Null output retains frame pacing. Desktop scheduler unchanged. Mixer scales
  before narrowing and clamps the final S16 sum; HMAS state accesses are locked.
  No camera, mesh, controller-map, or touch-layout edits in this repair.
- Added AudioProbe opt-in via paired kart_test/audio_test Android extras. Captures
  at most 20 seconds of synthesized WAV, zeros all actual output, and writes queue
  telemetry. `android_client.py audio` requires zero empty-queue observations,
  nonzero PCM, >=600 measured blocks and unchanged normal preferences.
- Completed phone run: 0 empty observations / 1140 blocks, mean33.330 ms, PCM20.0s,
  no mixer overrange. Evidence `build/android-phone/audio-fixed-repeat`; baseline
  `audio-baseline`. Initial repaired attempt hit heartbeat expiry and was rerun.
  Phone gameplay regression: all13 checks +17 touch/icon checks passed in
  `audio-gameplay-verified`; native composite inspected. Phone left closed with
  normal preferences restored. Listening on usual output route remains pending;
  no Bluetooth/headphone listening claim. Physical GameSir was not attached.
- Android Release and PC Debug compiled. Native audio CTest passed; muted PC
  OpenGL/DirectX11 gameplay suite passed in63.7s, including synthetic stereo.
  Controller-default regression also passed: three tests / five muted boots in
  56.6s, covering Select camera, B+RB item, migration/idempotence and custom maps.
  Details and reproduction commands: `docs/android-audio-diagnosis-2026-10-16.md`.
  APK SHA256 `0acbbee94fdc12985a0623dd6f724179ea11e937d64c8f5d7f6afaa9eec5f2db`.
- Compilation initially exhausted virtual memory. Native Ninja requires explicit
  `--parallel 2`, separate from Gradle's worker cap. PC retry used process-local
  `_CL_='/Z7 /MP2'`, `/p:CL_MPCount=2 /nodeReuse:false` after stopping only the
  stalled task-owned compiler tree. No global settings or unrelated processes changed.
- Important network discovery: our remote `origin/NetworkedMultiplayer` is still
  e8351e29f (remote head verified). It contains ENet discovery/hello, not racing;
  none is in this active checkout. Parser accepts an unterminated 64-byte name,
  same-port hosts are misidentified as self, raw structs form the wire format,
  and host/socket failure/rebind states need work. Do not merge it wholesale.
- Original netplay wishlist is outside this repo:
  `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\docs\netplay-wishlist.md`.
  Direction: host authority, eight total racers, 1-4 local per machine, local-only
  viewports, prediction/reconciliation. First prerequisite is separating participant,
  connection, racer, local-controller, and viewport identities. Then repaired ENet
  loopback/LAN lobby and two-device race, before internet/relay/GP. Android currently
  lacks INTERNET permission. Full audit: `docs/network-online-audit-2026-10-16.md`.
  No networking code merged or changed; old branch findings are source inspection.
- No paid services or resets. Account-wide weekly usage93% at first recorded
  reading this pass,95% near completion; credit balance0. Not per-task billing.

## Select Camera And B Item (2026-10-16)

- Shared PC/Android defaults: Select/Back toggles first person, X no longer does;
  B and RB both use items. B still cancels menus. Removed default gamepad B from
  ceremony debug and Select from screenshots; keyboard G/F12 remain unchanged.
  Touch icons and hit areas unchanged.
- Engine.cpp migrates known saved defaults once per configured port, preserving
  unrelated keyboard/custom bindings and honoring later remaps through
  `SelectCameraItemDefaultsVersion=1`. Legacy missing-camera initialization also
  uses Select. Updated Android app-local controller test expectations.
- Desktop Debug and Android Release compiled. New muted native integration suite
  `tests/kart_assets/test_controller_defaults.py`: three tests / five app boots
  passed (fresh defaults, all-four-port migration/idempotence, custom mappings
  and post-migration remap). game_client accepts optional controller fixtures.
- Installed on phone over USB R5CT607CKFX. All13 gameplay/config checks and17
  touch/icon checks passed: `build/android-phone/select-camera-verified`.
  Native test profile inspection confirmed Select camera, B+RB item, B cancel,
  no Select screenshot. GameSir was not attached for physical button testing.
  Normal launch applies the same migration to the user's saved configuration.
- APK SHA256 `4aee742c54e02167a5a648e59d28f28f0dc1deb3a6d593f22c65c4e282a011e2`.
  No paid service/reset; account weekly90->91%, credits0; account-wide snapshots,
  not per-task billing.

## Phone Action Icons (2026-10-14)

- Replaced A/B/R/Z/II/CAM text in TouchControls with Lucide vector drawables:
  gauge accelerate, octagon brake, bent arrow hop/drift, bolt item, pause bars,
  camera switch. Positions, hit areas, held colors and input masks unchanged.
  Hover tooltips identify actions. Physical GameSir routing untouched.
- Six 24-unit Android vector resources under `android/app/src/main/res/drawable`;
  upstream Lucide license shipped in `assets/licenses/lucide.txt`.
- Installed APK SHA256
  `546880a2517a4a4e1f116b51d37c62c45bef6cf6ac48c6da443ab98274d79346`.
  Gradle release and diff check passed. Muted phone tests: all17 touch/icon
  checks and13 gameplay/config checks passed. Inspected native scene/View
  composite at `build/android-phone/action-icons-verified/phone-cockpit-with-controls.png`.
  Preferences unchanged during tests; normal launch afterward.
- No paid service/reset; account weekly20% at opening and closing, credits0.
  Account-wide snapshots, not per-task billing.

## GameSir Phone Input (2026-10-14)

- Attached device: GAMESIR GameSir-X2s Type-C, USB3537:0105, Android device12,
  SDL GUID `05008d123735000005010000ffff3f00`. SDL2.32.10 already recognizes it
  and supplies the correct axes/buttons. The unrecognized `Android Accelerometer`
  in earlier logs is not the GameSir; no controller database override is needed.
- MainActivity now routes gamepad keys and joystick motion to SDL before Android
  child-view focus handling. Same ControlDeck mappings as PC, no new input backend
  or camera math. RT throttle, LT brake, LB drift, RB item, A jump/confirm, B cancel,
  X cockpit, Y/R3 look behind, Start menu, left stick steer, other stick look.
- Added opt-in `controller_test` alongside `kart_test`, and client operation
  `python tests/kart_assets/android_client.py controller --serial DEVICE`.
  The connected GameSir profile supplies app-local Android events through Activity,
  SDL and ControlDeck; native reports sample the real pad before the test queue
  isolates gameplay. Focus deliberately sits on the touch overlay. Only this
  self-test filters concurrent physical input; normal play is unaffected.
- Initial baselines mixed incoming physical input with synthetic events. Final isolated
  run passed all53 button/axis/trigger-pressure/release checks:
  `build/android-phone/gamesir-final-controller/controller-selftest.json`.
  This is app-local pipeline evidence, not an assertion of every physical switch.
- Muted real-phone gameplay regression passed all13 runtime/pixel/config and11 touch
  checks at `build/android-phone/gamesir-final-gameplay`. Native scene inspected; movement
  338.965, camera/velocity dot0.9999426. Settings preserved by both test runners.
- Final settings audit found test mute/quickboot in the normal profile; restored
  the opening backup. Android tests now copy preferences to an app-owned
  `kart-test/spaghettify.cfg.json` and initialize Context with that separate path.
  Controller test verifies the normal profile stays identical during testing.
  Client waits for app exit before restoring and starts a fresh Activity task
  with explicit true/false test extras so singleTask launches are unambiguous.
- Installed updated APK in place. SHA256
  `eead97ea725d37178676906353f6c0e8cf18cecaa00bb519ef8b2f5b5fb88dc8`.
  Gradle release, Python compile and git diff checks passed. No desktop behavior
  changes. No paid service/reset; weekly account18->19%, credit balance0, shared
  with concurrent tasks rather than exact per-task billing.

## Android Phone (2026-10-14)

- Installed non-VR ARM64 `dev.spaghettikart.phone` / SpaghettiKart Phone on
  SM-S908U (S22 Ultra), API36, via Wi-Fi ADB. Initial APK:
  `android/app/build/outputs/apk/release/app-release.apk` (16,925,452 bytes),
  SHA256 `9cd94e249af7780e1db38734655165515cd1ccadc12df0321e9f4186ab383392`.
- Reused Quest fork `0b5ab4eeaf` Gradle/SDL bootstrap and Ogg/Vorbis setup.
  No OpenXR, donor art, ROM or extracted game data in APK. Local desktop o2r
  archives copied separately. libultraship and original E:/SpaghettiKart untouched.
- Added phone Activity, multitouch controls and coherent native pad snapshot.
  A/B also confirm/cancel; R drift, Z items, II pause, CAM cockpit. Touch holds
  release on pause/focus loss. SDL orientation callback must enforce landscape:
  its resizable-window default overrides manifest orientation with FULL_USER.
- Compile fixes: shared SDL2 despite other static dependencies, libultraship
  include outside C linkage, mobile menu header path, missing math_util/actors
  declarations in race_mods.c. No camera-heading or physics changes this pass.
- Direct runner: `tests/kart_assets/android_client.py deploy|test|launch`.
  Native queues/readback, muted before startup, settings backup/restore, closes
  test app. Activity must create `files/kart-test`: ADB-created subdirectories
  are shell-owned and deny app writes. Wait for user unlock; no OS input.
- Final real-phone evidence: `build/android-phone/verified-20261014`.
  All 12 runtime/pixel checks and 11 app-local touch checks passed. Drove338.965
  units, camera/velocity dot0.9999426, eight paint hues (43,455+ pixels each),
  first/third-person switching and released input. Phone nose ROI is lower than
  desktop4:3. `phone-cockpit-with-controls.png` composites native framebuffer and
  Android View drawing for layout QA; not an OS screenshot.
- Normal launch confirmed (PID27963 at handoff), first-person1/skipintro3,
  no quickboot or mute overrides. Foreground released to concurrent Trilogy
  task afterward; app remains in launcher. Physical fingers/gamepads and long
  thermal/performance sessions untested. GL+DX desktop suite passed61.525s,
  binary-angle CTest, APK signing, 16KB zip alignment and diff check passed.
- No paid API/reset. Account weekly12->15%, balance0; includes concurrent work,
  not exact task billing. Full workflow: `android/README.md`.

## VR Refresh (2026-10-13)

- Cockpit and autonomous-test follow-up: `docs/cockpit-and-autonomous-tests.md`.
  New own pipe-frame model uses the previous Blender draft's proportions, 5,992
  rendered triangles, eight character paint colors, steering and wheel roll.
  Runtime uses `first_person_cockpit.inc.c`; old generated meshes and original
  Blender file retained. Source: `tools/kart_assets/author_cockpit.py`; editable
  scene and preview in `build/kart-tools/cockpit-final`.
- Native test bridge: opt-in `SK_TEST_ROOT`, client `tests/kart_assets/game_client.py`.
  Direct queued game inputs, native state, pre-present renderer screenshots,
  watchdog/stop, isolated muted saves/config. Use this instead of desktop grabbing.
  Test suite: `tests/kart_assets/test_game.py`; evidence:
  `build/kart-game-test/verification.json`. OpenGL and DX11 checked; synthetic
  left/right rendering is not an actual OpenXR/headset test.
- First-person near clipping changed to 0.35 to retain nearby cockpit geometry.
  Camera heading/position, physics, mesh-independent view basis and VR head
  transforms were not changed. Live headset seat fit/comfort remains pending.
- Usage for this implementation: weekly account snapshot 9% to 11%, credits 0
  at both observations. No paid asset service or reset; not a per-task charge.

- Own-model tooling is ready: `docs/kart-asset-tooling.md`,
  `tools/kart_assets/toolchain.py`, and `open-kart-blender.cmd`.
  Project-isolated Blender 5.2.2 loads pinned HM Fast64 2.5.4 and Litronom's
  population addon. OverKartDecomp, Tarmac64 and Library are local source
  references under `.cache/kart-tools`, not linked game code. No donor art used.
- Empty starter: `build/kart-tools/starter/kart_authoring.blend`. Static export
  maps author (x,y,z) to game (x,z,-y), preserving handedness, and records pivots
  separately. Source scenes and game assets are never overwritten by export.
  Existing legacy wheel exporter and camera/driving code remain untouched.
- Five regression tests pass, including eleven Blender functional checks,
  reopening saved files, repeatable export bytes and unchanged input hashes.
  Headless POP popup is replaced with logging in our wrapper; fixed hash seed
  stabilizes Fast64 material flag ordering. Donor sources remain unmodified.
  No game launch this turn. Production textures, skeletal drivers, runtime
  integration and headset verification are still pending our actual models.
- Usage snapshot for tooling setup: account-wide weekly used percentage 5% at
  opening, 6% at close; credit balance 0 at both observations. This is not an
  exact per-task charge. No paid asset service, purchase or reset used.

- Amped Up deep research: `docs/amped-up-technical-research-2026-10-13.md`.
  Public OverKartDecomp has skeletal animation code; exact Amped Up racer code
  remains unlocated. Author confirms Blender custom exporter history and a
  time-consuming feasible PC port. Do not confuse population and animation
  exporters or infer that model axes explain our historical camera offset.
- Fork/character/3D research: `docs/fork-feature-survey-2026-10-13.md`.
  VRmicelli has a real procedural cockpit; SpaghettiPad has sprite-pack selection;
  Ricky has a Windows 5-8-player release. Amped Up has animated 3D racers but is
  a ROM hack, with source/mesh availability still unresolved. Research only.
- Active branch: `codex/vr-refresh-oct13` in `E:\SpaghettiKart-VR-refresh`.
- Follow-up details and donor revisions: `docs/vr-refresh-2026-10-13.md`.
- User preference: all background game tests must be muted. Use temporary test settings with
  `gGameMasterVolume = 0.0` (plus music/SFX/environment at zero); leave normal play settings intact.
- `tests/vr_refresh/smoke.ps1` runs muted desktop startup checks in isolated runtime directories.

## VR Refresh (2026-09-30)

See `docs/vr-upstream-refresh.md` for the refreshed upstream/VR sources, camera history, runtime location,
and pending headset tests. Active integration is in `E:\SpaghettiKart-VR-refresh`; original WIP remains
in `E:\SpaghettiKart`.

## Screenshot Capture

- For fullscreen SpaghettiKart captures, call `SetProcessDPIAware()` before reading screen or window bounds.
- Without DPI awareness, PowerShell reports the 4K primary as 2560x1440 and captures only the top-left portion of the window.
- For DirectX fullscreen, prefer screen capture with `System.Drawing.Graphics.CopyFromScreen`; `PrintWindow`/BitBlt window copies can return black frames.
- Save screenshots under `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\screenshots\...` and embed them in the reply with an absolute Markdown image link so they appear in the Codex sidebar.

## RPM Meter

- The RPM faceplate is supplied by `mods/zz-arcadekart-rpm-faceplate.o2r` as `textures/common_data/common_texture_speedometer.png`.
- The faceplate art owns the orange warning region now; do not add runtime color masks over the gauge face.
- The root `mods/zz-arcadekart-rpm-faceplate.o2r` and `build/x64/Debug/mods/zz-arcadekart-rpm-faceplate.o2r` copies both need the forward-slash archive entry `textures/common_data/common_texture_speedometer.png`; backslash entries lose to the HD pack on runtime lookup.
- The custom source art is 256x384, aspect 0.666667. Keep the in-game faceplate quad at 64x96 local units (`-32..32`, `-48..48`) with 4096x6144 texture coordinates; the previous 63x94 mesh was aspect 0.670213, about 0.53% horizontally wide.

## HUD Layout

- HUD positioning now uses an explicit layout safe zone: the root player view stays full-size for scale math, while selected canvas slots anchor inside `HudLayoutContext.safeRect`.
- HUD cluster scale and placement tuning is fixed in code now. The only live ArcadeKart HUD tuning CVar is `gArcadeKart.Hud.SafeZoneScale`, which globally scales edge insets.
- RenderModernize HUD visual target as of 2026-06-01: placement high top-left, portrait strip below/left, time/lap centered high, vertical lap split list top-right, minimap mid-right, RPM meter bottom-right. Latest useful full capture: `C:\Users\Craig\OneDrive\Documents\mk64 Arcade\screenshots\render-modernize-layout\RenderModernize-layout-final-pass-20260601-152150.png`.
- The one-player minimap and RPM meter are separate anchored groups now. The minimap constructs a scaled rect from the active track map, so larger map textures grow leftward. The RPM meter does not read minimap dimensions. Right-side minimap/RPM placement is fixed in code from HUD-reference insets now, so stale saved placement CVars do not move those groups.
- Right-side ArcadeKart HUD elements render into the post-FX HUD framebuffer. Use unchanged 320x240 HUD coordinates there; old wide helpers/matrices that call `OTRGetDimensionFromRightEdge` push right-side sprites out of the HUD framebuffer.
- Portrait strip uses safe-zone anchoring for center-left placement with code-side constants for scale, edge inset, spacing, and center offset.
- HUD micro-pass completed: placement left/top offsets moved inward, portrait strip edge offset increased, map vertical/padding defaults moved inward slightly, and top/lap cluster defaults re-centered to reduce manual slider churn.
- RenderModernize HUD cluster scale constants are 0.8 for portrait strip, time/lap strip, lap-time list, minimap/dots, and RPM meter; each cluster keeps its safe-zone-aware anchor while shrinking.

## HUD Timer/Lap

- `func_8004EB38` was the legacy translucent afterimage pass for `TIME` and `LAP`; keep it disabled while the ArcadeKart HUD owns the race timer/lap presentation.
- The top-center timer/lap strip uses native sprite rectangles for `TIME`, timer digits, `LAP`, and `1/3`; keep those as separate layout children so spacing can be tuned without distorting glyph proportions.
- The top-right lap split list must use `hud_layout_canvas_safe_slot` and the scaled timer glyph renderer. The legacy `print_timer` path ignores the intended layout width/right edge in widescreen and can clip against the right side.

## Minimap

- `draw_minimap_character` has four explicit marker paths: normal dot, rainbow dot, normal human kart icon, and rainbow human kart icon. The first-place paths use the same scaled texture-rectangle geometry as the non-rainbow paths so markers stay aligned.
- Live human minimap kart icons now use an isolated rotated path: a private centered 8x8 quad, `func_80042330_unchanged`, `D_0D007968` for tint-friendly minimap state, the RGBA16 kart texture, and an explicit `D_0D007EB8` restore. Do not go back to `func_80046AD4` / `common_vtx_player_minimap_icon`; that old path repeatedly caused a wide horizontal chunk of HUD/game rendering to disappear. The angle comes from `player->rotation[1]` plus a baked 180-degree flip. The marker draws a slightly larger black pass under the colored/rainbow pass so it keeps a black outline while tinted.
- The race minimap now has a single layout helper for the texture, finish-line marker, and racer dots. Keep all minimap world-to-HUD transforms inside that rect so map art dimensions do not leak into other HUD groups.
- Minimap placement must clamp the actual active map rect against the HUD safe rect after per-track width/height and cluster scale are applied. Do not anchor from a fixed/reference map size or only from the track-provided center; large/wide maps must move their left edge inward while their right edge stays inside the safe region.

## Controller Sequential Shift

- `Shift Gear Up` and `Shift Gear Down` are dedicated bindable commands.
- Default controller flow is right-stick click for `Clutch`, then right-stick up/down for sequential upshift/downshift; sequential shifts are ignored unless the clutch command is active.
- Keep the running controls list in `docs/control-change-log.md`.
- `Toggle HUD` now uses `KART_TOGGLE_HUD_BUTTON`; `BTN_CRIGHT` is legacy only, so stale right-stick-right/C-right mappings do not hide the HUD.

## Transmission Mode

- Manual/automatic is selectable per human player on character select with L/R. The small cursor label shows `MANUAL` or `AUTO`, and the toggle persists through `gArcadeKart.Transmission.PlayerNMode` defaults.
- AI racers remain forced automatic in `kart_transmission_is_automatic` until explicit AI manual-shift behavior is authored.
- The ArcadeKart options menu exposes simple per-player auto-transmission default checkboxes; normal race-flow selection remains on character select.
- `gArcadeKart.DebugQuickBootHudRace` is default-off again. When manually enabled, it skips straight into a 1P 50cc Mario GP race and chooses a random stock GP track each process launch. Normal character/transmission select is the default path.

## Render Modernize

- The race scene now renders into `gArcadeKartPostFxSceneFrameBuffer`; HUD rendering switches to `gArcadeKartPostFxHudFrameBuffer` afterward so post effects sit under HUD.
- The DX11 path uses HLSL pixel shaders in `Gui.cpp`: one for scene barrel/overscan/blur/shake, and one to composite the HUD framebuffer by keying the magenta clear color into alpha.
- The HUD framebuffer does not carry useful alpha when sampled by ImGui; plain alpha blending turns its clear color opaque. Keep the keyed HUD shader unless the framebuffer backend starts preserving alpha. The HUD compositor must use the point-clamp sampler and neutral-edge de-spill so the magenta clear key does not bleed into white HUD glyphs, minimap edges, portrait edges, or the RPM meter.
- Full-screen smoke capture after the HLSL pass: `artifacts/render-modernize-hlsl.png`.
- HUD is composited at full player-view size over the post-processed scene. Do not scale the final HUD framebuffer; it double-shrinks the HUD and does not recover pixels that were already clipped in the HUD source. The RPM meter is anchored as a complete panel with fixed code-side right/bottom insets, not by its visible gauge face, so its hidden texture bounds stay inside the source. The executable-side `build/x64/Debug/mods/zz-arcadekart-rpm-faceplate.o2r` must stay in sync with the root `mods` copy because the app loads mods relative to the executable directory.
- Saved transient framebuffer CVars can be stale during startup. `GetFramebufferTextureId` must return null for out-of-range ids, and the compositor clears `LayeredHudActive` when either scene or HUD framebuffer lookup fails.
- Screen shake uses `gArcadeKart.PostFx.ShakeStrength` as the mushroom/top-speed-ish amount, then scales it by speed: `gArcadeKart.PostFx.ShakeIdleScale` defaults to 0.05 at idle and `gArcadeKart.PostFx.ShakeFullSpeedRatio` defaults to 1.0 for full shake.
- Race menu directions no longer tune post-FX or wheel spring values. Post-FX tuning lives in menu CVars: normalized `gArcadeKart.PostFx.TestShakeSlider`/`TestWarpSlider`, raw-max remaps `TuningShakeInputMax`/`TuningWarpInputMax`, visual gains `TuningShakeStrength`/`TuningWarpStrength`, and response shaping via `SpeedMinRatio`/`SpeedMaxRatio` plus `ResponsePower`. Legacy debug lap skip is gated by default-off `gArcadeKart.DebugLegacyLapSkipEnabled`.
- On `FXpass`, scene framebuffer binding moved after race viewport setup because `race_begin_viewport` can select the normal framebuffer. Drift puffs also explicitly reset texture LUT/alpha state inside `render_player_drift_particles` before loading the old I8 drift textures.

## ArcadeKart Audio

- Custom bad-shift samples are copied by the `Spaghettify` post-build step from `assets/arcadekart_audio/` to the executable-side `arcadekart_audio/` folder. HMAS rejected the original Vorbis OGG files as `Invalid file`, so gameplay now loads PCM WAV conversions instead. A bad shift writes `GRIND_SOUND ... custom 1` to `logs/ArcadeKartShift.log` when the custom sample path is active.

## Texture Mods

- The active HD texture pack is `mods/mk64-reloaded-v2025.12.20-sk-hd.o2r` (`MK64-Reloaded-SK`). Track targeted overrides as a separate loose folder mod named `spiny-valley-porcupine-override`, currently containing `textures/tracks/yoshi_valley/yoshi_valley_data/d_course_yoshi_valley_hedgehog.png` and `textures/common_data/common_texture_speedometer.png`. Keep copies in both `E:\SpaghettiKart\mods\` and `E:\SpaghettiKart\build\x64\Debug\mods\` so the runtime loads it after Reloaded without modifying the large pack.
- Yoshi Valley hedgehog sprite flipping is camera-space now: each hedgehog/camera pair tracks its previous projected X and uses the mirrored quad only when it moves camera-left.
- World drift feedback text keeps a per-particle side anchor in `unk_010`. The tire origin and rendered card corner are intentionally crossed so the word sits outward from the kart: left drift uses the back-right tire and right drift uses the back-left tire, while the drawn card aligns the opposite horizontal corner to that tire point. The red/blue world marker overlay render call is removed from the normal pool path; lateral/backward movement still starts after age zero. The spawn gate is currently 8 logic ticks.
- Drift words are rendered from the separate ArcadeKart feedback pool. The legacy human-drift particle slot now keeps calling `setup_tyre_particles`, so normal tire dust stays visible while word cards spawn.
- World drift feedback words should only spawn while the player is actually drifting (`DRIFTING_EFFECT`, positive `driftDuration`, or positive `driftState`). Hard steering alone can still create baseline skid particles, but it must not create ArcadeKart `Slide` text.

## Particle Modernization

- `src/engine/particles/ArcadeKartParticleEmitter` now extends the existing C++ particle hook with parameterized emitters, player attachment, world/local space simulation, spawn-rate accumulation, bursts, lifetime, velocity, acceleration, alpha, and scale interpolation. It is dormant infrastructure; tire dust/exhaust/drift words still use their current paths until a specific migration pass.
- Boost exhaust sparkles now use `ArcadeKartBoostSparkEmitter`, two local-space emitters per racer attached at the same left/right exhaust stack offsets as the duplicated smoke. They key off shared `BOOST_EFFECT`, so normal/triple/golden mushroom boosts and any AI boost use the same replacement path. Their per-particle local velocity scatters evenly left/right, biases upward, and only travels backward from the exhaust. The old type-8 player boost spark renderer is intentionally a no-op while this emitter owns the visual.
- Good manual shifts call `CM_BurstArcadeKartBoostSparks`, which reuses the same boost exhaust spark emitters for a small one-shot burst without setting `BOOST_EFFECT` on the player.
- Drift/turbo tire sparkles now use `ArcadeKartDriftTireSparkEmitter`, two local-space rear-tire emitters per racer. They key off the same drift evidence as the word effect (`DRIFTING_EFFECT`, positive `driftDuration`, or positive `driftState`). The visible effect has a tire-local zap core from split `lightning_zap_0/1` half-cards plus a copied mushroom/exhaust spark layer with extra outward/up/back motion. Zap flip animation uses private flipped UV quads plus swapped split textures, never the old mirrored stock display list. The zap halves share the same stage tint: yellow for slide, orange for drift, rainbow for turbo drift. Spark-copy velocity is stored per particle but only applied while drawing the copied spark layer, so the zap stays tire-local. Drift tire sparkles skip the normal world particle pass and draw from `CM_DrawArcadeKartLateParticles` after kart rendering; do not reintroduce the camera-space nudge, because it breaks wheel alignment without fixing draw order. The emitter starts at the reported rear tire location with no extra inward/rearward local offset; keep only per-particle jitter for variety. Its local motion uses half of `player->unk_0C0` on top of body yaw so the sparks follow drift slip without over-rotating around the visible kart sprite. The text word effect remains human/owner scoped in the legacy feedback pool.
- The drift tire spark draw pass receives both `cameraId` and `screenId`. The zap/spark render position is a blend between the physical rear tire point and a screen-specific visual anchor reconstructed from the same card origin/orientation math used by `render_kart`, including the card tilt term from `unk_0CC` and the roll/yaw fields `unk_048`/`unk_050`. The sprite-local X sign is mirrored from the physical tire side because the rear-facing kart billboard projects player-left/player-right opposite from the tyre array's world-space side. This is intentional camera-facing alignment for the visible sprite tire, not a change to the dust/contact physics origin.
- New C++ particle draw paths must restore common display state with `D_0D007EB8` after drawing. The drift tire emitter's first pass used the mirrored stock card display list (`D_0D008E70`) for random horizontal flips and did not restore state; that reproduced the wide missing-HUD-band failure where center/right HUD groups disappear from the post-FX HUD framebuffer. Keep tire spark flips off or implement them with a private quad/UV path, never by swapping to the mirrored stock display list in this branch.
## HUD Checkpoint And Networking Handoff (2026-10-16)

- HUD geometry now uses per-player anchored lap/RPM blocks and a fit-scaled map
  centered vertically in the viewport. Layout engine fixes cover nested ScaleBox
  transforms, collapsed children, grid spans/layers, ownership and nested insets.
  Native HUD tests passed including 405 race-layout combinations.
- Muted renderer tests passed seven of eight cases before a DX client-area sizing
  issue was isolated. Final focused dx-wide/four rerun passed both, with captures
  in `build/hud-game-test/20261016-105717`. DX changes are in the dirty libultraship
  submodule; preserve them. Full eight-case rerun remains pending. Android native
  compile passed; this HUD version has not yet been packaged or installed.
- User shifted priority to autonomous LAN networking from the older branch.
  User also requested A/B testing phone controls-inward versus HUD-inward layouts;
  retain both, not a single hard-coded choice. Network work begins from the audit
  below; fetched origin/NetworkedMultiplayer remains e8351e29f.
- No paid services. Opening HUD account snapshot: weekly97%, credits0; account
  totals are not task-specific usage.
