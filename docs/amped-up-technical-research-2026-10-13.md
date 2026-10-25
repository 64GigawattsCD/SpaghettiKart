# Amped Up Technical Research - 2026-10-13

## Result

Amped Up combines authored content, custom game code, Blender tooling and the
OverKart/Tarmac ecosystem. Public repositories expose useful animation and
geometry machinery, but no complete Amped Up racer source/assets package was
located. Do not equate related OverKart code with its exact racer renderer.

Research only. No game code changed, patch applied, executable run, or author
contacted. All future background game tests must remain muted.

## Evidence Levels

- Direct: author interview, author posts retrieved through Bluesky's public API,
  author release listing/archive, and inspected repository source.
- Related: public OverKart/Tarmac code, without a verified match to Amped Up's
  private revisions or racer-specific code.
- Unconfirmed: an indexed social mirror describing source availability and
  recompilation difficulties; no original post URL recovered for that claim.

## Author's Account

The November 7, 2022 interview establishes that courses began in 2020, were
authored in Blender 2.79b, and were repeatedly compiled/tested. Litronom added
custom code with DeadHamster's help and wrote a Python Blender exporter for
animated course objects. That interview predates the later 3D-racer release;
it does not establish that racers use the same exporter.

Source: https://gamingreinvented.com/interview/lets-interview-mario-kart-64-amped-up-developer-litronom/

## Direct Author Posts

Retrieved via `public.api.bsky.app/xrpc/app.bsky.feed.getAuthorFeed` and
`getPostThread`. Author: `litronom.bsky.social`, DID
`did:plc:foxt3nvdtr2fzzzrnhlwvxvh`. Web text extraction for the corresponding
pages failed, but the public API returned the records and reply context.

| Date | Finding | Primary post |
| --- | --- | --- |
| 2024-11-25 | Custom winning and losing animations for the 3D racers showcased. | https://bsky.app/profile/litronom.bsky.social/post/3lbribmdi722s |
| 2025-04-04 | Author says console is detected and adjustments respond to processing load. Exact adjustment algorithm unspecified. | https://bsky.app/profile/litronom.bsky.social/post/3llyypvuggk2i |
| 2025-07-18 | Asked about a decompilation/recompilation port, author confirms feasibility but emphasizes time required. | https://bsky.app/profile/litronom.bsky.social/post/3lubh6g6kp22m |
| 2025-07-26 | Version 3.00 release announcement. | https://bsky.app/profile/litronom.bsky.social/post/3luvddoj4qs2p |
| 2025-07-26 | Author confirms airborne steering changes help traverse new courses and longer jumps. | https://bsky.app/profile/litronom.bsky.social/post/3luviuegtsc2e |
| 2025-10-22 | Player-select portraits use actual 3D models and follow the 3D-racer setting. | https://bsky.app/profile/litronom.bsky.social/post/3m3s57ww3js27 |
| 2025-10-22 | Working emulator setup is Project64 plus GLideN64. This is the author's report, not our test. | https://bsky.app/profile/litronom.bsky.social/post/3m3sdxpht7c25 |
| 2025-11-17 | Ranking portraits reflect costumes by capturing framebuffer image data in player select. | https://bsky.app/profile/litronom.bsky.social/post/3m5tycxhug22l |
| 2025-12-20 | Version 3.21 hotfix announced. | https://bsky.app/profile/litronom.bsky.social/post/3mah5wjv7w223 |

Official v3 trailer URL, obtained from the release post's link metadata:
https://www.youtube.com/watch?v=vvg20Gd58WA
Video playback/transcript not inspected; do not attribute unseen details to it.

An earlier X post dated 2024-07-13 was verified through X's public syndication
endpoint, but its text is only a Mario caption and tags, with no technical detail:
https://x.com/Litronom/status/1812243295378264412

## Public Tooling and Source

### Litronom's Blender Population Exporter

https://github.com/Litronom/blender_pop_export

Its README describes curve-based route export and named-object placement for
Tarmac. Version 3 groups objects under Course Population and supports paths,
object types and integer flags such as item selection. Variants target Blender
2.79, 2.8 and 2.93-era APIs. Its linked course tutorial:
https://www.youtube.com/watch?v=2u1L_epK7mg

Important: this is a placement/path exporter, NOT the missing racer-animation
exporter, despite both being Blender scripts by the same author.

### Tarmac64 Asset Compiler

https://github.com/DeadHamster35/Tarmac64

Inspected revision: `33ae67ea0c03702e2f0502ea81cf0818214ea9b2` (2025-03-13).

Key files in that revision:

- `Tarmac64 Library/TM64_Geometry.cs`: `LoadSkeleton` (line 6541) finds BodyBone,
  reads the first FBX animation through Assimp, and processes its node channels.
  `LoadAnimation`, `TransformBone`, `WriteAnimationData`, and
  `WriteAnimationModels` process transforms and associate meshes with bones.
- `Tarmac64 Library/TM64_Course.cs`: `CompileObjectAnimation` (line 864) writes
  animation/model tables, alignment and segment-based offsets. Object animation
  slots include walk, target and death. These are object semantics, not evidence
  of racer steering/drift/win/lose state mapping.
- Geometry export changes coordinates from `(x,y,z)` to `(x,z,-y)` in inspected
  vertex/translation paths. Rotation conversion needs separate verification.
