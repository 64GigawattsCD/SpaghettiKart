# Current State

Updated: 2026-10-25. Living summary; replace stale facts rather than accumulating logs.

## Checkpoint
- Active work: `codex/vr-refresh-oct13` in `E:\SpaghettiKart-VR-refresh`.
- Android/UI, cockpit, LAN and renderer work was pushed to the user forks on
  2026-10-25. Verify live status; this sentence is not a cleanliness assertion.
- Finish holds five seconds after the last human, then translucent results over the live race.
- 1P/Local/Online menus retain four-player local parties and eight-racer sessions,
  full GP progression, LAN discovery, encrypted inputs and stock item replication.
  GP/VS and human-only Battle are enabled; see the topic doc for coverage and receipts.
- Shared native menus retain stock art and Options / Data / Back / Quit, with no
  dedicated L/R/Z shortcuts. Gameplay testing remains with the user.
- Main uses stock-sized mode tiles and padded corner join badges; drivers stay in
  the lobby for Local/Online. 1P goes setup > shared characters > transmission > race.
  Translucent native surfaces, texture focus and quiet checker/color accents are
  shared across menus. See `docs/native-menu-art-and-solo-2026-10-25.md`.
- Latest APK installed on SM-S908U on 2026-10-25 at 19:45; settings digest unchanged.
  App not launched during deployment. Rediscover the wireless endpoint next time.

## Keep These Behaviors
- Phone: controls inward, HUD outward. Lap times top-right; RPM/gear bottom-right;
  minimap fitted between. Place-number padding matches lap-time padding.
- Radial pedals: gas center, jump above, drift right, brake below, item left.
  Gas starts only on direct down and holds through drag until lift. Outer actions
  engage on entry/down and release on exit/up, including simultaneous gas+action.
- Camera stick beside minimap, rear-view above. Manual shifter follows a widened
  gate; touching the shifter acts as clutch. Action icons, not controller glyphs.
- Gameplay controls visible only during races. Android overlay visibility requires
  refreshing the parent's transparent region; View visibility alone is insufficient.
- GameSir matches PC: RT gas, LT brake, LB drift, RB/B item, A jump/confirm,
  B cancel in menus, Select camera, Y/R3 rear view, Start menu. Retain user remaps.
- Character-tinted authored cockpit is for first person/VR; chase uses sprites.
  Heading bugs concern view versus travel direction, not model orientation.
- Android audio uses queue-driven refill plus saturated mixing; desktop pacing stays.
- Native Options reuses PortMenu definitions; bindings are embedded, LAN is native.
  Live LAN menu input is local and driving-neutral; it does not pause the server.
  Details and remaining runtime checks: `docs/ui-unification-audit-2026-10-25.md`.

## Verified Versus Pending
- Native checks cover encrypted 4+4 and eight-system races, four-round network GP,
  ownership/readiness, loading/racing/finished reconnect and final world replay.
  Item/discovery/device-assignment/texture/lobby codecs passed. PC/Android final
  compilation receipts are in the topic doc; no new game/device test was launched.
- Public directory and NAT relay service are not configured. Current browser is
  LAN discovery; direct-address transport is distinct from public matchmaking.
- Live headset comfort/tracking/recentering checks remain pending. Synthetic stereo
  captures do not establish headset behavior. See topic docs for dated evidence.
- Latest test preference: user playtests after implementation; no autonomous game
  or phone interaction unless requested. Background games must always be muted.
- Rediscover phone connection with `adb devices -l`; old Wi-Fi ports expire.

## Resume Efficiently
- `AGENTS.md` maps source ownership; `tools/dev/README.md` has exact command recipes.
- `tools/dev/context.ps1` reports live Git state, including renderer changes.
- `docs/codex-session-notes.md` is historical, not a startup reading assignment.
  Use the helper's section lookup for specific decisions. Older same-day notes can
  describe work that later sections completed; reconcile with this file and code.
- Historical publication email/tree conversion is complete; do not repeat it.
