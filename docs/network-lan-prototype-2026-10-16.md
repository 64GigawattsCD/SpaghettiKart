# Phone / PC LAN Race

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
