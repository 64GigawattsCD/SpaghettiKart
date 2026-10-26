# Hit Radio Portraits

## Behavior

Eligible stock hit-success taunts now pair the attacker's portrait and existing
voice on the victim's local viewport. The seven stock voice call sites cover
bananas, green/red/blue shells and fake item boxes. Their human-owner, self-hit,
invulnerability and distance conditions remain as written; this does not add new
CPU taunts or infer an attacker for other damage sources.

Each viewport has one passive panel: 150 ms eased slide up from its bottom edge,
1.5 seconds held, then 150 ms down. The 750 ms victim cooldown suppresses repeated
radio audio and portraits together. A later accepted event replaces, never stacks.
The frame is translucent with character accents and a small signal mark. Existing
64x64 face resources are preloaded for the race, independent of the menu heap.

Desktop and split-screen panels follow actual viewport bounds, including signed
widescreen coordinates. Phone panels are shorter and sit below the manual shifter
in either touch layout. Clipping is per rectangle/texture tile without changing
global scissor or projection. VR uses the existing HUD reference plane. Hidden-HUD
settings retain their existing behavior. The panel has no input targets.

Portrait age advances once per unpaused game frame, including replicated clients
whose physics tick exits early. Race load and quit transitions reset it. The voice
uses the victim's local audio slot at a distinct zero-distance source rather than
attenuating at the attacker's position. The mixer keys sources by pointer, so each
victim has separate storage; positions reset on every accepted call because sound
cancellation may alter them. One shared speaker still mixes split-screen audio.

## Local And Online Events

Legacy non-session races dispatch directly. Session races, including offline 1P
and Local, use host-authored events with attacker/victim participant IDs, character,
sequence, course time and tick. Local victim events queue directly; remote victim
events travel reliably only to that participant's connection. Compact local HUD
slots are resolved after transport, not confused with physical controller ports.

Events validate identities against the roster and match the attacker's character.
Clients cannot author them; old epochs and duplicate sequences are discarded.
Queues hold at most 32 events, reset between races, and are not replayed after
reconnect. The adapter expires events more than 2.5 course seconds away and drops
them during menus, transitions and finished races. Existing stock attacker audio
remains the fallback for CPU victims; a host-local attacker hitting a remote human
retains their own original feedback without doubling the local victim's radio.

Application protocol is now **5**; PC and phone must both have this version.
The cryptographic channel version is unchanged. No new raster assets or paid art
jobs were used. Art/service credits: 0; per-task token billing unavailable.

## Verification

- All eight network suites pass, including encrypted victim-only delivery, local
  parties with gapped ports, malformed events, duplicate/epoch rejection, bounded
  queues, host authority, race resets and reconnect without stale playback.
- Both radio HUD suites pass: timing/model checks and actual C draw-command/API
  checks, with 887,191 assertions across characters, viewports, phones and VR layout.
- The actual audio helpers are compiled with recording stubs in
  `tests/hit_radio_integration`, checking paired delivery, stock fallback, invalid
  inputs, cooldown rejection and post-race suppression.

No game process, screenshots, headset session or phone inputs were used. These
checks do not establish the actual appearance, audible mix or headset comfort;
those remain for user review.

Windows Debug and Android release compilation passed:

- `build/agent-logs/hit-radio-final-pc-20261026-102414-51b5344d.log`
- `build/agent-logs/hit-radio-final-phone-20261026-102418-a859c3b5.log`

The final APK was installed with `adb install -r` on SM-S908U at 2026-10-26 10:26
phone time, without an app launch. Settings SHA-256 stayed unchanged across this
installation: `fad93e9fa72fbbd9227f68abf6eba1e5da892a93454f5c1611533693a8b03afe`.
