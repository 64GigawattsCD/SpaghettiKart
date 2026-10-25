# Network and Online Audit

Historical baseline: the findings below preceded the new LAN integration on the
same date. See [the LAN prototype and test receipt](network-lan-prototype-2026-10-16.md)
for the current phone/PC implementation. The older branch was not merged wholesale.

Audit date: 2026-10-16. Active checkout: `codex/vr-refresh-oct13`, HEAD
`0c8c9a675`, including the current Android/cockpit work.

The current PC/Android game is local-only. Our separate `NetworkedMultiplayer`
branch has ENet LAN discovery and a connection handshake, not multiplayer racing.
Its remote head was checked during this audit and still matches
`e8351e29f3b7d7e6b614cdae1a0068d41c6b5118` (2026-03-22).
Nothing from that branch was merged during this audit.
The old transport findings below are from source inspection, not a two-device
runtime test. Priorities concern future integration; local play does not currently
run this networking code.

## Findings

### P1 Discovery accepts an unterminated session name

In the [older network source](https://github.com/64GigawattsCD/SpaghettiKart/blob/e8351e29f3b7d7e6b614cdae1a0068d41c6b5118/src/port/net/NetworkManager.cpp#L366),
`server.SessionName = decoded.SessionName` treats the received 64-byte array as
a terminated C string. The decoder at line 73 checks only packet size, magic,
and version. A packet with 64 nonzero name bytes reads past the field, potentially
past the object. Use an explicit byte length or bounded terminator search, and
test all-zero, full-length, truncated, and malformed packets before reuse.
This code is absent from the installed phone app.

### P1 Every host on the same port is marked as self

`NetworkManager.cpp:368` uses only `mHostingLan && decoded.GamePort == mGamePort`
for `IsSelf`. Two machines hosting on the default 54777 therefore classify each
other as self. `LanMultiplayerWindow.cpp` then shows Hosting instead of Join.
Use a per-session identifier, with local endpoint identity, and test two hosts
on the same port plus a third browsing client.

### P2 The discovery format and host lifecycle need another pass

`NetworkManager.cpp:61-80` copies a native struct, including its padding and native
integer byte order, directly onto the wire. Define explicit byte offsets, widths,
byte order, packet type checks, name length, and legal port range.

`StartLanHost` at line 137 reports success and sets the hosting flag before binding
the server socket. Binding failure leaves announcements running for a nonexistent
server. Changing discovery ports also leaves an already-open discovery socket on
its original port (`EnsureDiscoverySocket`, line 274). Tie UI/announcements to a
confirmed host state and recreate sockets on endpoint changes. Add bind-failure,
stop/restart, port-change, disconnect/rejoin, and shutdown tests.

### P1 Remote racers have no ownership or simulation path yet

In the active checkout, `src/main.c:107` has four physical controller pads and
`read_controllers` at line 477 updates four fixed local slots. The menu assigns
player count from screen mode (`src/menus.c:1349`), and multiplayer cameras bind
viewport index directly to racer index (`src/spawn_players.c:1425`). Texture
loading also uses `{8, 8, 4, 4}` by local count at line 1486. Merely accepting eight
ENet peers would not result in eight playable racers.

`nControlFlags` and `nCharacter` in `include/common_structs.h:400` have no uses in
the searched source. Their network comments are not a working remote-input path.
Stable participant identity, connection ownership, local input slot, racer slot,
and local viewport slot must be distinct before mixed local/remote races work.

### P2 Prediction requires more than transform interpolation

`src/port/Engine.cpp` currently interpolates renderer matrices between game
updates. It has no authoritative snapshot history, input acknowledgements, or
reconciliation. Gameplay and visual effects both consume `random_int` backed by
`gRandomSeed16` (`src/racing/math_util.c:1013`, `src/code_80057C60.c`,
`src/race_mods.c`). Replaying player updates must also account for item inventory,
collision/contact state, timers, RNG, and one-shot sound/particle effects.
A transform-only correction is not enough for the wishlist's predicted movement.

## Existing Plan

The detailed plan is in the original workspace, not this checkout:
`C:\Users\Craig\OneDrive\Documents\mk64 Arcade\docs\netplay-wishlist.md`.
It explicitly describes future work, not delivered features.

Its direction remains coherent: host-authoritative racing, eight total racers,
one to four local players per machine, local-only viewport counts, predicted own
kart, interpolated remote karts, and host-owned items/results/GP progression.
Its milestones are offline player/session separation, loopback, LAN lobby,
race replication, then online GP. The active branch has not completed the first
milestone. The old ENet branch is a partial transport experiment outside that
milestone sequence, not a completed LAN lobby.

Keep ENet for an initial LAN experiment rather than also implementing the
wishlist's own packet acknowledgement/resend layer. Define application messages
and ownership above the transport. Do not inherit unrelated `properties.h`
version/branding reversions from the old commit.

## Reusable Pieces

- ENet dependency pin and host/client event-loop outline from `e8351e29f`.
- Discovery UI/status plumbing after the parser and host-identity fixes.
- Existing queued-input and framebuffer test bridge for two-process scenarios.
  Its ADB/file channel is test tooling, not a multiplayer protocol.
- Existing eight-racer arrays and local split-screen rendering, after count and
  identity separation. Existing renderer interpolation remains useful for display.

The 2026-10-13 fork survey did not substantiate a working online implementation
in the named online-multiplayer fork. VR and expanded local multiplayer are not
evidence of networked racing. No new external fork search was part of this audit.

## Next Delivery Sequence

1. Extract a session roster with explicit ownership and derived counts. Test
   one local plus seven remote, two plus six, and four plus four without sockets.
2. Restore only the repaired ENet transport and bounded, versioned message codec.
   Include protocol/content/rules compatibility checks and participant-slot
   reservations; a connection may own several racers.
3. Prove two PC processes can join, ready, start, disconnect, and rejoin a lobby.
   Reject stale or duplicate commands and out-of-owner racer actions.
4. Prove one track with two racers, host movement snapshots and input sequence
   acknowledgements; then add prediction/replay and one-shot event IDs.
5. Add host-owned items, checkpoints/results, eight-racer mixed local/remote
   sessions, then GP progression. Test delay, jitter, loss, reorder, and reconnect.
6. Test PC-to-Android LAN. The current Android manifest only requests vibration,
   not `INTERNET`; add network access with the transport, plus background/resume
   handling and direct-address joining when broadcast discovery is unavailable.
7. Treat internet hosting as a separate delivery: rendezvous, NAT traversal or
   relay, identity/session admission, and a defined host-disconnect outcome.

The first useful target is a two-device LAN race, not a generic replication
system or a public matchmaking service. This audit changes no networking code.
