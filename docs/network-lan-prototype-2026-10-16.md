# Phone / PC LAN Race

## Eight-Player Follow-Up (2026-10-25)

The sections below preserve the two-device prototype's historical test evidence.
Current work replaces its fixed two-player protocol with a versioned eight-seat
roster, one-to-four participants per connection, whole-party admission, per-player
input ownership and local-first viewport mappings. Native menus now have Local /
Online, party cards, host setup, LAN browser, direct connection, lobby and standings.
The unified Options/Data/Back/Quit footer remains.

Transport uses Monocypher 4.0.2 primitives for per-peer X25519, password derivation,
authenticated datagrams and replay windows. This is not a PAKE or a public-server
identity scheme. Passwords/tickets remain in RAM and are not advertised. New race
epochs keep prior-round packets out of subsequent races; eight-kart snapshots use
bounded four-kart chunks rather than increasing the existing packet budget.
Whole-party reconnect tickets retain participant identities and local viewports.
No firewall or organization settings were changed.

Local-only GP uses the shared roster/results path without sockets, including four
local drivers. Session GP progression covers four races and shared standings.
Stock items and human-only Battle are enabled. Battle supports two to eight
humans, one to three balloons, no CPUs. GP/VS support CPU fill, configurable laps,
engine class, mirror and item rules. Four-player local GP no longer takes the
stock four-racer render/texture path: each of four viewports has eight distinct
kart texture slots. Lap HUDs handle non-three-lap races without texture overruns.

Physical Start-to-join routes SDL devices to party seats, including reconnects.
Unconfigured secondary ports borrow SDL mappings in RAM; existing mappings and
saved ignore lists are retained. Touch and keyboard remain on the primary pad.
Any local player can open Options. Online only that player stops driving; Local
pauses world/object/particle simulation. Session Lakitus include remote rescue
simulation without rerunning the stock starter countdown.

The item sidecar commits complete bounded frames, remaps ownership by participant,
replicates eight inventories and balloons, and suppresses client item authority.
Final world frames are reliable and cached for finished reconnects. Lightning
scale is in kart snapshots; the balloon follows its replicated pickup on clients.
Moo Moo foliage, Kalimari trains, Turnpike vehicles and prop-swap branches use
session spawn rules so mixed viewport counts do not shift stock actor indices.
Custom actor-private state, course scripts, particles and the falling-rock private
respawn timer are outside the sidecar. Course visual objects are not a complete
general-purpose world replication system. The archive hash is not a full mod manifest.

Key derivation runs in at most two workers, including retired disconnect jobs;
handshake admission is bounded per address and globally. Encrypted traffic does
not add public server identity or PAKE properties. Public directory/relay hosting,
NAT traversal and client prediction/replay remain separate, unfinished work.
Current browser means LAN discovery; direct Internet needs a reachable UDP host.
Paid service/image credits used this pass: zero. Per-task token billing is not
exposed; account usage was not treated as task billing.