- `Tarmac64 Retail/Windows/AnimationCompiler.cs` imports FBX and emits geometry,
  compressed/raw data and symbol/header files, but its animation-writing call
  is commented out. Do not describe that standalone dialog as fully functional.

Source permalink:
https://github.com/DeadHamster35/Tarmac64/blob/33ae67ea0c03702e2f0502ea81cf0818214ea9b2/Tarmac64%20Library/TM64_Geometry.cs

### OverKart Runtime Library

https://github.com/DeadHamster35/Library

Inspected revision: `d3e298aabe9f311599076387edae6135113c2889` (2025-03-21).

- `CustomObjects/OKCustomObjects.c`: `SkeletalMatrix` reads per-frame angle,
  translation and scale data; `DrawOKAnimationLoop` visits skeleton nodes and
  draws mesh display lists. This path animates custom objects, not proven racers.
- `CompileLibrary.BAT` and `compile_o_files.sh` use MIPS GCC targeting VR4300.
- `LIBRARYBUILD1.asm` places hooks at fixed ROM addresses. This explains why
  moving related code into a native PC game involves more than copying files.

Source permalink:
https://github.com/DeadHamster35/Library/blob/d3e298aabe9f311599076387edae6135113c2889/CustomObjects/OKCustomObjects.c

### Newer OverKart Decompilation Integration

https://github.com/DeadHamster35/OverKartDecomp

Inspected revision: `66528ec905821601e419a943b810c0206fb899a9` (2026-09-18).
Its README identifies OverKart V8 and Tarmac course integration into MK64 decomp.

`src/OverKartLibrary/CustomObjects/OKAnime.c` contains keyframe interpolation,
binary-angle wrapping, matrix composition, recursive skeleton traversal, texture
state reuse, mesh display lists and matrix-stack cleanup. This is a more useful
source-level reference for a port than binary ROM hooks, but remains an object
animation system, not an identified Amped Up racer implementation. It advances
animation from a draw path; a VR adaptation would need one simulation update
per tick, not a separate advance for each eye/view.

Source permalink:
https://github.com/DeadHamster35/OverKartDecomp/blob/66528ec905821601e419a943b810c0206fb899a9/src/OverKartLibrary/CustomObjects/OKAnime.c

The VR observation is our engineering assessment from the inspected code, not
an author statement or a tested defect in Amped Up.

## Release, Credits and Availability

Primary listing: https://gamebanana.com/mods/613207
Structured metadata: https://gamebanana.com/apiv11/Mod/613207/ProfilePage

- Release inspected: `mario_kart_64_-_amped_up_-_v321.rar`, file ID 1584378,
  3,775,079 bytes. Archive contains a BPS patch, Readme and two emulator/flashcart
  setup texts. No .blend, FBX, animation exporter or source files.
- Credits: Litronom leads; DeadHamster supplies code/tools; HootHoot coding help;
  AndratVA and Stinger O Wanjiro art assets. The listing does not identify which
  artist authored each racer, kart part, rig or texture.
- The listing's `_sLicense` field labels the work CC BY-NC-ND 4.0. Do not assume
  a permissive code/asset license or a separate agreement for a port.
- Public GitHub profile/search checks did not locate Amped Up's complete source.
  The version archive on MEGA was linked by the author but not exhaustively
  inspected. Private OverKart Discord messages were not accessed.

The earlier survey checked `_aLicense`, which was null; `_sLicense` is populated.
This corrects that missing metadata, without drawing legal conclusions.

## Unconfirmed Source-Access Lead

Search-indexed TwStalker text attributes a post to Litronom describing a recomp
attempt, custom symbols/assembly hooks, and willingness to supply source.
Original post URL and first-party text were not recovered. Treat this only as a
contact lead, not verified source availability or permission to port.

The independently verified July 18 Bluesky reply above establishes feasibility
and time cost, but does NOT establish those detailed blockers or source access.

Author-linked community: https://discord.com/invite/NNkQSz9
Author profile: https://bsky.app/profile/litronom.bsky.social

## Implications for Our First-Person Work

Assessment, not an import plan already in motion:

1. Seek the racer renderer/state code, animation exporter, original .blend/FBX
   files, texture sources and export settings from the author/contributors.
2. Ask whether the kart frame, steering column and wheels are separate meshes,
   what axes/units they use, and how steering, drift and item states map to clips.
3. Establish per-component reuse terms before publishing adapted code/assets.
4. Examine OverKartDecomp's animation interfaces and our existing renderer. Keep
   asset transforms separate from driving-forward and headset/camera transforms.
5. Any future VR animation must tick once and draw identically for both eyes;
   verify depth, culling, frame interpolation and split-screen state independently.

Nothing found explains our historical 45/90-degree camera offset. The exporter
axis mapping is relevant to future mesh conversion, NOT evidence of that bug.

## Local Research Cache and Usage

Cache: `E:\SpaghettiKart-research\amped-up-2026-10-13`.
Contains shallow Tarmac64/Library source checkouts and the untouched release RAR.
No downloaded project scripts or executables were run.

RAR SHA-256: `AC457E7606DD618E7F53A25D1BEF5230E85777A093AABC7597FF074D7D6E2D8F`.

Account-wide weekly usage: 1% at start and near finish (rounded readings).
Credit balance: 0; no purchase or reset. Not a per-task token/cost measurement.
