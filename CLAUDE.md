# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Night Clubs Descent** is an Unreal Engine 5.7 C++ first-person run-and-gun game (game jam) whose enemies are **trained RL policies from a separate private project, FPS-Agent**, run through UE's Learning Agents plugin (inference only). The UE project lives in `GameJam/` (`GameJam.uproject`); the repo root holds the design docs.

Read before touching enemy/agent code, player body/movement/sounds, collision, spawning or level geometry:
- [PLAN.md](PLAN.md): game design, proposed architecture, milestones, open decisions. Confirmed vs Proposed vs Open is deliberate; don't promote open ideas to requirements or invent lore.
- [FPS-Agent-Constraints.md](FPS-Agent-Constraints.md) (v1.1) + [FPS-Agent-Constraints-v1.2-addendum.md](FPS-Agent-Constraints-v1.2-addendum.md) = **contract v1.2**, the authority on observation/action schema, body/rifle/movement numbers, perception, hearing, timing, spawning and acceptance tests. Code comments cite it as `§N` and PLAN as `PLAN §N`.
- [MAPGEN.md](MAPGEN.md): the procedural map generator (families, levels, rules, Unreal integration, measured stats).
- [FRONTEND.md](FRONTEND.md): the main menu, the starting sequence, the pause menu and the death/results screen.
- [MUSIC.md](MUSIC.md): the adaptive club music (stems, tiers, how to render, import or replace stems).
- [REPO-WARNINGS.md](REPO-WARNINGS.md) and [GameJam/Source/AgentRuntime/VENDORED.md](GameJam/Source/AgentRuntime/VENDORED.md).

## Build / run

- UE 5.7 (5.7.4 baseline), C++ project; toolchain per `GameJam/.vsconfig` (Visual Studio, Windows). Modules: `GameJam` and `AgentRuntime` (targets `GameJam` / `GameJamEditor` in `GameJam/Source/`).
- On the Linux dev box, UE 5.7.4 (installed build, with Learning Agents and the Python plugin) is at `~/Downloads/unreal_engine`. Build: `~/Downloads/unreal_engine/Engine/Build/BatchFiles/Linux/Build.sh GameJamEditor Linux Development -Project=$PWD/GameJam/GameJam.uproject -WaitMutex` (from the repo root; about 1 min). On Windows: `Build.bat ... Win64`.
- Run a level headless: `~/Downloads/unreal_engine/Engine/Binaries/Linux/UnrealEditor <abs>/GameJam.uproject /Game/GJ/Lvl_MapGen -game -nullrhi -nosound -unattended -nosplash -stdout -ExecCmds="..."`. Editor Python scripts run with `-run=pythonscript -script=<file> -EnablePlugins=PythonScriptPlugin -unattended -nullrhi` (how `Lvl_MapGen` was made).
- The only automated tests are for the map generator. They run without Unreal: `Tools/MapGen/build.sh /tmp/mapgen_test && /tmp/mapgen_test stats 30` (exit 1 on any invalid or non-deterministic layout); `/tmp/mapgen_test show graph 2.5 42` prints one map. Everything else is verified in-editor (PIE) against the contract's §16 acceptance tests. Packaged Development **and Shipping** inference must be checked (PLAN §12).
- Default game map: `/Game/GJ/Lvl_Club` (main menu, intro, the club plus procedural tunnels; FRONTEND.md). Editor startup map: `/Game/ClubMap/Maps/Lvl_ClubMap`, the hand-built club. It was migrated from the `ClubMap/` map-design project with identical `/Game/ClubMap/...` paths. `ClubMap/` stays the map designer's authoring project: after map changes there, copy `ClubMap/Content/ClubMap` over `GameJam/Content/ClubMap` again, or use Migrate (docs/MAP_DESIGN.md §8.3). Other levels: the testing ground `/Game/FirstPerson/Lvl_FirstPerson` (World Partition) the procedural map's test level `/Game/GJ/Lvl_MapGen`, and `/Game/GJ/Lvl_Club`: the club (`/Game/ClubMap/`) as an always-loaded sublevel plus procedural tunnel maps at its location sockets (MAPGEN.md §5b). Every level gets the game systems without map changes: `UGJEncounterSubsystem`, the crowd, combat FX and music run in every Game/PIE world, and the player is `Content/GJ/BP_GJPlayer` / `BP_GJPlayerController`.
- Console variables for testing:
  - `agent.Model <run id>` (before Play), `agent.Mobility static|walk|full` (live), `agent.Paused`, `agent.Debug`, `agent.ShowBean`, `agent.ShowHitboxes`, `agent.Tracers`
  - `gj.Enemies N`, `gj.SpawnRadius <cm>`, `gj.SpawnInterval <s>`, `gj.KillEnemies`, `gj.Tracers`
  - `gj.Map <graph|wfc|tunnels> <level> [seed]` (rebuild every procedural map in the level), `gj.MapInfo`, `gj.MapCheck`, `gj.MapSelfTest [delay] [quit]` (in-game map test; see MAPGEN.md §7)
  - `gj.Music 0|1`, `gj.MusicVolume`, `gj.MusicTier -1|0|1|2`, `gj.MusicInfo`
  - `gj.Play` (leave the main menu; works in `-ExecCmds`), `gj.SkipIntro`, `gj.Pause`, `gj.Restart`, `gj.MainMenu`, `gj.KillPlayer`, `gj.RunInfo`, `gj.DelayedExec <s> <cmd>` (scripted tests and screenshots). `-nomenu` skips the front end.