Primary cryptographic API references: [AEAD](https://monocypher.org/manual/aead),
[password derivation](https://monocypher.org/manual/argon2), and the pinned source
in `cmake/dependencies/monocypher.cmake`. Both devices need the matching
protocol-v4 application for the current driver-selection flow.

Subsequent deployment on 2026-10-25 at 17:58 phone time: the release APK installed
successfully on SM-S908U through the newly supplied wireless endpoint using
`adb install -r`. Existing game archives were present and left untouched. The
normal settings SHA-256 matched before/after installation. No app launch or
gameplay test followed. Paid service/image credits used: zero; per-task token
billing is not exposed.

### Verification

Driver selection revision: one global stock-portrait grid precedes per-player
transmission and Ready prompts, centered in the same full/half/quadrant regions
as each machine's local race views. Remote seats do not shrink those regions.
Local physical ports remain distinct from compact viewport slots. Character
availability checks pending local choices plus acknowledged remote ownership.
Protocol v4 adds per-party update request/acknowledgment counters, so an old
lobby broadcast does not overwrite a newer local choice. Authoritative Load
cancels pending profile edits. Both PC and phone need this matching version.
The revised driver flow passed 28,680 pure geometry/barrier checks, the existing
lobby and Python helper checks, and all six network suites. Added loopback checks
cover old-versus-latest party acknowledgments, host-normalized choices and Load
cancellation of pending edits. No gameplay or pixel checks were run.
Final compiler logs: `local-driver-ack-pc-20261025-184759-3da2d559.log` and
`local-driver-ack-phone-20261025-184808-ffb3a086.log` in `build/agent-logs`.
APK installation succeeded on SM-S908U at 18:50 phone time; normal settings
SHA-256 was unchanged. App was not launched. Paid service/image credits: zero;
per-task token billing is not exposed.

Main-menu revision: 1P, Local and Online now occupy stock-sized player-count tiles.
Player join badges occupy padded corners above the existing footer. Main/setup
pages no longer route to character/transmission/Ready editing. 1P locks secondary
joins, defaults to CPU fill and enters the same pre-race local lobby as Local.
Local preparation now stops unready in the lobby instead of immediately loading
a race. Characters and transmission remain editable there before Ready/Start.
This revision passed 79,357 lobby/model checks, both Python helper tests and the
six network suites. PC Debug and Android release compilation passed; local logs:
`corner-menu-final-pc-20261025-182449-26303053.log` and
`corner-menu-final-phone-20261025-182529-513a48f5.log` under `build/agent-logs`.
The revised APK installed on SM-S908U at 18:27 phone time. Settings SHA-256 was
unchanged; no app launch or gameplay test followed. Paid service/image credits:
zero. Per-task token billing is not exposed.

- Six native network suites cover malformed/transactional decoding, encrypted
  4+4 admission and racing, eight-system input/results, four network GP rounds,
  readiness/capacity/ownership, expired input, loading/racing/finished reconnect,
  final-world replay/clearing, LAN codecs, item frames, device assignments and
  64 independent kart texture slots. Transport tests run only on localhost.
- Native lobby (79,053 checks), Options and menu-pointer suites passed, as did
  two pure Python lobby-runner helper tests. Size/packet-budget regressions added.
- No game process, phone interaction or new gameplay screenshot was launched.
  Full races, mixed-device latency, Battle, item effects, four-controller joining
  and touch layouts still need user playtesting. Do not equate codecs with playtests.
- The opt-in muted runner is `tests/kart_assets/test_lobby_menus.py --run`; add
  `--race` for four-pad local GP. The bridge exposes `menu.lobby` and per-port
  queued inputs. Legacy menu runners are not updated for the new contexts.
- Windows Debug and Android arm64 release compilation passed. Final local logs:
  `build/agent-logs/eight-racer-release-pc-20261025-162347-70485b91.log` and
  `build/agent-logs/eight-racer-release-phone-20261025-162405-d8943381.log`.
  All six network suites passed in
  `build/agent-logs/eight-racer-release-tests-20261025-162428-12248d58.log`.
  Outputs: `build/x64/Debug/Spaghettify.exe` and
  `android/app/build/outputs/apk/release/app-release.apk`. Neither was launched.
  Main builds reuse libultraship's identical pinned Monocypher commit; standalone
  tests use the commit-addressed, SHA-256 checked archive. Sources match apart
  from checkout line endings. No renderer submodule changes this pass.

## Research and scope

Fetched `origin/NetworkedMultiplayer` on 2026-10-16: still e8351e29f, with discovery
and hello messages only. Its ENet revision is retained, its raw-struct discovery
codec and unrelated changes are not. Searches did not establish a finished
SpaghettiKart network-racing fork to import.

Primary references reviewed:

- [Epic networking overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine):
  client-owned intent, host-owned world and results, local presentation.
- [Epic networked movement](https://dev.epicgames.com/documentation/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine):
  authority, autonomous and simulated roles. Prediction needs saved moves plus
  correction/replay, not only transform smoothing.
- [SuperTuxKart GameProtocol](https://github.com/supertuxkart/stk-code/blob/master/src/network/protocols/game_protocol.hpp):
  tick-stamped controller actions, separate state and item messages, rewind-aware
  simulation. Its architecture is a reference, no GPL source copied.
- [SuperTuxKart networking](https://github.com/supertuxkart/stk-code/blob/master/NETWORKING.md):
  automated AI clients and configurable state cadence are useful test patterns.
- [ENet channel design](https://github.com/lsalzman/enet/blob/master/docs/design.dox):
  independently sequenced channels, reliable delivery for session changes and
  superseding unreliable updates for continuous state. Do not duplicate ENet's
  reliable retransmission machinery above the transport.

## First playable target

Experimental, direct IPv4 LAN, two devices, one local racer/viewport each. Luigi
Raceway, 50cc, three laps, automatic transmission, no item boxes. PC hosts physics,
collisions, checkpoint progress and finish decision. Phone sends driving input and
renders authoritative snapshots. Remote presentation interpolates snapshots. There
is deliberately no claim of client prediction/replay, eight-player support,
Internet matchmaking, NAT traversal, GP progression, or replicated items yet.

Network participants 1/2 and connections 1/2 are distinct from local slots.
Participant 2 occupies host racer slot1 and client racer slot0. Each machine keeps
its own slot0 camera/HUD and has exactly one viewport. The two-racer permutation is
explicit; expanding beyond two requires a roster/mapping implementation rather
than increasing ENet's peer count.

## Protocol v2

All integers big-endian, floats explicit IEEE754 binary32 bits. Header: magic
`SKN1` (4 bytes), version (u16), message kind (u8), reserved zero (u8), session nonce
(u64). No native structs, pointers, strings, padding or compiler-specific enum
representations travel on wire. Exact message lengths and numeric ranges checked.
Packets capped at1200 bytes, queued receive data capped at64KiB.

- Channel0 reliable: Hello, Welcome, Load, Loaded, Begin, Finish.
- Channel1 unreliable sequenced: Input with sequence, owner, sticks, buttons and
  analog throttle/brake. Latest held intent supersedes older intent. Duplicate and
  out-of-owner inputs are ignored. After250ms without a fresh input, neutral intent.
- Channel2 unreliable sequenced: full two-kart snapshot, host frame, last consumed
  input, host race clock, transforms, pre-physics camera anchors, tire contact
  positions, orientation, movement display fields, RPM,
  gear, laps, ranks and lap timestamps. Older snapshots ignored.
- Join checks rules revision and FNV-1a64 of the local `mk64.o2r` base archive.
  This is a compatibility check, not authentication. This prototype is for trusted
  LAN testing, not public hosting. Stock Luigi Raceway content is assumed; a
  manifest of gameplay resources/mods is still needed for general sessions.
- Both devices must reach staging before Begin. Loading/connection timeouts are
  bounded. Disconnect stops racing; no silent promotion or migration of authority.
- First racer reaching lap3 ends this initial match. Reliable final snapshot is
  distinct from the unreliable movement stream.

## Test entry points

`tests/network` compiles independently: wire endian/roundtrip/truncation, float
validation, sequence rollover, mapping,20k malformed packets, ENet bind failure,
loopback admission, character collision, host-only start, load barrier, input
expiry, duplicate rejection, snapshots, finish, disconnect, restart and mismatch.

`tests/kart_assets/test_network.py` runs two isolated muted game processes with
native queued input and renderer screenshots. Checks viewport ownership, independent
movement, authoritative state convergence, input acknowledgement and disconnect.
`tests/kart_assets/test_network_phone.py` runs the PC host with a USB-connected
phone as the Wi-Fi client. ADB carries test commands and screenshots, not gameplay
packets. Both games remain muted; test settings are isolated from normal settings.

## Manual entry

Settings > General > LAN Race. Host LAN on PC, enter its LAN IPv4 on phone and Join
LAN, then Start Race on PC. Default UDP54777. Both devices need matching assets
and this version. No persistent firewall changes are made by the game.

Verified local endpoints: PC `192.168.1.23`, phone `192.168.1.37`, UDP54777.
Use the refreshed PC executable at `build/x64/Debug/Spaghettify.exe` and the app
now installed on the phone. Start from menus on both devices. After joining,
Start Race on PC launches the shared Luigi Raceway session. A separate Windows
repair added a private-subnet permission for this exact executable/port.

Phone HUD comparison: Settings > General > Phone HUD Layout, A: Controls inward
or B: HUD inward. This is independent of network simulation.
User selected A after testing: inward controls suit their thumb reach; retain
the HUD at the outer edge. The phone already uses this default.

## Verification (2026-10-16)

- Native codec/ENet suite passed, including reliable finish, input expiry,
  disconnect, mismatch, restart, and20k malformed packet inputs.
- Two native PC game processes passed ownership, independent movement,
  convergence, acknowledgements, disconnect, two-racer ranking, and a moving
  client cockpit pixel check: `build/network-game-test/20261016-115400`.
- Real phone/PC Wi-Fi LAN passed all18 checks:
  `build/network-phone-test/20261016-115703/report.json`. Phone controlled the
  host's remote kart without moving the host's own kart; PC movement reached the
  phone. Both retained one viewport. Normal phone preferences remained unchanged.
- Native phone renderer captures and app-owned touch overlays were inspected for
  A/B layouts. `phone/layout-0-composite.png` is controls inward;
  `phone/layout-1-composite.png` is HUD inward, under the LAN evidence directory.
  Both layouts passed native touch self-tests. A remains the default.
- Offline phone regression passed all13 gameplay/config checks, eight paint hues,
  and camera alignment0.9999426 in `build/android-phone/network-offline-regression`.
- Phone RPM transition regression passed all32 acceleration/braking captures with
  no red-screen spill and unchanged settings in `build/android-phone/network-rpm-regression`.
- All eight offline renderer HUD cases passed, OpenGL/DirectX11, wide/ultrawide,
  and two/three/four-player splits: `build/hud-game-test/20261016-115449`.
  Native HUD, binary-angle, RPM-angle, and audio queue/mixer suites passed.
- Installed non-VR APK SHA256:
  `11edb9aa1ebeb793bfd3a6e35cddceafea4652a2e7abfc87b9392652a7ed2ecb`.

The client camera originally used a post-physics position while the cockpit used
pre-physics tire contacts. This hid the model at speed. Replicating both contact
points and the corresponding camera anchor fixed it without changing offline
camera behavior. LAN starts at its shared load barrier rather than running two
independent intro countdowns. The ranking display excludes the six inactive seats.

## Next Race Test

The automated hardware run covers driving/braking, not a completed three-lap
race. Finish delivery passed at the transport level only. Next: drive a complete
phone/PC race, verify both lap splits and the same winner, then leave/rejoin.
Physical GameSir and VR hardware were not exercised in this pass. Measure latency
under loss/jitter before prediction/reconciliation work. Items, eight racers,
split-screen network ownership, lobby discovery and Internet transport remain
separate follow-ups, not implemented features of this prototype.
