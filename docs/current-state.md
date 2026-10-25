# Current State

Updated: 2026-10-25. Living summary; replace stale facts rather than accumulating logs.

## Checkpoint
- Active work: `codex/vr-refresh-oct13` in `E:\SpaghettiKart-VR-refresh`.
- Android/UI, cockpit, LAN and renderer work was pushed to the user forks on
  2026-10-25. Verify live status; this sentence is not a cleanliness assertion.
- User said "All working" after the latest phone radial-control changes.
- Current request: lean project context/tooling. No gameplay behavior changed.

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

## Verified Versus Pending
- Prior muted phone/PC LAN checks passed movement, ownership and convergence.
  A complete two-player three-lap race, matching results and leave/rejoin remain
  pending. Items, eight racers, discovery and Internet transport are not complete.
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