- Headless runs normally use `-nosound`, which turns the music off. To exercise it with no speakers, drop `-nosound` and set `SDL_AUDIODRIVER=dummy`.

## Architecture

**`Source/AgentRuntime/`** hosts the policy:
- `Public/FPSAgent/` is **vendored** from FPS-Agent at a pinned commit (see VENDORED.md + `VENDORED.sha256`). The observation/action schema code (`Learning/CombatObservation.*`, the `Specify*`/`Gather*`/`Perform*` bodies of `UCombatInteractor`) must stay **byte-identical**. Only the six listed local changes are allowed, each marked `GameJam` in the code. Vendored files include each other relative to `Public/FPSAgent/` (e.g. `"Common/FPSAgentTypes.h"`); keep those includes verbatim.
- `AAgentDirector` (one per world, `AAgentDirector::Get`) is the game's own contract implementation: snapshot loading with size/hash checks that refuse mismatches, a 15 Hz batched-inference decision clock on game time, per-frame held Aim/Move, perception (view cone + LOS, 2 s memory), hearing, hit indicator, virtual episodes/time-left (§12), ally invisibility, and **stage + mobility gating**. All gating belongs here, never in vendored files.
- `AEnemyBean` is the vendored `ABeanCharacter` plus a cosmetic skin, the `AgentPawn` capsule channel, AI-controller possession, and ragdoll-on-death. Gameplay mechanics are untouched.
- `UAgentTargetComponent` (on the player) is **everything agents may know about the player**: head/chest points, velocity, facing, crouch, alive, hitboxes, footstep/gunshot sound events, and cloak hooks (`bHiddenFromAgents`, `bSilentToAgents`).
- `UAgentRuntimeSettings` (Project Settings > Game > Agent Runtime) is the **model registry**: run id, stage, mobility, time limit T, snapshot dir, `-Set` string, SHA-256s, and `ActiveRunId`. Defaults are in `Private/AgentRuntimeSettings.cpp`. Snapshots live in `Content/AgentModels/run-<id>/` (`encoder.bin`, `policy.bin`, `decoder.bin`, `RUN_REPORT.md`, `SHA256SUMS`).

