# Fork and Feature Survey - 2026-10-13

## Scope

Research only: enumerated 143 forks through the GitHub API and inspected selected
repositories, diffs, source files, documentation, and releases. This was not a
complete code audit of every fork. No donor code or assets imported this turn;
no game launched. Background game tests must remain muted.

## Closest Matches

### VRmicelliKart: cockpit and telemetry

- Repository: https://github.com/gnatman/VRmicelliKart
- Inspected revision: `7c222d7d70`.
- `src/port/FirstPersonCockpit.cpp` contains procedural steering wheel, spokes,
  column, dashboard, and rolling/steering front tires, plus drift sparks and
  Star/Boo appearance. Geometry offsets and scale have live settings.
- This is actual 3D cockpit geometry, not a full 3D driver/kart roster.
- `src/port/TelemetryManager.cpp` sends SimHub-style UDP telemetry (default port
  20777), including motion and boost/landing/hit events. Relevant to haptics.
- Our earlier refresh imported selected rendering/interpolation fixes, NOT this
  cockpit. Donor camera/rendering conventions differ from our OpenGL VR path.
- No GitHub release package found during this survey.

### Ricky fork: local 5-8 players

- Repository: https://github.com/Ricky69Gameplays69Caballo/Mario-kart-64-5-players-Ricky-Gameplays
- Windows release v0.1.2, published 2026-10-11:
  https://github.com/Ricky69Gameplays69Caballo/Mario-kart-64-5-players-Ricky-Gameplays/releases/tag/v0.1.2
- README/release describe 5-8 humans in GP/VS/Battle, 3/4-player GP, optional
  bots filling eight slots, repeated character selection, eight controller
  profiles, expanded HUD/minimap/results, spectators and multiplayer podiums.
- Also has 1x-5x draw distance and layout preferences for uneven player counts.
- Repeated characters are not additional custom character slots.
- Compared with current upstream: 9 commits ahead, 1 behind, 75 changed files.
  Substantial input/HUD overlap with our local work; not a small transplant.
- Author reports Windows testing. Not locally played during this survey.

### SpaghettiPad: character-pack selection

- Repository: https://github.com/chrissotraidis/spaghettipad
- Documentation: https://github.com/chrissotraidis/spaghettipad/blob/main/docs/MODS.md
- Wrapper revision inspected: `94393cb754`.
- Its native submodule points to https://github.com/chrissotraidis/SpaghettiKart
  at `d545ba9ca6a22ca73a328893f6b9e274f99853a2`.
- Persistent pack toggles, next-launch activation, character-over-HD priority,
  mutually exclusive packs occupying the same racer slots, conflict messages,
  and catalog/selection tests are useful donors for a desktop mod selector.
- These character packs replace sprites; they are not runtime 3D drivers.
- Docs report current-path compatibility for NPC Racers, while original Child
  Link/Kris/Ralsei packages need old resource paths migrated. Private migrated
  copies mentioned in the docs are not published there.

Character examples:

- Ocarina of Time NPC Racers (eight racer replacements): https://gamebanana.com/mods/701263
- Child Link over Luigi: https://gamebanana.com/mods/655866
- Kris over Yoshi: https://thunderstore.io/c/spaghetti-kart/p/sitton76/Kris_over_Yoshi/
- Ralsei over Toad: https://thunderstore.io/c/spaghetti-kart/p/sitton76/Ralsei_with_hat_over_toad/
- Kris/Ralsei credit sitton76 for models/renders and Andrat for the wheel mesh.
  Those credits do not establish that source mesh files ship with the packs.

### Duglas560: extra cup menu

- Repository: https://github.com/Duglas560/SpaghettiKart
- Inspected tip: `e68c54ea35`; 7 commits ahead, 1 behind upstream.
- Custom cup commit: https://github.com/Duglas560/SpaghettiKart/commit/3a63cf30f7
- Adds cup-page state and an extra cup containing Harbour plus existing tracks;
  later work adds course previews. Treat as work in progress, not a completed
  arbitrary-track organizer or tested release.

## Full 3D Racer Lead Outside SpaghettiKart

### Litronom's Amped Up

- Follow-up: `docs/amped-up-technical-research-2026-10-13.md` records the author
  interview, direct technical posts, inspected OverKart animation source, archive
  contents, licensing metadata and porting questions. Complete racer source and
  editable meshes remain unlocated.
- Primary listing: https://gamebanana.com/mods/613207
- Its ProfilePage API text explicitly lists animated 3D racers, racer costumes,
  16 courses and five additional modes. Credits name Litronom, DeadHamster,
  HootHoot, AndratVA and Stinger O Wanjiro among contributors.
- This is a ROM hack for emulator/console use, not a SpaghettiKart fork.
- Independent primary corroboration: MK64 Reloaded v2026.04.03 changelog adds
  N64 3D-driver textures for Amped Up:
  https://github.com/GhostlyDark/MK64-Reloaded/releases/tag/v2026.04.03
- No public package containing the relevant source code and editable meshes was
  located. Litronom's public GitHub repositories did not include Amped Up.
  Asset provenance, reuse terms and renderer integration need investigation
  before treating this as a practical donor. No claim of drop-in compatibility.
- Tarmac64 is an adjacent tool lead, not a verified complete donor:
  https://github.com/DeadHamster35/Tarmac64

## Lower-Priority and Unverified Leads

- https://github.com/LukasThum/SpaghettiKart has Lua CPU-control experiments:
  11 commits ahead, 137 behind upstream, with history noting steering trouble.
- lewilou22's online-multiplayer description and BassFace2761's ruleset
  description were not backed by unique commits on their visible default
  branches: both 0 ahead and 56 behind upstream during inspection.
- MarioKart64Recomp lists model replacements as PLANNED in its own README:
  https://gitlab.com/sonicdcer/MarioKart64Recomp
  Do not present this as an existing full-3D racer solution.
- No verified ready-to-merge full 3D driver/kart roster for SpaghettiKart found.

## Priority for Our Checkout

1. VRmicelliKart cockpit and telemetry for first-person/arcade hardware.
2. Amped Up source/mesh availability for a complete 3D racer approach.
3. SpaghettiPad pack selection for custom-character and HD-pack management.
4. Ricky multiplayer changes when 5-8 local players become a project objective.

## Usage Record

- Account-wide weekly usage observed at survey start: 85%; near finish: 88%.
- Credit balance: 0. No credit purchase or reset. These account-wide readings
  are not a per-task token or cost measurement.