**`Source/GameJam/`** is the UE First Person template (base classes plus `Variant_Shooter` / `Variant_Horror`). The game's own code is in `GJ/`:
- `AGJPlayerCharacter` (extends `AShooterCharacter`): bean-identical capsule and hitboxes, the bean's movement numbers, walk and crouch keys (silent to agents), armor and regen.
- `AGJRifle`: mirrors the enemy rifle using the vendored `FRifleFireState`/`CombatMath`, hitscan on the Weapon channel.
- `AGJHUD`: canvas HUD.
- `UGJEncounterSubsystem`: a placeholder Encounter Director that applies the contract's spawn rules. On a procedural map it probes tiles by walking distance from the player. On a hand-built level it uses the level's `EnemySpawn`-tagged markers (the club places them in every room) plus the floor tiles of any socket maps (the tunnels in `Lvl_Club`). Its floor probes query level geometry only (WorldStatic/WorldDynamic), so they pass through ceilings.
- Presentation (Project Settings > Game > GameJam): `AGJDancer` + `UGJCrowdSubsystem` (club crowd around `CrowdAnchorClass`, the club's mirror ball; flees gunshots), `GJCombatFX` + `UGJCombatFXSubsystem` (muzzle flash, shells, tracers, bullet holes, blood), `UGJViewModifier` (weapon feel). Enemy look, gun and body animation are in Agent Runtime > Enemy Look (`ABP_TP_Rifle` has an upper-body slot for them).
- `UGJRunSubsystem` + `UGJFrontEndSettings` (Project Settings > Game > GJ Front End): the run state (Menu, Intro, Playing, Dead), the Slate menus (title, pause, results), the intro camera and the run's stats, in levels with a `GJMenuOrbit` marker (`Lvl_Club`). There, death ends the run (no respawn) and Restart reloads the level. Gameplay checks `UGJRunSubsystem::IsPlaying()` (no spawning outside the run) and `HidesGameHud()`. Player settings and records are `FGJUserSettings`.
- `UGJMusicSubsystem` + `UGJMusicSettings` (Project Settings > Game > GJ Music): adaptive music. Looping stems are locked to one Quartz clock and faded by intensity tier (Prowl / Hunt / Fight). It is presentation only, so agents never hear it. The stems come from `Tools/Music/clubsynth` (plain C++17).

`Source/GameJam/MapGen/` is the procedural map generator (MAPGEN.md):
- `GJLayoutGen.*` (namespace `GJMap`) is **plain C++17 with no Unreal types or exceptions**, so `Tools/MapGen` can build it with g++. Keep it that way. All randomness goes through `GJMap::FRng`, never `std::` distributions, so a `(family, level, seed)` tuple gives the same map everywhere.
- `AGJMapGenerator` turns a layout into merged `BlockAll` box colliders plus collision-free instanced meshes, replaces the level's PlayerStarts with one at the layout start, and hands spawn candidates to the encounter subsystem. Its location is the map's south-west corner; row 0 is north (+X), column 0 is west. With `Socket` set it is a **socket map** instead: a `tunnels` layout that hides a hand-built level's gate at that `LocationSocket`, moves itself so its mouth continues the tunnel, keeps clear of the level, and leaves the player start alone (MAPGEN.md §5b).

Custom collision channels (`Config/DefaultEngine.ini`): `GameTraceChannel1` = Projectile (template), `GameTraceChannel2` = **Weapon** (`ECC_Weapon`), `GameTraceChannel3` = **AgentPawn**, `GameTraceChannel4` = **Ceiling** (`ECC_Ceiling`, object type for ceilings and roofs: blocks pawns and Weapon, invisible to agent perception, which queries WorldStatic/WorldDynamic only). The club map uses the same slots.

## Rules that are easy to break

- **Never change the agent interface to fit a game feature.** No changes to observations, actions, enemy body, speed, rifle, health or senses. Such features are deferred or sent back to FPS-Agent for retraining (contract §15). FPS-Agent is read-only for this project; never modify, build or run it, and requests go through the user.
- **Mobility:** the game's enemies use the **`walk`** line (no crouch/jump). A `full` snapshot with crouch and jump masked is *not* a stand-in. The intended game snapshot is `run-S1-walk-20261114-152929`, but it **isn't copied in yet**: the registry currently holds S0 (`static`) and `S1-20261113-L009` (`full`, active).
- **Stage gating:** S0 runs as a turret (no movement, no sounds, no Hit). S1 hears footsteps only. Gunshots and Hit are fed only from S2.
- Presentation (dancers, ragdolls, clutter, VFX, music, darkness) must not enter agent geometry/LOS queries or block the Weapon or AgentPawn channels. Ragdolls use `PhysicsBody` and ignore Weapon. "Hide in the dark/smoke" must be an explicit not-visible test in the Agent Director, never weakened LOS.
- Player speed stays within the trained envelope (run about 540, ceiling 600 cm/s). Use global time dilation only, never per-actor time dilation on agents.
- Levels: 4 m grid, corridors at least 4 m wide, a single walkable floor per XY location, cover at 120/250 cm, ramps instead of jumpable steps (PLAN §8, map format v1 in §8.1).

## Git

- `*.uasset` and `*.umap` go through **Git LFS**. `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `GameJam/Plugins/UnrealMCP/` and `/.mcp.json` are ignored.
- Stage files **by name**. Avoid `git add -A` / `git add .` (it once began staging `Intermediate/`).
- A stale `.git/index.lock` can be deleted only when no git process is running.
